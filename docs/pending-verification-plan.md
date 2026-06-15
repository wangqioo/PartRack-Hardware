# 待验证计划

本文档记录已经开发、已经构建验证、仍待实机验证和仍未接入的功能。后续每完成一项实机验证，都应同步更新 `docs/verification-matrix.md`。

## 当前硬件基线

- 开发板：Seeed Studio XIAO nRF52840 Sense。
- 当前启动链：Seeed 官方 `UF2 Bootloader 0.6.1` + `S140 7.3.0`。
- 当前 PartRack 裸 XIAO BLE 固件：`/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-bare/app/zephyr/zephyr.uf2`。
- 2026-06-16 烧录后状态：设备可被手机和 Mac BLE smoke 脚本连接，设备名 `VBRK-0000`。
- 当前裸 XIAO variant 不加载外接 WS2812、灯条电源门控和 NFC FD；相关 warning 属于预期。

## 已开发并待实机验证

| 功能 | 已开发内容 | 当前证据 | 待验证步骤 | 通过标准 |
|---|---|---|---|---|
| BLE 扫描和连接回归 | 设备名 `VBRK-0000`、connectable advertising、断开后重启广播。 | 2026-06-15 恢复后手机已能连接；历史记录有扫描、连接和断开后恢复广播。 | 手机 nRF Connect 扫描、连接、断开、再次扫描并连接。 | 每次断开后 10 秒内能重新扫到 `VBRK-0000`，再次连接成功。 |
| GATT service discovery | Binding Table Service 和 Light Control Service 已实现。 | 历史手机 nRF Connect 已发现两个 service。 | 连接后执行 service discovery。 | 能看到 `7f4b0001-...` 和 `7f4b0002-...` 两个 service。 |
| Table Info 读取 | `table_seq + crc16 + slot_count` 特征已实现。 | 历史返回 `0100 0000 2DE4 19`。 | 读取 `7f4b1002-8d1a-4d45-9a4e-2b4a7c000000`。 | 返回 7B；写入绑定表后 `table_seq` 或 CRC 有变化。 |
| Binding Control Point notify | 绑定表命令响应通过 notify 返回。 | 2026-06-16 Mac 自动烟测已收到 `10 00`、`READ_ONE` payload、`READ_ALL` 部分 payload 和 `30 00`。 | APP 侧开启 `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000` notify 后复验。 | 写命令后能收到对应状态或数据 notify。 |
| `WRITE_ONE -> READ_ONE` | `WRITE_ONE`、`READ_ONE`、槽位记录编码和解码已实现。 | 2026-06-16 Mac 自动烟测已通过：`10 00` 后读回完整 slot 1 记录。 | APP/nRF52832 复验；必要时重复 Mac smoke。 | 先收到成功状态，再收到 `01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18`。 |
| `READ_ALL` | 全表读取和结束帧已实现于 host/model。 | 2026-06-16 真实 BLE 只收到 slot 1 和后续空槽记录，未收到 `02 00 FF`。 | 修复固件 paced/asynchronous notify 后，写 `02` 到 Binding Control Point。 | 收到逐槽记录，最后收到 `02 00 FF`。 |
| `SET_QTY` | 单槽数量修改已实现。 | 2026-06-16 Mac 自动烟测已收到 `30 00`。 | 补 `SET_QTY` 后 `READ_ONE` 再读，确认数量为 `0x002A`；APP/nRF52832 复验。 | 收到 `30 00` 状态 notify；再次 `READ_ONE` 时数量为 `0x002A`；Table Info 变化。 |
| `CLEAR_ONE` | 单槽清空已实现。 | host/model 和 smoke vectors 已覆盖。 | 破坏性测试窗口内写 `11 01`，再 `READ_ONE slot 1`。 | slot 1 返回空记录，Table Info 变化。 |
| `INSERT_AT` | 插入并移动槽位已实现。 | host/model 已覆盖基础操作。 | 准备两条记录后执行 `INSERT_AT`，再 `READ_ALL`。 | 插入位置及后续槽位顺序符合协议。 |
| `REMOVE_AT` | 删除并前移槽位已实现。 | host/model 已覆盖基础操作。 | 准备多条记录后执行 `REMOVE_AT`，再 `READ_ALL`。 | 删除位置为空或后续记录前移符合协议。 |
| `MOVE_BLOCK` | 块移动已实现。 | host/model 已覆盖基础操作。 | 准备多条记录后执行 `MOVE_BLOCK`，再 `READ_ALL`。 | 槽位顺序符合协议，未影响块外数据。 |
| `FACTORY_RESET` | 全表清空命令已实现。 | host/model 和 smoke vectors 已覆盖。 | 破坏性测试窗口内写 `F0 A5 A5 5A 5A`。 | 全部槽位为空，Table Info 回到空表状态。 |
| settings/NVS 持久化 | `vbrk/binding_table` snapshot、magic/version/CRC16、损坏拒绝已实现。 | `tools/storage_snapshot_test.c` host verified。 | 写入 slot 1，重启 XIAO，再连接并 `READ_ONE slot 1`。 | 重启后读回同一 16B 记录，`table_seq` 不回退。 |
| BLE 加密/配对 | Binding Control Point 配置为 encrypted write；固件连接后请求 `BT_SECURITY_L2`；开发期支持 4 个 paired peers。 | 2026-06-16 串口记录 `security changed: level 2`，Mac 自动烟测 encrypted write 通过。 | APP 侧实现 authentication/encryption retry，并在 Android 真机复验。 | 配对后写入成功；APP 侧可据此实现 authentication/encryption retry。 |
| Light Status 读取 | `mode + remaining_s` 特征已实现。 | 历史空闲返回 `0000 00`。 | 读取/订阅 `7f4b2002-8d1a-4d45-9a4e-2b4a7c000000`。 | OFF 时返回 `0000 00`；灯控命令后 mode/remaining_s 变化。 |
| Light Command | `FIND/PICK/SORT/STOCK_IN/OFF` 命令解析和状态框架已实现。 | host/model 覆盖策略；真实 BLE 待验证。 | 写 Light Command，例如 `01 01 00 00 00 00 00 00 00 FF 00 00 00 00 00 0A 00`。 | Light Status 进入对应 mode，超时后回到 OFF。 |
| 灯条电源门控 | D3/P0.29 高电平上电，OFF/超时断电。 | 代码和 build 存在；裸 variant 当前不加载外设 alias。 | 烧录 peripheral build，万用表测 D3/P0.29。 | 非 OFF 命令拉高，OFF 或超时后拉低。 |
| WS2812 frame 生成 | `mask_a/mask_b` 生成 25 槽 RGB frame，B 覆盖 A。 | `tools/light_frame_test.c` host verified。 | 接灯条，发送单槽、多槽、A/B 重叠命令。 | 对应槽位颜色正确，B 覆盖 A，越界 bit 不影响 25 槽。 |
| WS2812 Zephyr 输出 | XIAO peripheral build 使用 `worldsemi,ws2812-spi`，25 像素，GRB，4 MHz。 | `tools/verify_host.sh --peripheral-build` 已确认编入驱动。 | 接真实 25 颗 WS2812，烧 peripheral build。 | 灯条按 Light Command 亮灭，颜色和槽位正确。 |
| NFC FD GPIO 唤醒入口 | `vbrk-nfc-fd` GPIO adapter 和 debounce 入口已实现。 | peripheral build 可编入 alias；未接真实 NT3H2111。 | 接 NT3H2111 FD 引脚，触碰 NFC。 | FD 中断触发后固件进入预期唤醒/广播刷新路径。 |

