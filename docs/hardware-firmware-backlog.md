# 硬件与固件任务清单

## P0 协议冻结

- 生成随机 128-bit 自定义基 UUID。
- 固化 Binding Table Service UUID 和 Light Control Service UUID。
- 定义 READ_ALL 结束帧格式。
- 定义复位原因 Notify 载荷。
- 给 APP 仓库提供协议测试向量。

## P0 固件最小闭环

- 确认固件工具链：nRF Connect SDK / Zephyr 优先，nRF5 SDK 作为备选。
- BLE 广播：设备名、厂商字段、table_seq、电量、状态位。初版代码已写，待编译验证。
- NFC FD 唤醒：GPIO 中断 + 50ms 去抖。初版代码已写，待板级验证。
- 绑定表：25 条 16B 记录。内存模型已写，settings/NVS 持久化待做。
- 操作码：`READ_ONE`、`READ_ALL`、`WRITE_ONE`、`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`SET_QTY`。初版代码已写，待编译和联调。
- 灯控：`FIND`、`PICK`、`SORT`、`STOCK_IN`、`OFF`。协议调度已写，WS2812 实际输出待做。
- 灯效输出：PWM + EasyDMA。
- 超时熄灯：独立于 BLE 连接状态。

## P0 硬件打样

- 主控板原理图。
- 主控首版按 nRF52832 设计，预留 nRF52811 降成本评估记录。
- PWM DATA 引脚映射确认。
- NT3H2111 I2C 与 FD 引脚连接确认。
- P-MOS 灯条整断电电路。
- 电池、LDO、测试点和调试接口。
- 25 槽灯条 PCB。
- 底盘凸台节距与导光结构。

## P1 固件完整协议

- `INSERT_AT`。
- `REMOVE_AT`。
- `MOVE_BLOCK`。
- `SORT`。
- `PICK`。
- `STOCK_IN`。
- Table Info：`table_seq`、全表 `crc16`、`slot_count`。
- Secure DFU。
- 看门狗和复位原因上报。

## P1 联调工具

- BLE 模拟器：模拟底盘广播和 GATT 行为。
- 协议帧测试向量。
- 灯控掩码可视化小工具。
- FDS 绑定表随机操作一致性测试。

## P1 硬件验证

- 25 盒满排偏移测试。
- 灯条 3.7V 亮度一致性测试。
- 断电静态电流测试。
- BLE 四状态平均电流测试。
- 磁铁强度 A/B 测试。

## P2 共享取料坞

- 光电对管正交计数。
- HX711 称重。
- USB 供电。
- 取料扣账协议。
- 料带节距和封装换算表。
