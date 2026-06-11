# APP 对接边界

Android APP 主仓库：

```text
https://github.com/Yrd980/LCSC_android_erp.git
```

该仓库已覆盖：

- Kotlin + Jetpack Compose + Material 3。
- Room + SQLite 本地库存。
- 扫码入库和手动入库。
- 库位管理、库存检索、数量修改、转移、删除。
- BOM 导入匹配。
- 蓝牙二维码打印。
- 本地库存 Excel 导入导出。

因此本仓库不实现 APP 功能，只提供 APP 需要对接的硬件契约。

## 本仓库输出给 APP 的内容

- BLE 广播字段。
- GATT 服务、特征和操作码。
- 槽位记录 16B 格式。
- 灯控 17B 帧格式。
- NFC NDEF URI 格式。
- 固件行为约束和错误码。
- 模拟器或测试夹具，帮助 APP 在硬件未回板前联调。

APP 开发者优先阅读：[Android BLE 接入指南](android-ble-integration-guide.md)。

## APP 仓库需要实现的对接点

- 扫描 `VBRK-XXXX` 设备并解析厂商字段。
- 通过 NFC URI 定向连接目标 MAC。
- 调用绑定表服务完成 `READ_ALL`、`WRITE_ONE`、`SET_QTY`。
- 调用灯控服务完成 `FIND`、`PICK`、`SORT`、`STOCK_IN`、`OFF`。
- 根据 `table_seq` 判断本地缓存是否过期。
- 写类操作前确保 BLE 链路已加密。
