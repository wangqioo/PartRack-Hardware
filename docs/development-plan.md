# 开发计划

本计划只覆盖硬件、固件、BLE 协议和 APP 对接契约。Android APP 本体由 `Yrd980/LCSC_android_erp` 仓库维护。

## 当前决策

- 首版主控：nRF52832。
- 后续降成本备选：nRF52811。
- 当前开发验证板：Seeed Studio XIAO nRF52840 Sense，使用 nRF Connect SDK / Zephyr 的 `xiao_ble/nrf52840/sense` board target。
- 高配实验/余量验证：nRF52840。
- 固件工具链：优先 nRF Connect SDK / Zephyr。
- XIAO nRF52840 资料、引脚和烧录路径沉淀在 [xiao-nrf52840-bringup.md](xiao-nrf52840-bringup.md)。
- 本仓库不开发 APP 页面、Room 数据库、BOM 匹配、扫码入库、二维码打印和库存导入导出。

## 当前进度快照

已完成：

- 仓库已同步到 GitHub：`wangqioo/PartRack-Hardware`。
- nRF Connect SDK / Zephyr 本机工具链已可构建 XIAO nRF52840 Sense 固件。
- 已通过 UF2 烧录到 Seeed Studio XIAO nRF52840 Sense。
- 官方 Zephyr `peripheral_hr` 样例已在同一开发板上验证，手机可扫描到 `Zephyr Heartrate Sensor`。
- PartRack 固件已实现并实机验证 BLE 广播、连接、GATT service discovery、`Table Info` 读取和 `Light Status` 读取。
- 已修复两个 bring-up 问题：
  - `CONFIG_BT_SETTINGS=y` 后必须在 `bt_enable()` 后调用 `settings_load()`。
  - 连接断开后必须重新启动 connectable advertising，否则手机再次扫描不到设备。
- 已加入板载 LED 诊断：
  - 蓝灯快速闪 3 次：固件启动。
  - 红灯闪：初始化或 BLE 启动失败。
  - 绿灯慢闪：固件 ready，正在广播，未连接。
  - 蓝灯常亮：BLE 已连接。
- 已新增电脑侧 BLE/GATT 烟测辅助脚本：`tools/ble_gatt_smoke_test.py`。
- BLE/GATT 烟测脚本已能校验 `WRITE_ONE -> READ_ONE` 的完整 16B 槽位记录和 `READ_ALL` 结束帧；当前本机运行真实 BLE 烟测阻塞在 macOS CoreBluetooth 后端不可用，未进入设备扫描阶段。
- 已实现 Binding Table settings/NVS 持久化：
  - 25 个槽位和 `table_seq` 会编码为带 magic/version/CRC16 的 snapshot。
  - 固件启动时通过 Zephyr settings 读取 `vbrk/binding_table`。
  - 写入类操作会先保存到 flash，保存成功后才更新 RAM 和递增 `table_seq`。
  - 已加入 host-side snapshot 单测。
- 已实现灯条电源门控初版：
  - D3/P0.29 作为开发期 P-MOS 控制，非 OFF 灯控命令拉高，OFF/超时拉低。
  - D2/P0.28 作为 WS2812 DATA，当前保持低电平，避免未实现数据输出时倒灌。
  - 灯控 timeout/default/FX cap 逻辑已抽到 host-side 可测试策略模块。

当前仍未完成：

- Binding Table 的 `WRITE_ONE -> READ_ONE` 还需要完整 notify 闭环验证。
- Binding Table 持久化固件已烧录到 XIAO nRF52840 Sense，仍需做“写入 -> 重启 -> 读回”的手机实机确认。
- 灯控已有 GATT 接口、状态框架和电源门控，还没有真实 WS2812 数据输出。
- NFC / NT3H2111、电池 ADC、低功耗和 OTA 仍未接入。

下一步优先级：

1. 使用手机 nRF Connect 手工验证 Binding Table 持久化：写入槽位、重启、再次连接读回。
2. 烧录灯条电源门控固件，验证 D3/P0.29 会随灯控命令拉高/拉低。
3. 在可访问本机蓝牙栈的环境中运行 `tools/ble_gatt_smoke_test.py --run-smoke`，完成 Binding Table 真实设备读写闭环验证。
4. 接入 WS2812 PWM + EasyDMA 数据输出。
5. 接入 NT3H2111 I2C / NDEF / FD 唤醒。
6. 回到 nRF52832 目标资源预算和硬件约束验证。

## 阶段 0：协议冻结

目标：让 APP、固件、测试工具对同一份二进制协议达成一致。

已完成：

- 固定自定义 128-bit UUID。
- 固定 Binding Table Service 和 Light Control Service 特征 UUID。
- 固定 `READ_ALL` 结束帧格式。
- 固定 Table Info：`table_seq`、全表 `crc16`、`slot_count`。
- 固定灯控状态 Notify：`mode + remaining_s`。
- 固定基础错误码。
- 输出协议头文件、协议校验脚本和 nRF Connect 手工测试帧。
- 输出 BLE/GATT 烟测脚本的闭环校验逻辑。

待办：

- 固定 APP 侧重试策略。
- 在可用 BLE 后端上跑通电脑侧自动 BLE/GATT 烟测。

交付物：

- [ble-protocol-v0.1.md](ble-protocol-v0.1.md)
- `protocol/viberack_protocol.h`
- `tools/protocol_check.py`

