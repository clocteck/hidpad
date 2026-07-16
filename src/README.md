# HID Pad Service

`hidpad` 是 Cubic Lua 的常驻 BLE 手柄服务。BLE 扫描、连接、配对、GATT 发现和 HID
报告解析位于 `hidpad.so`；Lua 只负责校准、按键映射、Web 设置和向
`controller` 总线发布标准化状态。

该版本要求固件提供 BLE dynmod ABI、`controller.publish` 和 `ipc.listen`，不兼容旧
固件。能力缺失时，Service 会在串口输出：

```text
[hidpad] 固件版本不支持：需要 BLE ABI、controller.publish 和 ipc.listen
```

## 支持的手柄

- Xbox Wireless Controller：沿用旧 Gamepad 的 16 字节 Xbox Input Report 解码。
- BLE HID 手柄：广播中发现 HID Service `0x1812` 后，优先使用 Q36 profile；连接后
  发现 Report Map `0x2A4B`、Input Report `0x2A4D`、Report Reference `0x2908` 和
  CCCD `0x2902`，根据 HID Report Map 解析方向帽、按钮、双摇杆和扳机。
- 名称包含 `Q36` 或 `ShanWan` 的设备，即使广播包没有携带 `0x1812`，也使用 Q36
  profile 尝试连接。

## Q36 profile

Q36 profile 是当前对 Android BLE HID 手柄的优先兼容规则。Xbox 会先被识别并走独立的
16 字节报告解码；除此之外，所有广播了 HID Service `0x1812` 的手柄都先按 Q36
profile 处理。WebUI 和状态接口中显示为 `q36-hid`。

Q36 profile 仍以手柄提供的 HID Report Map 决定字段位置、位宽和轴范围，但采用以下
按钮语义：

- Hat Switch：`0` 表示松开，`1`～`8` 依次表示上、右上、右、右下、下、左下、左、左上；
- Button Usage `1/2/4/5`：分别映射为 `A/B/X/Y`；
- Button Usage `7/8`：分别映射为 `LB/RB`；
- Button Usage `9/10`：作为数字 `LT/RT`，按下值为满量程；
- Button Usage `11/12/13/14`：分别映射为 `View/Menu/Home/Share`；
- `X/Y/Z/Rx/Ry/Rz` 等轴和扳机字段按 Report Map 中的 logical range 归一化。

方向帽保持旧 Q36 固件的行为：Report Map 声明 logical range 为 `1..8` 时按
`1=上、8=左上` 解释，声明为 `0..7` 时按 `0=上、7=左上` 解释；范围外的值表示松开。

连接链路也保持旧 Q36 行为：先用 16-bit `1812` 查找 HID Service，找不到时改用完整
Bluetooth Base UUID `00001812-0000-1000-8000-00805f9b34fb`，整套初始化最多执行两次。
Q36 必须成功读取并解析 Report Map；随后只订阅带 notify/indicate 的 Input Report，
Report ID `3` 继续作为 Consumer Report 与普通手柄状态合并。Xbox 则和旧
`LiteXboxController` 一样，只订阅第一个支持 notify 的 `0x2A4D` controls report。

通知订阅通过 descriptor discovery 查找 CCCD，不使用 `value_handle + 1` 猜测。
通用 HID profile 中，没有 notify/indicate 的可读 Input Report 会由驱动低频轮询；
Q36 和 Xbox 保持旧固件的纯通知链路，不额外轮询 Input Report。
驱动发现 HID Control Point `0x2A4C` 后，每 15 秒发送一次标准 Exit Suspend 命令；没有
Control Point 时低频读取可读 Input Report，避免部分通用 HID 手柄在无按键时进入应用层
休眠。Q36 和 Xbox 不启用这条额外保活链路。

## 运行过程

1. `autostart_service=true` 让应用管理器常驻拉起 Service。
2. Lua 从 `/sd/apps/hidpad/modules/hidpad.so` 加载驱动并调用 `start(8000)`。
3. `.so` 独占 BLE session，扫描 Xbox、带 `0x1812` 的 HID 或常见手柄名称；配置了
   首选地址时只自动连接该设备。
4. 连接后配对，发现 HID service、characteristic 和 descriptor，订阅 Input Report。
5. `.so` 解析原始报告；Lua 应用校准与映射后发布到 `controller` source
   `ble-main`。
6. RetroGo 使用 `controller.state("ble-main")` / `controller.on("ble-main", ...)`
   获取输入，不再启动或持有 BLE。

## 任务与栈

新固件提供 `runtime.event_post` 时，`hidpad.so` 创建一个绑定 CPU0、6KB PSRAM 栈的
`hidpad_worker`，由它处理扫描、连接、配对、GATT 与 HID 报告；Lua callback 只读取合并状态并
发布 controller/IPC。旧固件不支持该 ABI 时会输出
`runtime.event_post unsupported; please update to latest firmware`，并保持 Lua 每 10ms 调用
`poll()` 的兼容路径。

为减少工作任务的栈峰值：

