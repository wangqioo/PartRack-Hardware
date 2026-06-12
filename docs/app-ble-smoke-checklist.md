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
- [ ] 可用 BLE 后端环境中再运行真实 `--run-smoke`。
- [ ] macOS CoreBluetooth 后端不可用时，不能把脚本单测当成真实 BLE 通过。
