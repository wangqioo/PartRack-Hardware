# APP BLE 烟测清单

## nRF Connect 手工烟测

- [ ] 扫描到 `VBRK-0000`。
- [ ] 连接成功。
- [ ] 发现 Binding Table Service。
- [ ] 发现 Light Control Service。
- [ ] 读取 Table Info。
- [ ] 开启 Binding Control Point notify。
- [ ] 写入 `WRITE_ONE` 测试帧。
- [ ] 发送 `READ_ONE` 并确认返回记录。
- [ ] 发送 `READ_ALL` 并确认结束帧 `02 00 FF`。
- [ ] 发送 Light Command 并读取 Light Status。

## Android APP 烟测

- [ ] Android 12+ 权限申请完整。
- [ ] 扫描回调能解析 manufacturer data。
- [ ] 连接后能 discover services。
- [ ] 遇到加密写失败能触发配对或重试流程。
- [ ] `WRITE_ONE -> READ_ONE` 闭环通过。
- [ ] `READ_ALL` 超时和结束帧处理正确。
- [ ] Light Command 后能用 Light Status 确认。

## 电脑脚本烟测

- [ ] `python3 tools/ble_gatt_smoke_test.py --print-vectors` 可输出测试帧。
- [x] 2026-06-16 Mac CoreBluetooth/Bleak + XIAO 实机运行真实 `--run-smoke` 通过，覆盖 `WRITE_ONE -> READ_ONE`、`READ_ALL` 结束帧和 `SET_QTY`。
- [x] 2026-06-16 `--run-batch` 通过，覆盖 `SET_QTY` 后读回、Table Info seq/CRC 和 Light Status FIND/OFF。
- [x] 2026-06-16 单击 reset 后 `--run-persistence-read` 通过，slot 1 读回 `C1234567 / qty=42`。
- [ ] 后续在 Android APP 侧复验同一流程。
- [ ] macOS CoreBluetooth 后端不可用时，不能把脚本单测当成真实 BLE 通过。