## 已做工具和模型但待真实环境补证

| 工具 / 模型 | 已完成内容 | 当前限制 | 后续验证 |
|---|---|---|---|
| `tools/ble_gatt_smoke_test.py` | 生成测试帧，并可自动连接、订阅 notify、执行 encrypted `WRITE_ONE -> READ_ONE`、`READ_ALL` 部分校验、`SET_QTY`。 | 2026-06-16 已可在当前 Mac 跑通非破坏性 smoke；`READ_ALL` end marker 缺失被显式记录为固件待修复。 | 修复 paced `READ_ALL` 后恢复对 `02 00 FF` 的硬性校验。 |
| BLE lifecycle host model | 覆盖断开后恢复广播、connected 状态延迟刷新、notify 失败不回滚 domain 操作。 | 不证明真实 radio、手机栈、实际 notify 顺序。 | 手机实测断开重连、写入后广播数据刷新和 notify 顺序。 |
| Binding Table host/model tests | 覆盖槽位操作、记录编码、CRC snapshot。 | 不证明 Zephyr settings/NVS 与 flash 实际写入成功。 | 做“写入 -> 重启 -> 读回”真实设备闭环。 |
| Light policy/frame/state tests | 覆盖超时、模式、25 槽 RGB frame 和状态切换。 | 不证明 GPIO、电源门控和真实 WS2812 时序。 | 接真实硬件测 GPIO 和灯条。 |

