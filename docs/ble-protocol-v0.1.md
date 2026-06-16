# BLE 协议 v0.1

适用范围：智能底盘与 Android APP 之间的蓝牙交互。

## 设备标识

- 设备名：`VBRK-XXXX`，`XXXX` 为 MAC 后两字节十六进制，放在 Scan Response。
- `batch_id`：`uint16`，出厂或首次绑定时分配，写入 MCU flash 与 NFC NDEF。
- `proto_ver`：当前 `0x01`。

## 广播

常态广播间隔 1500ms。NFC 碰醒后快速广播，100ms 间隔持续 30s。

Manufacturer Specific Data：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 2 | `company_id` | 开发期 `0xFFFF` |
| 2 | 1 | `proto_ver` | 协议版本 |
| 3 | 2 | `batch_id` | 小端 |
| 5 | 1 | `battery_pct` | 0-100 |
| 6 | 1 | `status_flags` | bit0 低电量，bit1 存在未绑定槽位，bit2 正在点灯，bit3 故障 |
| 7 | 2 | `table_seq` | 绑定表版本号低 16 位 |

## 槽位记录

16 字节定长：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `slot` | 槽位号 1-25，0 无效 |
| 1 | 10 | `part_id` | ASCII，不足补 `0x00` |
| 11 | 2 | `qty` | `uint16` 小端 |
| 13 | 1 | `flags` | bit0 MSD，bit1 低库存，bit2 自定义物料 |
| 14 | 1 | reserved | 写 `0x00` |
| 15 | 1 | `crc8` | 前 15 字节 CRC-8/MAXIM |

## 绑定表操作码

所有响应帧格式：

```text
op(1B) + status(1B) + payload
```

`status`：

- `0x00` 成功
- `0x01` 参数错误
- `0x02` 槽满
- `0x03` flash 忙
- `0x04` CRC 错误

| Op | 名称 | 载荷 |
|---:|---|---|
| `0x01` | `READ_ONE` | `slot(1B)` |
| `0x02` | `READ_ALL` | 无 |
| `0x10` | `WRITE_ONE` | `record(16B)` |
| `0x11` | `CLEAR_ONE` | `slot(1B)` |
| `0x20` | `INSERT_AT` | `slot(1B) + record(16B)` |
| `0x21` | `REMOVE_AT` | `slot(1B)` |
| `0x22` | `MOVE_BLOCK` | `from(1B) + to(1B) + len(1B)` |
| `0x30` | `SET_QTY` | `slot(1B) + qty(2B)` |
| `0xF0` | `FACTORY_RESET` | `magic(4B)`，`0x5A5AA5A5` |

### 手工测试帧

以下帧可在 nRF Connect 中使用 Hex / Byte Array 模式写入。

写入第 1 槽，`part_id=C1234567`，`qty=12`：

```text
10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

读取第 1 槽：

```text
01 01
```

读取全表：

```text
02
```

注意：如果 nRF Connect 的 `Last Write` 显示为 `3130 2030...`，说明写入模式是 Text/String，而不是 Hex / Byte Array。

## 灯控帧

17 字节，Write Without Response：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `mode` | 灯效模式 |
| 1 | 4 | `mask_a` | bit0 = 1 号槽，bit24 = 25 号槽 |
| 5 | 4 | `mask_b` | 第二掩码 |
| 9 | 3 | `color_a` | RGB |
| 12 | 3 | `color_b` | RGB |
| 15 | 2 | `timeout_s` | `0` 默认 30s，上限 300s |

模式：

| 值 | 名称 |
|---:|---|
| `0x00` | `OFF` |
| `0x01` | `FIND` |
| `0x02` | `PICK` |
| `0x03` | `SORT` |
| `0x04` | `STOCK_IN` |
| `0x05` | `FX` |

## Device Health Service

Service UUID：

```text
7f4b0003-8d1a-4d45-9a4e-2b4a7c000000
```

| Characteristic | UUID | 属性 | 用途 |
|---|---|---|---|
| Device Health | `7f4b3001-8d1a-4d45-9a4e-2b4a7c000000` | Read, Notify | 电量、复位原因和健康状态 |

Device Health payload 固定 4 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `battery_pct` | 0-100；没有电池 ADC alias 时开发板返回 100 |
| 1 | 2 | `reset_reason` | 小端 bitmask |
| 3 | 1 | `health_flags` | bit0 看门狗已启用，bit1 健康状态故障 |

`reset_reason` bit：

| Bit | 含义 |
|---:|---|
| 0 | power-on |
| 1 | reset pin |
| 2 | software reset |
| 3 | watchdog |
| 4 | CPU lockup |
| 5 | low-power/off wake |
| 15 | other/unknown platform reason |

当前固件启动时读取并清除 Zephyr `hwinfo` reset cause，后续 APP 通过 Device Health 读取的是本次启动捕获值。看门狗驱动已编入，真正启用仍受固件配置开关控制。
