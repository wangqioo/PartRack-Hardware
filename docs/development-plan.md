# 开发计划

本计划只覆盖硬件、固件、BLE 协议和 APP 对接契约。Android APP 本体由 `Yrd980/LCSC_android_erp` 仓库维护。

## 当前决策

- 首版主控：nRF52832。
- 后续降成本备选：nRF52811。
- 当前开发验证板：Seeed Studio XIAO nRF52840，使用 nRF Connect SDK / Zephyr 的 `xiao_ble` board target。
- 高配实验/余量验证：nRF52840。
- 固件工具链：优先 nRF Connect SDK / Zephyr。
- XIAO nRF52840 资料、引脚和烧录路径沉淀在 [xiao-nrf52840-bringup.md](xiao-nrf52840-bringup.md)。
- 本仓库不开发 APP 页面、Room 数据库、BOM 匹配、扫码入库、二维码打印和库存导入导出。

## 阶段 0：协议冻结

目标：让 APP、固件、测试工具对同一份二进制协议达成一致。

待办：

- 固定自定义 128-bit UUID。
- 固定 Binding Table Service 和 Light Control Service 特征 UUID。
- 固定 `READ_ALL` 结束帧格式。
- 固定 Table Info：`table_seq`、全表 `crc16`、`slot_count`。
- 固定灯控状态 Notify：`mode + remaining_s`。
- 固定错误码和 APP 重试策略。
- 输出 APP 对接测试向量。

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
- NFC FD GPIO 唤醒入口。

待办：

- 安装 nRF Connect SDK 后，优先完成 XIAO nRF52840 的 `west build -b xiao_ble firmware/nrf52/app` 编译验证。
- 在 XIAO nRF52840 上完成 BLE 广播、GATT 服务和绑定表命令的开发板验证。
- 再回到 `nrf52dk_nrf52832` 做目标芯片资源、引脚和功耗验证。
- 修正编译期 API/配置问题。
- 接入 settings/NVS 持久化。
- 接入电池 ADC。
- 完成 BLE 加密/配对策略验证。
- 增加复位原因上报。

交付物：

- `firmware/nrf52/app`

## 阶段 2：灯条和电源控制

目标：完成 WS2812 实际输出和灯条整断电。

待办：

- 确认 WS2812 DATA 引脚映射。
- 实现 PWM + EasyDMA 编码。
- 实现 P-MOS 电源门控。
- DATA 断电期间保持低电平，避免倒灌。
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
