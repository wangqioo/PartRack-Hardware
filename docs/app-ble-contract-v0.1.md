# APP BLE 契约 v0.1

本文档是 Android APP 对接 PartRack 智能底盘的稳定契约。APP 主体由 `Yrd980/LCSC_android_erp` 维护，本仓库只定义硬件/固件/BLE/NFC 契约。

## 开发期状态

- 当前开发板设备名：`VBRK-0000`。
- 量产扫描建议：按 `VBRK-` 前缀过滤。
- Manufacturer Company ID：开发期 `0xFFFF`。
- `batch_id`：当前开发值 `1`。
- NFC URI、OTA、电池 ADC 尚未完成 APP 可依赖闭环。

## 广播 Manufacturer Data

原始 Manufacturer Specific Data：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 2 | `company_id` | 小端，开发期 `FF FF` |
| 2 | 1 | `proto_ver` | 当前 `0x01` |
| 3 | 2 | `batch_id` | 小端 |
| 5 | 1 | `battery_pct` | 0-100 |
| 6 | 1 | `status_flags` | bit0 低电量，bit1 未绑定槽位，bit2 正在点灯，bit3 故障 |
| 7 | 2 | `table_seq` | 低 16 位，小端 |
| 9 | 2 | reserved | 当前 `00 00` |

Android `ScanRecord.getManufacturerSpecificData(0xFFFF)` 通常返回去掉 Company ID 后的数据：

| Android 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 1 | `proto_ver` |
| 1 | 2 | `batch_id` |
| 3 | 1 | `battery_pct` |
| 4 | 1 | `status_flags` |
| 5 | 2 | `table_seq` |
| 7 | 2 | reserved |

## GATT

### Binding Table Service

- Service UUID: `7f4b0001-8d1a-4d45-9a4e-2b4a7c000000`
- Binding Control Point: `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Write, Notify
  - Permission: encrypted write
- Table Info: `7f4b1002-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Read, Notify

### Light Control Service

- Service UUID: `7f4b0002-8d1a-4d45-9a4e-2b4a7c000000`
- Light Command: `7f4b2001-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Write Without Response
- Light Status: `7f4b2002-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Read, Notify

## Table Info

7 字节：

| 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 4 | `table_seq` 小端 |
| 4 | 2 | `crc16` 小端 |
| 6 | 1 | `slot_count`，当前 25 |

## 槽位记录

16 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `slot` | 1-25，0 表示空/无效 |
| 1 | 10 | `part_id` | ASCII，不足补 `00` |
| 11 | 2 | `qty` | uint16 小端 |
| 13 | 1 | `flags` | bit0 MSD，bit1 低库存，bit2 自定义 |
| 14 | 1 | reserved | 写 `00` |
| 15 | 1 | `crc8` | CRC-8/MAXIM over bytes 0-14 |

## Binding Control Point

响应格式：

```text
op(1B) + status(1B) + payload
```

状态码：

| 值 | 含义 | APP 建议 |
|---:|---|---|
| `0x00` | 成功 | 继续流程 |
| `0x01` | 参数错误 | 检查长度、slot、CRC 或 opcode |
| `0x02` | 槽满 | 提示用户整理槽位 |
| `0x03` | flash 忙 | 延迟后重试 |
| `0x04` | CRC 错误 | 重新生成记录后重试 |

操作码：

| Op | 名称 | 请求载荷 |
|---:|---|---|
| `0x01` | `READ_ONE` | `slot(1B)` |
| `0x02` | `READ_ALL` | 无 |
| `0x10` | `WRITE_ONE` | `record(16B)` |
| `0x11` | `CLEAR_ONE` | `slot(1B)` |
| `0x20` | `INSERT_AT` | `slot(1B) + record(16B)` |
| `0x21` | `REMOVE_AT` | `slot(1B)` |
| `0x22` | `MOVE_BLOCK` | `from(1B) + to(1B) + len(1B)` |
| `0x30` | `SET_QTY` | `slot(1B) + qty(2B)` |
| `0xF0` | `FACTORY_RESET` | `magic(4B) = 5A 5A A5 A5` 小端按线序 |

`READ_ALL` 成功时，固件发送 25 条 notify：

```text
02 00 + record(16B)
```

最后发送结束帧：

```text
02 00 FF
```

## Notify 顺序

写类操作成功并改变绑定表时：

1. Binding Control Point 状态 notify。
2. Table Info notify。

APP 不应假设两个 notify 在同一个 BLE connection event 内到达。

## 灯控

Light Command 固定 17 字节：

| 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 1 | `mode` |
| 1 | 4 | `mask_a` 小端 |
| 5 | 4 | `mask_b` 小端 |
| 9 | 3 | `color_a` RGB |
| 12 | 3 | `color_b` RGB |
| 15 | 2 | `timeout_s` 小端 |

模式：

| 值 | 名称 |
|---:|---|
| `0x00` | `OFF` |
| `0x01` | `FIND` |
| `0x02` | `PICK` |
| `0x03` | `SORT` |
| `0x04` | `STOCK_IN` |
| `0x05` | `FX` |

Light Status 固定 3 字节：

```text
mode(1B) + remaining_s(2B little endian)
```

Light Command 是 Write Without Response。APP 下发后应读取或订阅 Light Status，用 `mode + remaining_s` 判断固件是否接受了当前状态。
