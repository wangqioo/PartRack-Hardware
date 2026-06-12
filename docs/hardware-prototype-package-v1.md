# 硬件打样开发包 v1

本文档是首版 nRF52832 智能底盘硬件打样入口。它索引主控板、灯条、结构、BOM 和回板验证资料，并明确哪些内容已经冻结、哪些仍是待决策项。

当前包是打样准备草案，不是可直接布线、下单或开模的冻结包。所有标为“草案”“待冻结”“待决策”的内容必须在画板、采购或结构制造前复核。

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

## 冻结状态

| 类别 | 当前状态 | 使用限制 |
|---|---|---|
| 产品目标 | 已冻结为 25 槽智能底盘 v1 | 可用于方案沟通 |
| 主控方向 | 首版 nRF52832 | 可用于方案沟通，具体料号仍需 BOM 冻结 |
| BLE/灯控协议 | v0.1 已实现 | 可用于 APP/固件联调 |
| XIAO 引脚 | 仅开发板验证事实 | 不能用于目标板布线 |
| nRF52832 pinmap | 草案，含 `P0.xx` | 不能用于布线或下单 |
| 灯条电源预算 | 待冻结 | 冻结前不能选定 MOS、连接器、走线宽度和电池规格 |
| 结构尺寸 | 待冻结 | 冻结前不能开模或出正式结构加工图 |

## 已有输入

- `docs/product-brief.md`：产品边界和硬件域定义。
- `docs/development-plan.md`：当前固件和开发板进度。
- `docs/xiao-nrf52840-bringup.md`：XIAO nRF52840 Sense 验证记录。
- `firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi`：开发板引脚和 WS2812 devicetree 绑定。
- `docs/ble-protocol-v0.1.md`：BLE/灯控协议约束。

## 明确分离

XIAO nRF52840 Sense 的 D2/P0.28、D3/P0.29、D0/P0.02、D4/P0.04、D5/P0.05 是开发板验证映射，不是 nRF52832 目标板冻结引脚。目标板引脚以 `hardware/main-board/pinmap-nrf52832-v1.md` 作为草案入口；在所有 `P0.xx` 和 `待冻结` 状态清除前，不能用于布线或下单。

## 当前未决项

- nRF52832 最终引脚冻结。
- WS2812 在 nRF52832 上使用 SPI、PWM、I2S 还是其它 EasyDMA 路径。
- 灯条电源门控拓扑和控制极性。
- 25 颗 WS2812 的原型电流预算、峰值电流、走线宽度、连接器额定电流和 bulk capacitance。
- WS2812 数据线是否需要电平转换。
- NT3H2111 FD 极性、天线约束和 NDEF 写入流程。
- LDO、P-MOS、电池 ADC 分压和采样开关具体器件。
- 槽距、料盒外形包络、主控板包络、电池包络、NFC 触碰区、磁铁规格和结构公差。
- OTA DFU 是否进入 v1 beta，还是只作为 v1 release gate。
