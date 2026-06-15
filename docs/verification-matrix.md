# 验证证据台账

本文档是 PartRack-Hardware 的验证事实单一来源。证据状态只在这里更新；开发计划、集成控制台、质量计划和回板清单只应摘要或链接到本台账，不应另行声明“已验证”。

详细待验证清单、推荐验证顺序和每项通过标准见 [pending-verification-plan.md](pending-verification-plan.md)。本台账只记录证据状态和结论。

## 证据标签

| 标签 | 证明什么 | 不证明什么 |
|---|---|---|
| Host verified | 纯 C/Python 测试通过，协议编码、数据结构、策略或帧生成逻辑在主机侧符合测试向量。 | 不证明 Zephyr 集成、真实 BLE 时序、真实外设、电源、功耗或重启后行为。 |
| Model verified | 用模型或脚本模拟 Zephyr/BLE/时间边界并通过测试。 | 不证明手机、CoreBluetooth/Android BLE 栈、真实 notify 顺序或真实硬件外设通过。 |
| Zephyr build verified | 目标应用可被 Zephyr/NCS 构建，相关驱动或配置可编入固件。 | 不证明固件已烧录成功，不证明真实 BLE、NVS、WS2812、NFC、ADC 或功耗可用。 |
| Hardware verified | 在指定真实设备/板卡上按记录步骤通过验证。 | 只覆盖记录中的板卡、固件、接线和步骤；不能自动外推到 nRF52832 目标板或量产硬件。 |
| Hardware required | 当前仍需要真实设备补证，或现有证据不足以支撑 release gate。 | 不代表功能未实现；只表示尚未形成可引用的实机证据。 |

## 当前能力矩阵

| 能力 | 当前标签 | 证据来源 | 验证对象 | 缺口 / 下一步 |
|---|---|---|---|---|
| BLE 扫描 | Hardware verified | `docs/development-plan.md`：手机可扫描到 `VBRK-0000`，断开后恢复广播；`docs/xiao-nrf52840-bringup.md` 记录 XIAO bring-up。 | Seeed Studio XIAO nRF52840 Sense，手机 nRF Connect。 | 仍需在 nRF52832 目标板和回板硬件复验；量产设备名和 Company ID 不能按开发期占位外推。 |
| BLE 连接 | Hardware verified | `docs/development-plan.md`：手机可连接，连接后板载蓝灯常亮；`docs/xiao-nrf52840-bringup.md`：2026-06-16 Mac 自动烟测可连接并进入 security level 2。 | Seeed Studio XIAO nRF52840 Sense，手机 nRF Connect，Mac CoreBluetooth/Bleak。 | 仍需验证断线重连稳定性和 nRF52832 目标板表现。 |
| GATT service discovery | Hardware verified | `docs/development-plan.md`：手机可发现 Binding Table Service 和 Light Control Service。 | Seeed Studio XIAO nRF52840 Sense，手机 nRF Connect。 | 仍需在 APP 侧和目标硬件复验。 |
| `Table Info` 读取 | Hardware verified | `docs/development-plan.md`：实测返回 `0100 0000 2DE4 19`。 | Seeed Studio XIAO nRF52840 Sense，手机 nRF Connect。 | 仍需验证写类操作后 `table_seq`/CRC 变化和重启后保持。 |
| `Light Status` 读取 | Hardware verified | `docs/development-plan.md`：空闲状态返回 `0000 00`。 | Seeed Studio XIAO nRF52840 Sense，手机 nRF Connect。 | 仍需验证订阅 notify、灯控命令后 remaining/mode 变化和超时行为。 |
| Binding `WRITE_ONE -> READ_ONE` notify 闭环 | Hardware verified | `docs/xiao-nrf52840-bringup.md`：2026-06-16 `.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke` 返回 `10 00`，并读回 `01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18`。 | Seeed Studio XIAO nRF52840 Sense，bare PartRack firmware，Mac CoreBluetooth/Bleak。 | 仍需 APP 侧复验 Android 配对/重试流程；仍需 nRF52832 目标板复验。 |
| `READ_ALL` | Hardware verified | `docs/xiao-nrf52840-bringup.md`：2026-06-16 修复为 paced/asynchronous notify 后，`.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke` 收到 slot 1、后续空槽记录和 `02 00 FF` 结束帧；`tools/verify_host.sh --host-only` 覆盖 pacer retry/end marker。 | Seeed Studio XIAO nRF52840 Sense，bare PartRack firmware，Mac CoreBluetooth/Bleak。 | 仍需 Android APP 侧处理超时/结束帧并复验；仍需 nRF52832 目标板复验。 |
| `SET_QTY` | Hardware verified | `docs/xiao-nrf52840-bringup.md`：2026-06-16 自动烟测在 slot 1 有记录后执行 `SET_QTY 30 01 2A 00`，收到 `30 00`。 | Seeed Studio XIAO nRF52840 Sense，bare PartRack firmware，Mac CoreBluetooth/Bleak。 | 仍需补 `READ_ONE` 再读数量为 `0x002A` 的端到端证据，以及 APP/nRF52832 复验。 |
| settings/NVS 重启恢复 | Hardware required | `tools/storage_snapshot_test.c` 覆盖 snapshot magic/version/CRC/损坏拒绝；`docs/development-plan.md` 记录固件已实现并烧录到 XIAO，但仍需“写入 -> 重启 -> 读回”手机实机确认。 | Host verified；XIAO 固件已具备实现但未完成闭环证据。 | 手机写入槽位，重启设备，再连接并 `READ_ONE` 读回；记录日期、固件版本和原始结果。 |
| 灯条电源门控 | Hardware required | `tools/light_policy_test.c`、`tools/light_state_test.c` 覆盖策略和超时；`docs/development-plan.md` 记录 D3/P0.29 初版实现。 | Host verified；开发板引脚实现存在。 | 烧录灯控固件，测 D3/P0.29：非 OFF 拉高，OFF/超时拉低；回板后复验目标 P-MOS。 |
| WS2812 真实灯条 | Hardware required | `tools/light_frame_test.c` 覆盖 25 槽 RGB frame；Zephyr build 已确认编入 `ws2812_spi.c`、`spi_nrfx_spim.c`、`nrfx_spim.c`；XIAO devicetree 已绑定 `worldsemi,ws2812-spi`。 | Host verified + Zephyr build verified；尚未接真实 25 颗 WS2812 灯条。 | 接真实灯条，验证 1-25 槽位、颜色、B 覆盖 A、多槽 PICK、OFF 和超时熄灯。 |
| NFC FD | Hardware required | `docs/development-plan.md` 记录已有 NFC FD GPIO 唤醒入口；`docs/integration-control.md` 标为 v1 目标且未完成实机闭环。 | 无完整实机证据。 | 接 NT3H2111 FD 引脚，验证触碰后唤醒或快速广播 30 秒。 |
| NT3H2111 I2C / NDEF | Hardware required | `docs/development-plan.md` 阶段 3 待办：确认 I2C 地址/引脚、实现读写、写入 NDEF URI。 | 尚未接入。 | 实测 I2C 通信，写入 `lcscerp://device?...` URI，手机触碰可读取并路由。 |
| 电池 ADC | Hardware required | `docs/development-plan.md` 记录电池 ADC 未接入；`hardware/prototype-validation-checklist-v1.md` 要求 ADC 单调性。 | 尚未接入。 | 接入 ADC 分压，做标定和电压下降单调性测试。 |
| 低功耗 | Hardware required | `docs/development-plan.md` 记录低功耗未接入；回板清单列 BLE 广播/连接电流和灯条关闭泄漏。 | 尚无实测电流。 | 冻结期望值，实测广播、连接、灯条关闭泄漏、单槽/多槽/25 槽电流。 |
| nRF52832 资源 / 引脚 / 功耗预算 | Hardware required | `docs/integration-control.md`：目标硬件为 nRF52832；XIAO 引脚不能直接当作 nRF52832 pinmap；`docs/development-plan.md` 待回到 nRF52832 验证。 | 当前主要证据来自 XIAO nRF52840 Sense。 | 在 `nrf52dk_nrf52832` 或目标板上构建/烧录，复核 Flash/RAM、SPI/I2C/GPIO 引脚、功耗和 OTA 余量。 |
| OTA / DFU | Hardware required | `docs/integration-control.md`：OTA DFU 是 v1 release gate；`docs/development-plan.md` 阶段 6 待办 MCUboot/DFU 集成。 | 尚未集成。 | 决定 v1 beta 是否需要 OTA；集成 MCUboot/DFU，验证升级、回滚/失败处理和 release gate 记录。 |

