# HID Pad Service 1.1.0 / Module 1.1.50

## 本次更新

- Web 页面拆为 `package/main.html`，支持简中、繁中、英文、日文；默认读取
  `/sd/settings.json`，不存在有效语言字段时回退 `/sd/apps/settings.json`。
  接受 `language`、`locale`、`lang` 字段。右上角语言选择仅保存在浏览器。
- `config.json` 新增默认开启的 `auto_connect`。关闭后仍可手动连接；Q34/Q36
  的一次无输入恢复独立于此开关。手动断开暂停重连，意外掉线才按策略重连。
- 扫描使用独立状态，不覆盖配对、发现或订阅阶段；只有输入初始化完成才 `ready`。
- C 命令返回 `true, command_id`；Web/IPC 状态提供 `command_id`、`command_kind`、
  `command_status`、`command_error`。`pending` 表示已接收，`succeeded`/`failed`
  才是完成结果。首选设备在连接就绪时保存，忘记设备在执行成功后保存。
- 报告十六进制及详细统计移至 `GET /hidpad/api/diagnostics`（或模块 `diagnostics()`），
  普通输入更新不再格式化或编码这些数据。重新连接成功会清除上次连接错误；SD
  配置写入错误单独保留。
- 未连接且未扫描时 worker 最长等待 1000ms；命令通过信号量立即唤醒。输入就绪
  的轮询间隔仍为 10ms，扫描为 20ms，连接初始化为 50ms。
- S3 编译器测得实例热状态 4576 → 3600 字节，冷状态 1764 → 2024 字节。
  热状态少 976 字节，冷状态在 PSRAM；6KB worker 栈不变。这是分配结构大小，
  不代表设备启动后实际剩余堆大小。

部署需同时更新 `main.lua`、`main.html`、`modules/hidpad.so` 及应用元数据；保留
设备现有 `config.json`。本地预览运行 `node tools/preview-server.cjs`，仅提供模拟数据。

原生 S3 模块构建脚本为 `tools/build-s3-module.ps1`，参数 `-Compiler` 指向
`xtensa-esp32s3-elf-gcc.exe`，`-ModuleAbiDir` 指向宿主 `module_abi.h` 所在目录。
只构建动态模块，不编译宿主固件。

定向检查：`src/tests/hidpad_state_test.c` 使用模拟 BLE 事件检查真实 C 状态机；
`src/tests/service_state_test.py` 使用 Lua 5.4（lupa）检查持久化与错误恢复；
`node src/tests/web_state_test.cjs` 检查页面异步结果及旧错误清除。

`hidpad` 是 Cubic Lua 的常驻 BLE 手柄服务。`hidpad.so` 负责扫描、连接、配对、
GATT 发现、CCCD 订阅和输入报告解码；`main.lua` 负责校准、按键映射、Web/IPC
管理，并把标准化状态发布到 `controller` source `ble-main`。

当前包含 Xbox BLE、Q34/Q34U、Q36/ShanWan、标准 BLE HID、BTP BFM 和飞智
BLE HID/Android 智连模式的兼容链路。

## 基本原则

- 所有手柄由 `.so` 独占一个 BLE session，连接后使用加密配对。
- CCCD 始终通过 descriptor discovery 查找 `0x2902`，不猜测 `value_handle + 1`。
- 手动扫描只更新设备列表；用户选中设备后保存其地址，以后只自动重连该设备。
- 驱动把所有协议统一为按键位图、双摇杆和 LT/RT，Lua 层再应用死区、校准和映射。

## 兼容总览

| 手柄 | 识别条件 | 连接和订阅 | 解码 |
| --- | --- | --- | --- |
| Xbox BLE | 名称包含 `Xbox`，或 Microsoft company/appearance | 发现 `0x1812`，只订阅第一个可 Notify 的 `0x2A4D` | Xbox 16 字节基础报告 |
| Q34/Q34U | 名称包含 `Q34` 或 `ShanWan` | 订阅所有 Input Report CCCD，可 READ 轮询兜底 | Report Map 或 Q34U 10 字节报告 |
| Q36 | 名称包含 `Q36` 或 `ShanWan` | 与 Q34 相同，必要时仅做一次加密重连 | Q36 Report Map 语义或 ShanWan 10 字节报告 |
| 飞智 BLE HID/智连 | 名称包含 `Flydigi` | HOGP 后枚举非标准服务，订阅全部 Notify/Indicate，通过 NUS RX 初始化 | 智连 14/20 字节报告，并兼容飞智 V2 |
| 其它标准 BLE HID | 广播 `0x1812` 并具有手柄名称/appearance | 标准 HOGP，订阅所有 Input Report | 按 Report Map 通用解码 |

## Xbox BLE

