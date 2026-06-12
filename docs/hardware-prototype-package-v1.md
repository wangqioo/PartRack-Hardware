# 硬件打样开发包 v1

本文档是首版 nRF52832 智能底盘硬件打样入口。它索引主控板、灯条、结构、BOM 和回板验证资料，并明确哪些内容已经冻结、哪些仍是待决策项。

## 当前目标

- 主控：nRF52832。
- NFC：NT3H2111。
- 灯条：25 x WS2812B。
- 供电：1S 锂电，MCU 走 3.3V LDO，灯条由电池侧经电源门控供电。
- 料盒：纯结构件，磁吸定位，无电子触点。

## 文件索引

| 文件 | 用途 |
|---|---|
| `hardware/main-board/pinmap-nrf52832-v1.md` | nRF52832 目标板引脚草案 |
| `hardware/main-board/schematic-requirements-v1.md` | 主控板原理图需求 |
| `hardware/main-board/bom-prototype-v1.md` | 首批打样 BOM 草案 |
| `hardware/led-strip/led-strip-spec-v1.md` | 25 槽 WS2812 灯条 PCB 约束 |
| `hardware/mechanical/prototype-mechanical-constraints-v1.md` | 底盘、料盒、磁铁和导光结构约束 |
| `hardware/prototype-validation-checklist-v1.md` | 回板验证清单 |

## 已有输入

- `docs/product-brief.md`：产品边界和硬件域定义。
- `docs/development-plan.md`：当前固件和开发板进度。
- `docs/xiao-nrf52840-bringup.md`：XIAO nRF52840 Sense 验证记录。
- `firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi`：开发板引脚和 WS2812 devicetree 绑定。
- `docs/ble-protocol-v0.1.md`：BLE/灯控协议约束。

## 明确分离

XIAO nRF52840 Sense 的 D2/P0.28、D3/P0.29、D0/P0.02、D4/P0.04、D5/P0.05 是开发板验证映射，不是 nRF52832 目标板冻结引脚。目标板以 `hardware/main-board/pinmap-nrf52832-v1.md` 为准。

## 当前未决项

- nRF52832 最终引脚冻结。
- WS2812 在 nRF52832 上使用 SPI、PWM、I2S 还是其它 EasyDMA 路径。
- 灯条电源门控拓扑和控制极性。
- WS2812 数据线是否需要电平转换。
- NT3H2111 FD 极性、天线约束和 NDEF 写入流程。
- LDO、P-MOS、电池 ADC 分压和采样开关具体器件。
- OTA DFU 是否进入 v1 beta，还是只作为 v1 release gate。
