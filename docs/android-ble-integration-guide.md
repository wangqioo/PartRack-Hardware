# Android BLE 接入指南

本文给 Android APP 开发者使用，目标是让外部 APP 仓库可以直接接入当前 nRF52 固件。

当前开发板状态：

- 开发板：Seeed Studio XIAO nRF52840 Sense。
- 固件工具链：nRF Connect SDK / Zephyr。
- 当前设备名：`VBRK-0000`。
- 当前已烧录固件：peripheral 完整版 XIAO UF2。
- 当前 UF2 路径：`/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-peripherals/app/zephyr/zephyr.uf2`。
- 当前 UF2 SHA-256：`25514abd08d93a7154a704e4b9a151acb4f7823dca6617c8cb043741ea972689`。
- 当前已实机验证：手机/Mac 可扫描、连接、发现 GATT 服务、读取 `Table Info`、`Light Status` 和 Device Health。
- 当前已脚本化并在 XIAO 实机通过：Mac CoreBluetooth/Bleak 自动 smoke 可完成 encrypted `WRITE_ONE -> READ_ONE` notify 闭环、paced `READ_ALL` 结束帧、`SET_QTY` 后读回、Light Status 状态变化、10s 超时 OFF、Device Health 读取和单槽重启恢复。
- 当前固件已接入 settings/NVS 持久化和 XIAO WS2812 SPI 输出绑定；单槽“写入 -> 重启 -> 读回”已通过，真实灯条点亮仍待确认。

## 当前给软件侧测试的固件

2026-06-16 已给 nRF52840/XIAO 烧录 peripheral 完整版固件。该固件用于 APP/BLE 联调，不是裸板最小验证版。

APP 侧现在可以重点测试：

1. 扫描 `VBRK-0000` 并连接。
2. `discoverServices()` 后确认三个 service 都存在：Binding Table、Light Control、Device Health。
3. 处理 Binding Control Point 的 encrypted write：首次写入遇到加密/认证错误时触发系统配对，然后重试。
4. 开启 Binding CP notify，执行 `WRITE_ONE -> READ_ONE`。
5. 执行 `READ_ALL`，等待 `02 00 FF` 结束帧。
6. 执行 `SET_QTY`，确认 notify 和 `Table Info.table_seq` 变化。
7. 下发 Light Command，读取或订阅 `Light Status`。
8. 连接后读取 Device Health，当前实测样例为 `64 02 00 00`。

暂时不要作为 APP 必须依赖的能力：

- NFC URI / NT3H2111 NDEF。
- OTA / Secure DFU。
- 真实电池 ADC 百分比。
- 真实 WS2812 槽位颜色，除非测试设备已经接好灯条和供电。
- 破坏性绑定表命令，除非明确进入测试窗口。

## 当前 APP 可并行开发任务

APP 同学现在可以不等最终 PCB，直接按当前 XIAO nRF52840 Sense 固件开发 BLE Central 侧逻辑。

| 模块 | 现在可做 | 注意 |
|---|---|---|
| 扫描 | 按 `VBRK-0000` 或 `VBRK-` 前缀扫描 | 设备名在 Scan Response 里 |
| 广播解析 | 解析 Manufacturer Data 的协议版本、电量、状态位、`table_seq` | Company ID `0xFFFF` 是开发占位 |
| 连接 | `connectGatt()` 后 `discoverServices()` | 断开后设备会恢复广播 |
| 绑定表同步 | 读取 `Table Info`，再发送 `READ_ALL` | 全表结束帧是 `02 00 FF` |
| 单槽写入 | `WRITE_ONE` 后 `READ_ONE` 比对 16B 记录 | Binding CP 写入需要加密/配对 |
| 库存数量 | 用 `SET_QTY` 修改槽位数量 | 成功后应观察 notify 和 `table_seq` 变化 |
| 灯控 | 下发 `FIND` / `PICK` / `SORT` / `STOCK_IN` / `OFF` | 真实灯条颜色仍待硬件实测 |
| Device Health | 连接后读取 `7f4b3001-...` | 当前实测 `64 02 00 00`；电池仍是开发板占位 |

