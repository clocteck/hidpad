# HID Pad

Version / 版本：**1.1.0**

[中文](#中文) · [English](#english)

## 中文

Holo / Cubic Lua 的常驻 BLE 手柄服务。连接一个手柄，将按键、双摇杆和扳机统一发布到 `controller` 的 `ble-main` 输入源，供游戏等应用使用。

网页 `/hidpad/` 提供扫描、连接、默认开启的自动连接、校准、死区和按键映射，支持简中、繁中、英文、日文。语言默认读取 `/sd/settings.json`，再回退到 `/sd/apps/settings.json` 的 `language` / `locale` / `lang` 字段，也可在页面右上角选择。

### 项目架构

```text
BLE 手柄 → hidpad.so → main.lua → controller / ble-main → 应用
                            ↕
                        Web / IPC
```

| 文件 | 职责 |
| --- | --- |
| [hidpad_module.c](src/main/hidpad_module.c) | BLE 扫描、配对、GATT 发现、订阅、连接状态机和厂商协议 |
| [hid_report_parser.c](src/main/hid_report_parser.c) | HID Report Map 解析与标准输入解码 |
| [main.lua](package/main.lua) | 配置、校准、映射、输入发布、Web API 和 `ble-controller` IPC |
| [main.html](package/main.html) / [info.html](package/info.html) | 控制页面 / 使用说明 |
| [tests](src/tests) | 输入解析、状态机及 Web/Lua 定向检查 |

支持事件回调的固件由独立 worker 串行处理 BLE；Web 命令区分已接收与执行完成。高频输入与低频诊断分开，详细诊断位于 `/hidpad/api/diagnostics`。

### 已有手柄协议支持

| 手柄 / 模式 | 当前适配 |
| --- | --- |
| Xbox BLE | 主要按键、双摇杆和 LT/RT；含 Elite 2 基础输入，不含拨片、震动 |
| Q34 / Q34U / Q36 | HID 描述符、部分 ShanWan 10 字节报告及有限连接恢复 |
| 飞智 BLE HID / 安卓智连 | 含八爪鱼 5 的厂商初始化与输入报告适配 |
| 北通 BFM | 含 KP20D 的 HID 输入、厂商会话和保活适配 |
| 通用 BLE HID 手柄 | 根据 Report Map 解析方向键、按键、摇杆和扳机 |

这是已实现的协议范围，不代表同品牌所有型号均已验证。当前服务不接入经典蓝牙 HID、USB 或 2.4G 接收器。**Q37 Pro D 档、GameSir Cyclone 2 尚未确认兼容**；扫描识别到名称不等于输入协议兼容。

### 如何增加手柄

1. **确认连接通路。**记录型号、固件版本、档位和广播名称，确认该模式提供 BLE。收集 GATT 服务、特征、CCCD、Report Map 和实际按键/摇杆输入报告。
2. **优先复用标准解析。**在 `hidpad_module.c` 的名称识别、`score_advertisement()` 和 profile 选择中加入有依据的条件；标准 HID 字段问题在 `hid_report_parser.c` 处理。不要仅凭品牌名强制套用 Xbox 或 Q36 协议。
3. **按需增加专用协议。**私有握手、特征订阅和报告解码放在 `hidpad_module.c`，仅对确认的型号/模式启用。CCCD 必须实际发现；复用现有 worker、连接状态机和标准输入结构，避免重复实现自动重连。
4. **检查并记录兼容性。**补充真实报告样本的解析用例，检查按键、摇杆、扳机、重复连接、手动断开、意外掉线及自动连接开关。成功后在支持表中注明型号、模式和限制。

### 构建与部署

模块依赖宿主提供的 `module_abi.h`。原生 S3 构建使用 [build-s3-module.ps1](tools/build-s3-module.ps1)，传入 `-Compiler`（`xtensa-esp32s3-elf-gcc.exe` 完整路径）与 `-ModuleAbiDir`（ABI 头文件目录）。脚本只构建模块，并更新 `package/modules/hidpad.so`。不同芯片需要使用匹配的工具链和宿主 ABI，不能共用预编译 `.so`。

将 `package/` 内容部署到 `/sd/apps/hidpad/`，保留设备上的 `config.json`，更新后重启 HID Pad 服务。运行包包括 `app.info`、`main.lua`、`main.html`、`info.html` 和 `modules/hidpad.so`。

## English

A persistent BLE controller service for Holo / Cubic Lua. It connects one controller and publishes normalized buttons, dual sticks and triggers to the `controller` source `ble-main` for games and other apps.

The `/hidpad/` page provides scanning, connection, auto-connect (on by default), calibration, deadzone and button mapping. It supports Simplified Chinese, Traditional Chinese, English and Japanese. The default language comes from `language` / `locale` / `lang` in `/sd/settings.json`, falling back to `/sd/apps/settings.json`. A page-level language selector is also available.

### Architecture

```text
BLE controller → hidpad.so → main.lua → controller / ble-main → apps
                                ↕
                            Web / IPC
```

| File | Responsibility |
| --- | --- |
| [hidpad_module.c](src/main/hidpad_module.c) | BLE discovery, pairing, GATT subscriptions, connection state machine and vendor protocols |
| [hid_report_parser.c](src/main/hid_report_parser.c) | HID Report Map parsing and standard input decoding |
| [main.lua](package/main.lua) | Settings, calibration, mapping, input publication, Web API and `ble-controller` IPC |
| [main.html](package/main.html) / [info.html](package/info.html) | Control page / user instructions |
| [tests](src/tests) | Focused parser, state-machine and Web/Lua checks |

On firmware with event callbacks, a dedicated worker serializes BLE operations. Web commands distinguish acceptance from completion. Fast input updates are separate from on-demand diagnostics at `/hidpad/api/diagnostics`.

### Implemented controller support

| Controller / mode | Implementation |
| --- | --- |
| Xbox BLE | Main buttons, sticks and LT/RT; includes Elite 2 basic input, without paddles or rumble |
| Q34 / Q34U / Q36 | HID descriptors, selected ShanWan 10-byte reports and bounded connection recovery |
| Flydigi BLE HID / Android smart mode | Includes APEX 5 vendor initialization and input reports |
| BTP / BETOP BFM | Includes KP20D HID input, vendor session and keepalive handling |
| Generic BLE HID controllers | Report Map-based D-pad, button, stick and trigger decoding |

This describes implemented protocols, not verified support for every model from a brand. The service does not connect through Classic Bluetooth HID, USB or 2.4 GHz receivers. **Q37 Pro D mode and GameSir Cyclone 2 remain unverified.** Recognizing a device name does not establish input compatibility.

### Adding a controller

1. **Identify the transport.** Record the model, firmware, mode and advertised name. Confirm BLE is available in that mode. Capture GATT services, characteristics, CCCDs, the Report Map and real button/stick reports.
2. **Reuse standard decoding first.** Add evidence-based detection to the name checks, `score_advertisement()` and profile selection in `hidpad_module.c`. Handle standard HID field issues in `hid_report_parser.c`. Do not assign the Xbox or Q36 profile solely from a brand name.
3. **Add vendor handling only when required.** Keep handshakes, subscriptions and custom decoding in `hidpad_module.c`, scoped to the verified model/mode. Discover actual CCCDs and reuse the worker, connection state machine and normalized input structure instead of adding another reconnect loop.
4. **Test and document.** Add parser cases using captured reports. Check buttons, sticks, triggers, repeated connections, manual disconnect, unexpected link loss and auto-connect settings. Document the tested model, mode and limitations in the support table.

### Build and deploy

The module requires the host's `module_abi.h`. For native S3 builds, run [build-s3-module.ps1](tools/build-s3-module.ps1) with `-Compiler` (the full path to `xtensa-esp32s3-elf-gcc.exe`) and `-ModuleAbiDir` (the ABI header directory). It builds only the module and updates `package/modules/hidpad.so`. Other chip targets require their matching toolchain and host ABI; prebuilt `.so` files are not interchangeable.

Deploy the contents of `package/` to `/sd/apps/hidpad/`, preserve the device's `config.json`, and restart the HID Pad service. The runtime package contains `app.info`, `main.lua`, `main.html`, `info.html` and `modules/hidpad.so`.