## 阶段 1：固件最小闭环

目标：开发板上完成“广播 -> 连接 -> 写绑定表 -> 读绑定表 -> 点灯命令 -> 超时熄灯”的最小流程。

已完成初版：

- Zephyr app 工程骨架。
- BLE 广播厂商字段。
- Binding Table Service。
- Light Control Service。
- 25 槽绑定表内存模型。
- `READ_ONE`、`READ_ALL`、`WRITE_ONE`、`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`SET_QTY`、`FACTORY_RESET`。
- 灯控模式调度和超时熄灯框架。
- 灯条电源门控：D3/P0.29 高电平上电，OFF/超时断电；D2/P0.28 保持低电平。
- NFC FD GPIO 唤醒入口。
- settings/NVS 绑定表持久化初版。
- 绑定表 snapshot 编码、CRC 校验和损坏数据拒绝测试。
- 灯控策略 host-side 测试：默认 30s、最大 300s、FX 最大 10s、OFF 断电、非法 mode 拒绝。

已完成实机验证：

- XIAO nRF52840 Sense 可扫描到 `VBRK-0000`。
- 手机 nRF Connect 可连接，连接后板载蓝灯常亮。
- 手机可发现 Binding Table Service 和 Light Control Service。
- `Table Info` 读取通过，实测返回 `0100 0000 2DE4 19`。
- `Light Status` 读取通过，空闲状态返回 `0000 00`。
- 断开连接后设备会恢复广播。
- `tools/ble_gatt_smoke_test.py` 已生成并校验 `WRITE_ONE`、`READ_ONE`、`READ_ALL`、灯控命令测试帧。

当前待验证：

- 执行 `WRITE_ONE` 写入槽位，重启设备，再通过 `READ_ONE` 读回同一槽位。
- 真实 BLE 后端可用时，执行 `WRITE_ONE` 写入槽位后，通过 notify 和 `READ_ONE` 读回验证。
- 真实 BLE 后端可用时，执行 `READ_ALL` 结束帧验证。
- `CLEAR_ONE`、`SET_QTY`、`FACTORY_RESET` 的端到端验证。
- 灯控命令实机验证：发送非 OFF 后 D3/P0.29 拉高，发送 OFF 或超时后拉低。

待开发：

- 扩展 BLE/GATT 烟测覆盖 `CLEAR_ONE`、`SET_QTY`、`FACTORY_RESET`。
- 接入 WS2812 PWM + EasyDMA 数据输出。
- 再回到 `nrf52dk_nrf52832` 做目标芯片资源、引脚和功耗验证。
- 修正编译期 API/配置问题。
- 接入电池 ADC。
- 完成 BLE 加密/配对策略验证。
- 增加复位原因上报。

交付物：

- `firmware/nrf52/app`

## 阶段 2：灯条和电源控制

目标：完成 WS2812 实际输出和灯条整断电。

已完成：

- 开发期 P-MOS 电源门控：D3/P0.29 高电平导通。
- DATA 断电期间保持低电平：D2/P0.28 输出低电平。

待办：

- 确认 WS2812 DATA 引脚映射。
- 实现 PWM + EasyDMA 编码。
- 实现 `FIND`、`PICK`、`SORT`、`STOCK_IN`、`OFF` 的真实灯效。
- 测试 3.7V 灯条亮度和一致性。
- 测试静态断电电流。

## 阶段 3：NFC / NT3H2111

目标：完成 NFC 碰醒和 NDEF URI 写入。

待办：

- 确认 NT3H2111 I2C 地址和引脚。
- 实现 I2C 读写基础驱动。
- 写入 NDEF URI：`lcscerp://device?mac=...&batch=...&ver=1`。
- FD 引脚唤醒低功耗验证。
- NFC 碰醒后快速广播 30 秒。

## 阶段 4：硬件打样

目标：输出可以画板和打样的硬件约束。

待办：

- nRF52832 引脚分配表。
- 主控板原理图需求。
- NT3H2111 I2C / FD / 供电设计。
- WS2812 DATA / P-MOS 控制设计。
- 电池 ADC 分压和低功耗处理。
- SWD 调试口。
- 测试点清单。
- 灯条 PCB 规格。
- 底盘槽距、导光、凸台定位设计。
- 料盒尺寸和磁铁规格。

## 阶段 5：APP 联调

目标：朋友的 APP 仓库可以稳定对接硬件。

待办：

- APP 扫描 `VBRK-XXXX` 并解析广播厂商字段。
- APP NFC URI 定向连接。
- APP 调用 `READ_ALL` 从硬件恢复绑定表。
- APP 调用 `WRITE_ONE` / `SET_QTY` 更新硬件台账。
- APP 调用 `FIND` / `PICK` / `SORT` / `STOCK_IN` / `OFF`。
- 联调错误码和重试策略。

## 阶段 6：OTA / 可靠性

目标：固件可现场升级，异常可定位。

待办：

- MCUboot / DFU 集成。
- 看门狗。
- 复位原因 Notify。
- 绑定表写入掉电测试。
- flash 写入寿命评估。
- 低功耗广播实测。
- 连接参数实测。

## 暂不做

- APP 主体开发。
- 云端多人协作。
- 共享取料坞。
- 湿敏 MSD 完整时效管理。
- 量产降成本到 nRF52811。