建议先实现这个最小闭环：

```text
scan VBRK-0000
connect
discoverServices
read Table Info
enable Binding CP notify
enable Table Info notify
read Device Health
WRITE_ONE slot 1
READ_ONE slot 1
READ_ALL until 02 00 FF
SET_QTY slot 1
send Light Command
read or subscribe Light Status
```

不要作为量产假设的开发期占位：

- 设备名固定为 `VBRK-0000`。
- Manufacturer Company ID 为 `0xFFFF`。
- `batch_id = 1`。
- NFC URI、OTA、真实电池 ADC 已在硬件计划中，但当前固件还没有提供 APP 可用接口。

## 角色边界

本仓库只提供硬件、固件和 BLE 协议契约。

Android APP 需要作为 BLE Central：

- 扫描 `VBRK-XXXX` 设备。
- 连接目标设备。
- 发现 GATT 服务。
- 读写绑定表。
- 下发灯光命令。
- 维护 APP 本地库存数据和设备槽位绑定关系。

设备固件作为 BLE Peripheral：

- 广播设备名和设备状态。
- 提供 Binding Table Service。
- 提供 Light Control Service。
- 保存硬件侧槽位绑定表。

## Android 权限

Android 12 及以上至少需要：

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

Android 11 及以下扫描 BLE 通常还需要定位权限：