- `module_ble_event_t`、BLE config/scan config、HID decoded report 和广播解析缓冲都放在
  模块实例中；热路径实例优先使用内部 RAM，冷状态明确放入 PSRAM；
- HID Report Map parser 的 global/local state、push stack 和 report offset 表使用模块
  静态工作区；
- 构建启用 `-fconserve-stack`，并用 `-Wframe-larger-than=256` 阻止以后重新引入较大
  栈帧。

这些缓冲由单 BLE owner 串行复用；Lua 读取状态时通过模块 mutex 与 worker 隔离。

驱动会缓存短报告并跳过完全重复的通知；多 Report ID 的按键、Consumer Control、摇杆和
扳机按有效字段合并，只有公开控制状态确实变化时才返回精简输入状态。Lua 只在 `.so`
返回 dirty state 时执行映射；标准化输出没有变化时不再调用 `controller.publish`。禁用
蓝牙手柄后会停止 poll timer 并关闭 BLE session，Service 本身仍常驻以保留 IPC 和 Web
管理能力。

## Web 和 IPC

WebUI 路由默认为 `/hidpad/`，提供：

- 实时连接状态、手柄名称、驱动 profile、按键、摇杆和扳机；短按由一个整数掩码
  锁存，不会因 Web 低频轮询漏掉；
- 扫描最多 8 个受支持手柄，显示名称、地址与 RSSI，并可选择连接配对；
- 摇杆中心/范围、扳机范围采样校准和中心死区；
- 16 个标准按钮的来源映射、恢复默认设置。

串口按键调试默认关闭；需要诊断时把 `main.lua` 的 `DEBUG_BUTTONS` 设为 `true`。
启用后只在连接状态或按键掩码变化时输出，例如：

```text
[hidpad] pad connected xbox Xbox Wireless Controller AA:BB:CC:DD:EE:FF
[hidpad] buttons raw=0x00000010 A mapped=0x00000020 B
```

其中 `raw` 是驱动标准化后的原始键，`mapped` 是校准页面映射后发布给应用的键。
摇杆变化不会逐报告打印，避免高频刷屏。

页面只在首次加载和执行命令后读取完整配置；实时显示改用精简的 `/api/input`，前台
已连接或扫描时每 400ms 读取一次数字状态，空闲未连接时降为 1500ms，页面隐藏时降为
5000ms。设备列表仅在手动扫描期间读取，并在扫描结束时补读一次，避免持续编码配置和扫描
结果。驱动扫描结果使用模块实例中的 8 项定长数组，不创建任务或事件表。手动扫描保持
高响应扫描参数；自动重连使用 12.5% 扫描窗口和最高 8 秒指数退避。自动或手动扫描发现
名称包含 Xbox、Q36 或 Q34 的设备时会立即连接；成功连接过的首选设备也会按地址自动连接。
服务启动和断线重连都先扫描再连接，保证 Q34/Q36 的 GATT 初始化顺序一致。
手柄已连接时执行手动扫描会保持当前连接，并行更新设备列表，不会切换到扫描到的其他手柄。

IPC endpoint 为 `ble-controller`，topic 支持：

```text
status
enable
disable
rescan
scan_devices
connect_device {"address":"AA:BB:CC:DD:EE:FF"}
disconnect
pair
calibration_start
calibration_save
calibration_cancel
set_mapping
set_config
restore_defaults
```

Settings 发送 `{"reply":"settings-hidpad"}` 时，Service 会向该 endpoint 的 `status`
topic 返回精简 JSON；连接、阶段、名称、地址和按键都使用定长字段，按键为整数位图。

配置保存在 `/sd/apps/hidpad/config.json`。IPC payload 是不超过固件限制的 JSON。

## 构建

只构建本模块，不需要编译 Cubic Lua 固件：

```powershell
$pio = Join-Path $env:USERPROFILE ".platformio"
$env:IDF_PATH = Join-Path $pio "packages\framework-espidf"
$env:PATH = (Join-Path $pio "packages\tool-cmake\bin") + ";" +
            (Join-Path $pio "packages\tool-ninja") + ";" +
            (Join-Path $pio "packages\toolchain-xtensa-esp-elf\bin") + ";" +
            (Join-Path $pio "penv\Scripts") + ";" + $env:PATH
$python = Join-Path $pio "penv\Scripts\python.exe"

cmake -S . -B E:\cubicsrc\APPS\hidpad_build -G Ninja `
  -DIDF_TARGET=esp32s3 `
  "-DPYTHON=$python" `
  -DPYTHON_DEPS_CHECKED=1 `
  -DMODULE_ABI_DIR=E:\cubicsrc\cubic_lua\cubic_arduino\cubic-develop\src\dynmod
cmake --build E:\cubicsrc\APPS\hidpad_build --target so
```

产物：

```text
E:\cubicsrc\APPS\hidpad_build\hidpad.so
E:\cubicsrc\APPS\hidpad\package\modules\hidpad.so
```

部署整个 `package/` 到 `/sd/apps/hidpad/`。