## 新证据登记流程

1. 记录日期、固件版本或 commit、设备/板卡、接线和测试人。
2. 写清命令或手工步骤，例如 `tools/ble_gatt_smoke_test.py --run-smoke`、手机 nRF Connect 操作、万用表/电源表测量步骤。
3. 保存原始结果指针：日志、截图、串口输出、BLE 抓包、照片、视频或测量表格路径。
4. 在本台账对应行更新“当前标签”“证据来源”“验证对象”“缺口 / 下一步”。
5. 只有当证据覆盖真实设备闭环时，才可把 `Hardware required` 升级为 `Hardware verified`；升级时必须保留适用范围，不能把 XIAO 证据外推为 nRF52832 目标板证据。

## Release Gate 摘要

| Gate | 验收口径 | 当前状态 |
|---|---|---|
| M0 | 无 NFC 前提下完成扫描、连接、Table Info、绑定表读写、灯控状态。 | 部分通过：扫描、连接、service discovery、Table Info、Light Status、encrypted `WRITE_ONE -> READ_ONE`、`READ_ALL` 结束帧和 `SET_QTY` 已在 XIAO 实机通过；灯控状态变化、灯条电源门控实机闭环仍缺。 |
| M1 | NFC 触发 APP 定向连接并下发 FIND。 | 未通过：NFC FD、NT3H2111 I2C/NDEF 和 APP URI 路由仍需实机验证。 |
| M2 | 重启后从硬件恢复 25 槽绑定表。 | 未通过：settings/NVS host 证据存在，但“写入 -> 重启 -> 读回”真实设备闭环未完成。 |
| v1 | 硬件、APP 契约、固件质量和回板验证完成，OTA DFU release gate 达成。 | 未通过：回板验证、nRF52832 资源/引脚/功耗、真实 WS2812、NFC、电池 ADC、低功耗和 OTA/DFU 仍缺证据。 |