```xml
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

APP 侧需要做运行时权限申请。没有权限时常见表现是扫描不到设备，或能看到缓存设备但连接/发现服务失败。

## 扫描设备

开发期先按设备名扫描：

```text
VBRK-0000
```

量产或多设备场景按前缀扫描：

```text
VBRK-
```

设备名在 Scan Response 中，Android 扫描回调里可从 `ScanResult.scanRecord.deviceName` 读取。

### 广播 Manufacturer Data

开发期 Company ID：

```text
0xFFFF
```

固件广播的 Manufacturer Specific Data 原始字节格式：

| 原始偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 2 | `company_id` | 小端，开发期 `FF FF` |
| 2 | 1 | `proto_ver` | 当前 `0x01` |
| 3 | 2 | `batch_id` | 小端，当前开发值 `1` |
| 5 | 1 | `battery_pct` | 当前开发值 `100` |
| 6 | 1 | `status_flags` | 状态位 |
| 7 | 2 | `table_seq` | 绑定表版本号低 16 位，小端 |
| 9 | 2 | reserved | 当前为 `00 00` |

Android `ScanRecord.getManufacturerSpecificData(0xFFFF)` 通常返回去掉 Company ID 后的数据，因此 APP 侧数组偏移会变成：

| Android 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 1 | `proto_ver` |
| 1 | 2 | `batch_id` |
| 3 | 1 | `battery_pct` |
| 4 | 1 | `status_flags` |
| 5 | 2 | `table_seq` |
| 7 | 2 | reserved |

`status_flags`：

| Bit | 含义 |
|---:|---|
| 0 | 低电量 |
| 1 | 存在未绑定槽位 |
| 2 | 正在点灯 |
| 3 | 故障 |

## GATT 服务

### Binding Table Service

```text
Service UUID:
7f4b0001-8d1a-4d45-9a4e-2b4a7c000000
```

| Characteristic | UUID | 属性 | 用途 |
|---|---|---|---|
| Binding Control Point | `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000` | Write, Notify | 绑定表命令和响应 |
| Table Info | `7f4b1002-8d1a-4d45-9a4e-2b4a7c000000` | Read, Notify | 绑定表版本、CRC、槽位数 |

注意：`Binding Control Point` 当前固件权限是 encrypted write。Android 写入前需要处理配对/加密。若写入返回 authentication/encryption 相关错误，需要触发系统配对流程后重试。

### Light Control Service

```text
Service UUID:
7f4b0002-8d1a-4d45-9a4e-2b4a7c000000
```

| Characteristic | UUID | 属性 | 用途 |
|---|---|---|---|
| Light Command | `7f4b2001-8d1a-4d45-9a4e-2b4a7c000000` | Write Without Response | 下发灯光命令 |
| Light Status | `7f4b2002-8d1a-4d45-9a4e-2b4a7c000000` | Read, Notify | 读取灯光状态 |

当前灯光服务已接入 XIAO WS2812 SPI 输出绑定。APP 可按协议下发灯控命令；真实灯条颜色和槽位仍需硬件接线后验证。

## 连接后初始化流程

APP 连接设备后建议按这个顺序执行：

```text
1. connectGatt()
2. discoverServices()
3. 找到 Binding Table Service、Light Control Service 和 Device Health Service
4. 读取 Table Info
5. 开启 Binding Control Point notify
6. 开启 Table Info notify
7. 开启 Light Status notify
8. 读取 Device Health
9. 发送 READ_ALL，同步硬件绑定表
10. 根据 table_seq 和 crc16 更新 APP 本地缓存状态
```

如果只做最小联调，可以先执行：

```text
连接 -> discoverServices -> 读取 Table Info -> 开启 Binding CP notify -> WRITE_ONE -> READ_ONE
```

### Device Health Service

```text
Service UUID:
7f4b0003-8d1a-4d45-9a4e-2b4a7c000000
```

| Characteristic | UUID | 属性 | 用途 |
|---|---|---|---|
| Device Health | `7f4b3001-8d1a-4d45-9a4e-2b4a7c000000` | Read, Notify | 电量、复位原因、看门狗/故障状态 |

Device Health 固定 4 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `battery_pct` | 0-100；当前 XIAO 裸板无电池 ADC alias 时返回 100 |
| 1 | 2 | `reset_reason` | 小端 bitmask |
| 3 | 1 | `health_flags` | bit0 watchdog enabled，bit1 fault |

`reset_reason` bitmask：

| Bit | 含义 |
|---:|---|
| 0 | 上电复位 |
| 1 | Reset pin |
| 2 | 软件复位 |
| 3 | 看门狗复位 |
| 4 | CPU lockup |
| 5 | 低功耗/关机唤醒 |
| 15 | 其他平台原因 |

APP 建议连接后读取一次 Device Health，用于调试“为什么设备重启”和“电池显示”。当前 notify 属性已预留，但开发期先按 read-first 接入；等电池周期采样策略冻结后再依赖 notify 做 UI 实时刷新。

## Table Info 格式

`Table Info` 固定 7 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 4 | `table_seq` | 小端，绑定表变更版本 |
| 4 | 2 | `crc16` | 小端，全表 CRC16 |
| 6 | 1 | `slot_count` | 当前 `25` |

当前开发板空表实测读数示例：

```text
01 00 00 00 2D E4 19
```

含义：

- `table_seq = 1`
- `crc16 = 0xE42D`
- `slot_count = 25`

## 槽位记录格式

每个槽位记录固定 16 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `slot` | 槽位号，`1-25` |
| 1 | 10 | `part_id` | ASCII，不足补 `00` |
| 11 | 2 | `qty` | 小端 `uint16` |
| 13 | 1 | `flags` | 槽位标志 |
| 14 | 1 | reserved | 写 `00` |
| 15 | 1 | `crc8` | 对前 15 字节做 CRC-8/MAXIM |

`flags`：

| Bit | 含义 |
|---:|---|
| 0 | MSD |
| 1 | 低库存 |
| 2 | 自定义物料 |

重要限制：

- 当前 `part_id` 最多 10 个 ASCII 字节。
- 如果 APP 内部物料 ID 更长，需要在 APP 侧建立短 ID 映射，或在下一版协议中调整字段长度。
- `qty` 最大 `65535`。

## Binding Table 命令

所有 Binding Control Point 响应格式：

```text
op(1B) + status(1B) + payload
```

`status`：

| 值 | 含义 |
|---:|---|
| `0x00` | 成功 |
| `0x01` | 参数错误 |
| `0x02` | 槽满 |
| `0x03` | flash 忙 |
| `0x04` | CRC 错误 |

命令：

| Op | 名称 | 写入载荷 |
|---:|---|---|
| `0x01` | `READ_ONE` | `slot(1B)` |
| `0x02` | `READ_ALL` | 无 |
| `0x10` | `WRITE_ONE` | `record(16B)` |
| `0x11` | `CLEAR_ONE` | `slot(1B)` |
| `0x20` | `INSERT_AT` | `slot(1B) + record(16B)` |
| `0x21` | `REMOVE_AT` | `slot(1B)` |
| `0x22` | `MOVE_BLOCK` | `from(1B) + to(1B) + len(1B)` |
| `0x30` | `SET_QTY` | `slot(1B) + qty(2B)` |
| `0xF0` | `FACTORY_RESET` | magic value `0x5A5AA5A5`，wire bytes `A5 A5 5A 5A` |

### 手工测试帧

以下帧可在 nRF Connect 的 Hex / Byte Array 模式写入 `Binding Control Point`。

写入第 1 槽：

```text
slot = 1
part_id = C1234567
qty = 12
flags = 0
```

写入帧：

```text
10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