Xbox 使用独立 `xbox` profile。完成连接和配对后，驱动发现 HID Service
`0x1812`、Report `0x2A4D` 及其 CCCD，只选择第一个具有 Notify 属性的
controls report，写入 `0x0001`。Xbox 不使用通用 HID 的 Input READ 轮询、
Control Point 保活或 Q34/Q36 无通知重连。

普通 Xbox BLE 主报告为 16 字节。Xbox Elite Wireless Controller Series 2 的前
16 字节具有相同布局，驱动忽略后续 Elite 元数据，因此支持主要按键、摇杆和
LT/RT，不支持拨片、Profile 元数据和震动输出。第一代 Xbox Elite 没有
Bluetooth/BLE，无法由本驱动连接。

## Q34 / Q34U

Q34 通过广播名识别，即使广播没有 `0x1812` 也会尝试连接。配对后依次用
16-bit `1812` 和完整 Bluetooth Base UUID
`00001812-0000-1000-8000-00805f9b34fb` 发现 HID Service。然后读取
Report Map `0x2A4B`、Report Reference `0x2908`，并给每个带 Notify/Indicate 的
Input Report `0x2A4D` 写入实际发现的 CCCD。

`GamepadSpace-Q34U` 及部分 ShanWan 模式提供键盘型 Report Map，但实际手柄输入
是 10 字节定长报告。专用解码将前 4 字节解析为双摇杆，第 8、9 字节解析为
LT/RT。具有 READ 属性的 Input Report 在收到该路首条 Notify 前会被低频读取；
第一次 READ 只建立缓存基线，之后的变化才作为输入。

## Q36

Q36 与 Q34 共用 `q36-hid` profile 及上述连接/订阅链路。标准输入以
Report Map 决定字段位置、位宽和 logical range，并保留 Q36 语义：

- Hat Switch 声明为 `1..8` 时，分别是上、右上、右、右下、下、左下、左、左上；
  声明为 `0..7` 时改为 `0=上、7=左上`；
- Button Usage `1/2/4/5` 映射 `A/B/X/Y`，`7/8` 映射 `LB/RB`；
- Button Usage `9/10` 映射数字 `LT/RT`，`11/12/13/14` 映射
  `View/Menu/Home/Share`；
- `X/Y/Z/Rx/Ry/Rz` 按 Report Map 范围归一化，Report ID `3` 的 Consumer Control
  与普通手柄状态合并。

`Q36 for Android`/ShanWan 的键盘型 Map 同样可使用 10 字节专用解码。
Q34/Q36 如果 CCCD 写入成功却始终没有任何 Input Report，会最多做一次完整的
加密链路重连；不会因为手柄暂时静止就反复切换 CCCD。

## 飞智 BLE HID / Android 智连

这条链路面向飞智 HID/安卓智连模式，不依赖 XInput。已验证的 APEX 5 在
XInput 模式下本来就不提供 BLE。智连模式的标准 `0x1812` Report Map 可能只描述
Touch Screen Digitizer；驱动不会把触摸坐标解析成摇杆，而是继续发现厂商服务。

厂商服务发现依次尝试旧飞智服务 `0x1204`、Nordic UART Service
`6e400001-b5a3-f393-e0a9-e50e24dcca9e`，最后枚举全部非标准 Primary Service。
对每个服务，驱动发现所有带 Notify/Indicate 的特征和它们的 CCCD，并串行订阅。
APEX 5 实测会订阅：

- NUS TX `6e400003-b5a3-f393-e0a9-e50e24dcca9e`；
- Telink/OTA 服务 `00010203-0405-0607-0809-0a0b0c0d1912` 下的特征
  `00010203-0405-0607-0809-0a0b0c0d2b12`。

写通道优先使用 NUS RX `6e400002-b5a3-f393-e0a9-e50e24dcca9e`。初始化序列为：

1. 写 `BA C0 00 00`，等待 `AC C0` 设备信息响应；
2. 写 `A5 01 00 00`，查询 switch-chip 版本；
3. 写 `A5 A0 00 00`，等待 `A5 A0` UUID 响应；
4. 写 20 字节 GATT-only 命令 `41 02 00 ... 00 43`，开启物理按键原始通知。

智连输入支持 14 字节旧包和以 `FE 00` 结尾的 20 字节包：字节 `0..3`
是双摇杆，`4..5` 是按键位图，`6..7` 是 LT/RT，Home 位于字节 8 的
bit 3。同时保留以 `5A A5 EF` 开头的飞智 V2 USB/接收器报告解码。

其它飞智型号如果复用上述 NUS 握手和智连报告，可以直接兼容；如果服务 UUID、
初始化命令或输入帧不同，则需要继续按型号添加协议适配。

## 通用 BLE HID 和 BTP BFM

