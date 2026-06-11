# 固件参考项目调研

目标：寻找能直接改造的开源固件，减少从零实现。

## 结论

没有找到一个同时覆盖 nRF52 BLE GATT、FDS/settings 持久化、Secure DFU、WS2812 PWM+DMA、NT3H2111 NFC FD 唤醒的完整固件。

建议采用“官方 BLE 主干 + 小型驱动移植”的路线：

1. 主工程基于 Nordic 官方 nRF Connect SDK / Zephyr BLE Peripheral 类样例。
2. 绑定表持久化使用 Zephyr settings/NVS 或 Nordic 官方存储机制。
3. DFU 使用 MCUboot / Nordic DFU 官方路径。
4. WS2812 参考 Apache-2.0 的 nRF52 PWM 实现，按 nRF52832 首版硬件重写为本项目驱动；后续再评估 nRF52811 兼容。
5. NT3H2111 只参考寄存器、I2C、NDEF 写入和 FD 行为，不直接搬许可证不明代码。

## 候选项目

### WS2812 / PWM / DMA

| 项目 | 地址 | 许可证 | 结论 |
|---|---|---|---|
| `BartMassey/ws2812-nrf52833-pwm` | https://github.com/BartMassey/ws2812-nrf52833-pwm | Apache-2.0 | 最适合参考。用途接近，许可证清楚，但目标是 nRF52833，需要按 nRF52832 首版硬件和本项目灯效封装重写。 |
| `nicokorn/NRF52840_WS2812B` | https://github.com/nicokorn/NRF52840_WS2812B | 未确认 | 可读思路，不建议直接引入。README 明确说明使用 PWM 控 WS2812B，但目标是 nRF52840/IAR。 |
| `Carbon225/nrf52-ws2812` | https://github.com/Carbon225/nrf52-ws2812 | 未声明 | 只作算法参考，不拷贝源码。 |

### NT3H2111 / NFC Tag

| 项目 | 地址 | 许可证 | 结论 |
|---|---|---|---|
| `SiliconLabsSoftware/third_party_hw_drivers_extension` | https://github.com/SiliconLabsSoftware/third_party_hw_drivers_extension | GitHub API 显示 NOASSERTION | 有 NT3H2111 驱动文件，可参考寄存器和 I2C 流程，不直接拷贝。 |
| `robotman2412/esp32-component-i2c-nt3h2111` | https://github.com/robotman2412/esp32-component-i2c-nt3h2111 | 未确认 | ESP32 组件，适合查 NDEF 和寄存器封装思路。 |
| `notaroomba/ember` | https://github.com/notaroomba/ember | 未确认 | 有 NT3H2111 defs，可参考定义，不直接引入。 |

### BLE / 存储 / DFU

个人仓库没有找到足够贴近且维护明确的完整项目。BLE、存储和 DFU 不建议从随机仓库搬，因为这些部分直接影响功耗、连接稳定性、OTA 安全和后续维护。

建议使用官方路线：

- nRF Connect SDK Bluetooth Peripheral/GATT 样例作为 BLE 主干。
- Zephyr settings/NVS 或 Nordic 官方持久化机制保存绑定表和 `table_seq`。
- MCUboot / Nordic Secure DFU 官方样例作为 OTA 主干。

## 推荐固件起点

首选：nRF Connect SDK / Zephyr + nRF52832。

理由：

- 官方维护，BLE、低功耗、DFU、settings/NVS 在同一工具链内。
- 后续更容易接入 CI、west、devicetree、Kconfig。
- 避免 nRF5 SDK 老工程在新电脑和新 SDK 上维护成本高。
- nRF52832 的 Flash/RAM 余量更适合首版调试和 OTA。

备选：nRF5 SDK + SoftDevice。

理由：

- 文档中提到 SoftDevice、FDS、GPIOTE、PWM + EasyDMA，和旧 Nordic SDK 术语一致。
- 如果团队已有 nRF5 SDK 经验或现成 nRF52811 工程，可更快落地。

代价：

- nRF5 SDK 已偏 legacy，新项目长期维护不如 nRF Connect SDK。

## 下一步

1. 确认固件工具链：nRF Connect SDK 还是 nRF5 SDK。
2. 如果选 nRF Connect SDK，创建 `firmware/nrf52/app`：
   - BLE 广播和两个自定义 GATT 服务。
   - settings/NVS 绑定表。
   - PWM/WS2812 驱动模块。
   - NFC FD GPIO 唤醒和 NDEF URI 写入模块。
3. 如果选 nRF5 SDK，创建 `firmware/nrf52/nrf5sdk_app`：
   - SoftDevice BLE。
   - FDS 绑定表。
   - app_pwm/nrfx_pwm 灯效。
   - DFU bootloader 集成说明。