读取第 1 槽：

```text
01 01
```

期望 notify 响应：

```text
01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

解释：

- `01`：`READ_ONE`
- `00`：成功
- 后 16 字节：槽位记录

读取全表：

```text
02
```

`READ_ALL` 会通过 notify 逐槽返回，最后发送结束帧：

```text
02 00 FF
```

注意：如果 nRF Connect 的 `Last Write` 显示 `3130 2030...`，说明写成了 Text/String，不是 Hex / Byte Array。

## Light Command 格式

`Light Command` 固定 17 字节，使用 Write Without Response：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `mode` | 灯效模式 |
| 1 | 4 | `mask_a` | 小端，bit0 对应 1 号槽 |
| 5 | 4 | `mask_b` | 第二掩码，预留组合灯效 |
| 9 | 3 | `color_a` | RGB |
| 12 | 3 | `color_b` | RGB |
| 15 | 2 | `timeout_s` | 小端，`0` 表示默认 30 秒，上限 300 秒 |

`mode`：

| 值 | 名称 |
|---:|---|
| `0x00` | `OFF` |
| `0x01` | `FIND` |
| `0x02` | `PICK` |
| `0x03` | `SORT` |
| `0x04` | `STOCK_IN` |
| `0x05` | `FX` |

示例：让 1 号槽绿色亮 30 秒：

```text
01 01 00 00 00 00 00 00 00 00 FF 00 00 00 00 1E 00
```

示例：关闭灯光：

```text
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## Light Status 格式

`Light Status` 固定 3 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `mode` | 当前灯效模式 |
| 1 | 2 | `remaining_s` | 小端，剩余秒数 |

空闲状态实测读数：

```text
00 00 00
```

## Android 侧建议封装

建议 APP 内单独做一个 BLE 硬件模块，不要把 GATT 细节散在页面里。

建议暴露这些领域方法：

```kotlin
interface PartRackBleClient {
    fun startScan()
    fun stopScan()
    fun connect(deviceAddress: String)
    fun disconnect()

    suspend fun readTableInfo(): TableInfo
    suspend fun readAllSlots(): List<SlotRecord>
    suspend fun readSlot(slot: Int): SlotRecord?
    suspend fun writeSlot(record: SlotRecord)
    suspend fun clearSlot(slot: Int)
    suspend fun setQty(slot: Int, qty: Int)

    suspend fun findSlots(slots: List<Int>)
    suspend fun pickSlots(slots: List<Int>)
    suspend fun lightsOff()
}
```

建议 APP 侧维护这些状态：

- `Disconnected`
- `Scanning`
- `Connecting`
- `DiscoveringServices`
- `Ready`
- `PairingRequired`
- `Error`

连接失败、服务发现失败、写入加密失败需要分开处理，否则现场调试会很难定位。

## Android 接入检查清单

第一阶段最小接入：