通用 `hid` profile 在配对后交换 MTU，将 Protocol Mode `0x2A4E` 设为
Report Protocol，再发现 Report Map `0x2A4B`、Input Report `0x2A4D`、
Report Reference `0x2908` 和 CCCD。GATT Read Long 可分片读取最多 512 字节的
Report Map，通用解码支持方向帽、独立 D-pad、按键、双摇杆和扣机。

如发现 HID Control Point `0x2A4C`，就绪时只发一次 Exit Suspend（值 `1`）；
否则低频读取可读 Input Report 作为不主动通知设备的兜底。通用 HID 每个 CCCD
只写一次，不会把“手柄静止”视为订阅失败。

`BTP-KP20D ... BFM` 实际使用标准 Report ID `3` 的 10 字节通知，按键 Usage
采用 Q34/Q36 语义：`1/2/4/5=A/B/X/Y`、`7/8=LB/RB`、
`9/10=LT/RT`、`11/12=View/Menu`、`13=Home`。L3/R3 在该模式下不单独映射。
它的 GATT Read 值是全零旧缓存，不能作为按键兜底，因此订阅成功后不轮询读取，
也不强制覆盖连接参数。KP20D 还要求北通 BFM 的 `7310` 厂商会话：订阅
`7311/7313`（与官方 App 一样，不依赖特征声明中的 Notify 位），向 `7312`
先写入 `21 00 00` 打开会话，再依次写入 `15`、`55`，之后每秒写入
`21 00 00`。连续约 3 秒收不到厂商应答时，先写 `11 00 20` 再恢复心跳。
发现并订阅厂商特征后还会按官方流程读取一次 `7311`，用于启动其输入通道。
缺少这条会话时，标准 HID 通知会在约 20 秒后停止，即使 BLE 链路仍显示在线。
该流程仅对 BTP/BETOP 的 BFM 名称启用，不根据普通 HID 通知静默重订阅或主动断线；
其它通用 HID 手柄仍只在就绪时写一次 HID Exit Suspend。

## 运行和接口

1. `autostart_service=true` 由应用管理器常驻拉起 Service。
2. Lua 从 `/sd/apps/hidpad/modules/hidpad.so` 加载驱动并调用 `start(8000)`。
3. `.so` 扫描并连接首选设备，完成对应 profile 的 GATT 初始化。
4. 原始报告解码后，Lua 应用校准和映射，再发布到 `ble-main`。
5. RetroGo 通过 `controller.state("ble-main")` 或 `controller.on("ble-main", ...)` 获取输入。

WebUI 路由默认为 `/hidpad/`，提供连接状态、设备选择、实时输入、摇杆/扣机校准、
死区和 16 个标准按键的映射。主要路由是：

- `GET /hidpad/api/state`：完整状态和配置；
- `GET /hidpad/api/input`：精简实时输入；
- `GET /hidpad/api/devices`：扫描设备列表；
- `POST /hidpad/api/command`：启用、扫描、连接、断开、配对、校准和映射命令。

IPC endpoint 为 `ble-controller`，支持 `status`、`enable`、`disable`、`rescan`、
`scan_devices`、`connect_device`、`disconnect`、`pair`、`calibration_start`、
`calibration_save`、`calibration_cancel`、`set_mapping`、`set_config` 和
`restore_defaults`。配置保存到 `/sd/apps/hidpad/config.json`。

## 任务和内存

新固件提供 `runtime.event_post` 时，`.so` 创建绑定 CPU0、使用 6KB PSRAM 栈的
`hidpad_worker`，由它串行处理 BLE 事件。热路径状态优先使用内部 RAM，扫描、
Report Map 和发现缓冲位于冷状态/PSRAM。Lua 状态读取通过模块 mutex 与 worker 隔离。

驱动跳过完全重复的短报告，并将多 Report ID 的按键、Consumer Control、摇杆和扣机
按有效字段合并。公开状态没有变化时，不会重复调用 `controller.publish`。

## 构建和部署

只构建动态模块，不需要重新编译 Cubic Lua 固件：

```powershell
$pio = Join-Path $env:USERPROFILE ".platformio"
$env:IDF_PATH = Join-Path $pio "packages\framework-espidf"
$env:PATH = (Join-Path $pio "packages\tool-cmake\bin") + ";" +
            (Join-Path $pio "packages\tool-ninja") + ";" +
            (Join-Path $pio "packages\toolchain-xtensa-esp-elf\bin") + ";" +
            (Join-Path $pio "penv\Scripts") + ";" + $env:PATH

cmake --build E:\cubicsrc\APPS\hidpad_build --target so
```

产物会复制到 `package/modules/hidpad.so`。部署时将 `package/` 的内容上传到
`/sd/apps/hidpad/`，不要把 `src/` 部署到设备。

Parser 回归测试位于 `src/tests/hid_report_parser_test.c`，覆盖通用 Report Map、长
Report Map、Digitizer 排除、BTP 报告和独立 D-pad Usage。
