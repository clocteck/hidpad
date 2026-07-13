# AGENTS.md

`hidpad` 是 BLE HID 手柄 Lua 测试服务。

- `package/` 是设备运行包，部署时把其中内容复制到 `/sd/apps/hidpad/`。
- `src/` 仅放开发说明、协议记录和后续 C/Lua 模块源码，不直接部署。
- 当前实现优先用于验证 Lua BLE 接口能力：扫描、连接、GATT 发现、CCCD 订阅尝试、Input Report 读取/解析、状态发布和串口日志。
- 不要在服务里调用 `app.exit()` 去关闭其它 app。
