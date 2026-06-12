# nRF52832 主控板引脚草案 v1

本文档定义目标 nRF52832 主控板的引脚分配草案。画板前必须复核 nRF52832 封装、外设冲突、低功耗状态和 SWD/量产烧录需求。

## 设计原则

- 保留 SWDIO/SWCLK 和 RESET 测试点。
- WS2812 DATA 使用可被目标外设稳定驱动的 GPIO。
- NT3H2111 FD 接支持 GPIOTE/PORT 唤醒的 GPIO。
- 电池 ADC 使用 SAADC 可用输入，并通过高阻或开关降低静态泄漏。
- 灯条电源门控默认关闭，复位期间不能误点亮。
- XIAO 开发板引脚只作为验证参考，不能直接照抄。

## 草案表

| 功能 | 目标引脚 | 方向 | 外设/说明 | 状态 |
|---|---|---:|---|---|
| SWDIO | SWDIO | I/O | 调试和量产烧录 | 必须保留 |
| SWDCLK | SWDCLK | I | 调试和量产烧录 | 必须保留 |
| RESET | RESET | I | 复位测试点 | 建议保留 |
| WS2812 DATA | P0.xx | O | SPI/PWM/I2S/EasyDMA 路径待定 | 待冻结 |
| 灯条电源门控 | P0.xx | O | 默认关闭，极性取决于 MOS 拓扑 | 待冻结 |
| NT3H2111 SDA | P0.xx | I/O | I2C/TWI SDA，上拉待定 | 待冻结 |
| NT3H2111 SCL | P0.xx | O | I2C/TWI SCL，上拉待定 | 待冻结 |
| NT3H2111 FD | P0.xx | I | GPIOTE/PORT 唤醒，极性待定 | 待冻结 |
| 电池 ADC | AINx/P0.xx | I | SAADC，分压/开关待定 | 待冻结 |
| UART TX 日志 | P0.xx | O | 开发期可选，量产可不贴 | 可选 |
| UART RX 日志 | P0.xx | I | 开发期可选，量产可不贴 | 可选 |

## XIAO 验证映射

| 功能 | XIAO 映射 | 说明 |
|---|---|---|
| WS2812 DATA | D2 / P0.28 | Zephyr `worldsemi,ws2812-spi` 验证路径 |
| 灯条电源门控 | D3 / P0.29 | 开发期电源门控 GPIO |
| NFC FD | D0 / P0.02 | GPIO 中断入口 |
| I2C SDA | D4 / P0.04 | NT3H2111 计划 SDA |
| I2C SCL | D5 / P0.05 | NT3H2111 计划 SCL |

## 画板前检查

- 确认 WS2812 DATA 引脚支持选定外设。
- 确认电源门控 GPIO 在 reset/default 状态不会误导通。
- 确认 NT3H2111 FD 引脚可从低功耗唤醒。
- 确认 ADC 引脚没有和 NFC、灯条、调试口冲突。
- 确认所有量产测试点可接触。