- [ ] 申请 BLE 运行时权限。
- [ ] 扫描并显示 `VBRK-0000`。
- [ ] 连接设备。
- [ ] `discoverServices()` 后能找到两个主服务。
- [ ] 读取 `Table Info`。
- [ ] 读取 `Light Status`。
- [ ] 开启 `Binding Control Point` notify。
- [ ] 写入 `WRITE_ONE`。
- [ ] 发送 `READ_ONE` 并通过 notify 读回。

第二阶段业务接入：

- [ ] APP 物料 ID 映射到 10 字节 `part_id`。
- [ ] APP 调用 `READ_ALL` 同步硬件绑定表。
- [ ] APP 写入 `WRITE_ONE` / `SET_QTY` 后更新本地缓存。
- [ ] APP 根据 `table_seq` 判断硬件表是否变化。
- [ ] APP 下发 `FIND` / `PICK` / `STOCK_IN` / `OFF`。
- [ ] APP 对写入失败、配对失败、断连重连做用户可理解的错误提示。

## 硬件烟测脚本

本仓库提供电脑侧 BLE/GATT 烟测脚本，APP 开发者可以用它对照 APP 行为：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python tools/ble_gatt_smoke_test.py --print-vectors
.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke
.venv/bin/python tools/ble_gatt_smoke_test.py --run-batch
```

`--run-smoke` 会自动执行：

- 扫描 `VBRK-0000`。
- 读取 `Table Info`。
- 读取 `Light Status`。
- 开启 `Binding Control Point` notify。
- 写入第 1 槽 `C1234567` / `qty=12`。
- 发送 `READ_ONE` 并校验读回记录完全一致。
- 发送 `READ_ALL` 并校验结束帧 `02 00 FF`。
- 发送 `SET_QTY` 并校验写操作状态 notify。

`--run-batch` 在 `--run-smoke` 基础上额外校验：

- `SET_QTY` 后再次 `READ_ONE`，确认数量变成 `0x002A`。
- 写类操作后 Table Info seq 增长，CRC 随内容变化。
- Light Command FIND 后 `Light Status` 进入 mode 1，发送 OFF 后回到 `00 00 00`。

单击 reset 后可执行：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --run-persistence-read
```

该命令会确认重启后 slot 1 仍可读回 `C1234567 / qty=42`。

若要同时验证清空和恢复出厂，可追加：

```bash
python3 tools/ble_gatt_smoke_test.py --run-smoke --include-destructive
```

如果输出 `CoreBluetooth reported 'BLE is unsupported'`，说明当前电脑运行环境不能访问 macOS 蓝牙后端，尚未进入设备扫描阶段，不代表 nRF 设备失败。

2026-06-16 当前 Mac 环境已跑通非破坏性 `--run-smoke`、`--run-batch` 和单击 reset 后的 `--run-persistence-read`；如果后续再次出现 CoreBluetooth 后端错误，应先排查本机蓝牙权限或运行环境，而不是直接判断固件失败。

## 当前限制和后续变化

当前限制：

- 绑定表已接入 settings/NVS 持久化，XIAO 单槽“写入 -> 重启 -> 读回”已通过；多槽和破坏性操作后的恢复仍需补证。
- WS2812 已接入 XIAO devicetree/SPI 驱动，真实灯条仍需接线确认颜色、槽位和供电。
- 设备名仍固定为 `VBRK-0000`。后续会改为按设备唯一信息生成 `VBRK-XXXX`。
- NFC URI 尚未接入。后续目标格式为 `lcscerp://device?mac=...&batch=...&ver=1`。
- OTA 尚未接入。

APP 端应尽量通过协议版本、服务 UUID、`table_seq` 和错误码做兼容，不要依赖某次开发板的临时状态。

## 相关文档

- [BLE 协议 v0.1](ble-protocol-v0.1.md)
- [APP 对接边界](app-integration-boundary.md)
- [开发计划](development-plan.md)
- [XIAO nRF52840 bring-up](xiao-nrf52840-bringup.md)
