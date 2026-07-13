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
- Q36 / Android 手柄：发现 HID Service `0x1812`、Report Map `0x2A4B`、Input
  Report `0x2A4D`、Report Reference `0x2908` 和 CCCD `0x2902`，使用旧版 Q36
  按钮规则。
- 其他 BLE HID 手柄：根据 HID Report Map 解析方向帽、按钮、双摇杆和扳机。

通知订阅通过 descriptor discovery 查找 CCCD，不使用 `value_handle + 1` 猜测。
没有 notify/indicate 的可读 Input Report 会由驱动低频轮询。

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

`hidpad.so` 不创建 FreeRTOS 任务。Lua 每 20ms 调用一次 `poll()`，驱动只消费固件
NimBLE 回调复制到固定队列的事件。为避免占用 Service 的 `lua_update` C 调用栈：

- `module_ble_event_t`、BLE config/scan config、HID decoded report 和广播解析缓冲都放在
  模块实例中；实例由 host heap 分配到 PSRAM；
- HID Report Map parser 的 global/local state、push stack 和 report offset 表使用模块
  静态工作区；
- 构建启用 `-fconserve-stack`，并用 `-Wframe-larger-than=256` 阻止以后重新引入较大
  栈帧。

这些缓冲均为单 BLE owner 串行复用，不增加后台任务或并发锁。

Lua 只在 `.so` 返回 dirty state 时执行映射；标准化输出没有变化时不再调用
`controller.publish`。禁用蓝牙手柄后会停止 20ms timer 并关闭 BLE session，Service
本身仍常驻以保留 IPC 和 Web 管理能力。

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
每 400ms 读取一次数字状态。设备列表仅在手动扫描期间读取，避免持续编码配置和扫描
结果。驱动扫描结果使用模块实例中的 8 项定长 PSRAM 数组，不创建任务或事件表。

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

部署整个 `package/` 到 `/sd/apps/hidpad/`。本次实现没有执行设备上传或烧录。