## 未开发或未完整接入

| 功能 | 当前状态 | 下一步 |
|---|---|---|
| NT3H2111 I2C / NDEF | 尚未完整接入。 | 确认 I2C 地址和引脚，读取 tag memory，写入 `lcscerp://device?...` URI，验证手机触碰路由。 |
| 电池 ADC | 尚未接入。 | 按 Seeed 电池读取注意事项处理 `P0.14` 和 `P0.31`，实现电压读取、百分比估算和广播字段更新。 |
| 低功耗 | 尚未形成实测方案。 | 定义广播、连接、灯条关闭、灯条点亮等电流测试场景，接电源表记录。 |
| OTA / Secure DFU | 尚未集成，是 v1 release gate。 | 决定 beta 是否需要 OTA；集成 MCUboot/DFU，验证升级、失败处理和回滚策略。 |
| nRF52832 目标迁移 | 当前主要在 XIAO nRF52840 Sense 验证。 | 在 `nrf52dk_nrf52832` 或目标板上构建/烧录，复核 Flash/RAM、引脚、功耗和灯条驱动方案。 |
| 复位原因上报 | 文档列为待开发。 | 接入 Zephyr reset reason 或芯片寄存器读取，定义 BLE 上报格式。 |
| 看门狗 | 尚未作为 release 行为接入和验证。 | 设定 watchdog 策略，验证正常喂狗和异常复位上报。 |

## 推荐验证顺序

1. BLE 回归：扫描、连接、service discovery、Table Info、Light Status。
2. Binding CP notify：开启 notify，执行 `WRITE_ONE -> READ_ONE`。
3. 修复全表同步：将 `READ_ALL` 改为 paced/asynchronous notify，复验结束帧 `02 00 FF`。
4. 数量修改补证：执行 `SET_QTY` 后追加 `READ_ONE`，确认数量和 Table Info 变化。
5. 持久化：写入 slot 1，重启设备，再 `READ_ONE slot 1`。
6. 断开重连：断开后再次扫描连接，确认广播恢复。
7. 破坏性绑定表测试：`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`FACTORY_RESET`。
8. 灯控无灯条验证：发送 Light Command，读取 Light Status，确认超时回 OFF。
9. 外设 build 验证：烧 peripheral build，测 D3/P0.29 电源门控。
10. 真实 WS2812：接 25 颗灯条，验证颜色、槽位、B 覆盖 A、超时熄灯。
11. NFC FD 和 NT3H2111 NDEF。
12. 电池 ADC、低功耗、OTA/DFU。
13. nRF52832 目标板迁移和回板验证。

## 证据记录格式

每次验证后追加以下信息到 `docs/verification-matrix.md` 对应行：

```text
日期：
固件 UF2 路径 / commit：
设备：
启动链：
接线：
测试工具：
操作步骤：
原始结果：
结论：
剩余风险：
```

当前已知固件路径：

```text
/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-bare/app/zephyr/zephyr.uf2
/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-peripherals/app/zephyr/zephyr.uf2
```
