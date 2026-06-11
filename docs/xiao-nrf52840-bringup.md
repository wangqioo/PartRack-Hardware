# Seeed XIAO nRF52840 开发板 Bring-up

本文记录 PartRack 当前开发验证板资料和本仓库采用的 nRF Connect SDK / Zephyr 接入方式。

## 开发板定位

当前用 Seeed Studio XIAO nRF52840 系列做固件开发验证。它不是首版目标量产硬件；首版目标主控仍是 nRF52832。XIAO nRF52840 的作用是先跑通 BLE、GATT、绑定表、灯控调度、NFC FD 唤醒入口和 APP 联调流程。

后续回到目标硬件时，需要重新验证：

- nRF52832 Flash/RAM 余量。
- 目标 PCB 引脚映射。
- WS2812 PWM + EasyDMA 时序。
- 低功耗广播、连接和灯条断电电流。
- OTA / DFU 分区和现场升级流程。

## 官方资料

- Seeed Studio XIAO nRF52840 Wiki: <https://wiki.seeedstudio.com/XIAO_BLE/>
- Zephyr XIAO BLE board docs: <https://docs.zephyrproject.org/latest/boards/seeed/xiao_ble/doc/index.html>
- Seeed Arduino board package index: <https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json>
- Seeed nRF52 Arduino core: <https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino>
- XIAO nRF52840 schematic PDF: <https://files.seeedstudio.com/wiki/XIAO-BLE/Seeed_Studio_XIAO_nRF52840_PDF.pdf>
- Zephyr board source: <https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/seeed/xiao_ble>

## 工具链决策

Seeed 官方入门路线偏 Arduino-first，适合快速确认板子、USB、LED、GPIO、I2C 和 BLE 基础功能。

PartRack 正式固件使用 nRF Connect SDK / Zephyr，原因：

- Nordic 当前主推 nRF Connect SDK。
- BLE GATT、settings/NVS、MCUboot/DFU、devicetree、Kconfig 在同一工程体系内。
- 后续更容易从 XIAO nRF52840 开发板迁移到 nRF52832 目标硬件。
- 本项目需要长期维护绑定表、低功耗、OTA 和 APP 对接契约，不适合作为 Arduino sketch 维护。

## Zephyr board target

当前手上开发板为 XIAO nRF52840 Sense，优先使用精确 Sense target：

```bash
west build -b xiao_ble/nrf52840/sense /path/to/PartRack-Hardware/firmware/nrf52/app
```

普通 XIAO nRF52840：

```bash
west build -b xiao_ble /path/to/PartRack-Hardware/firmware/nrf52/app
```

回到 nRF52832 DK：

```bash
west build -b nrf52dk_nrf52832 firmware/nrf52/app
```

## PartRack 开发期引脚映射

| PartRack 信号 | XIAO 引脚 | nRF52840 引脚 | 说明 |
|---|---:|---|---|
| WS2812 DATA | D2 | P0.28 | 后续接 PWM + EasyDMA 输出。 |
| 灯条 P-MOS 控制 | D3 | P0.29 | 高电平导通，开发期约定。 |
| NFC FD 模拟输入 | D0 | P0.02 | 用按钮、跳线或 NT3H2111 FD 信号触发快速广播。 |
| NT3H2111 SDA | D4 | P0.04 | XIAO 默认 I2C SDA。 |
| NT3H2111 SCL | D5 | P0.05 | XIAO 默认 I2C SCL。 |

当前 overlay: `firmware/nrf52/app/boards/xiao_ble.overlay`。Zephyr 会在 `xiao_ble/nrf52840/sense` 构建中加载 `xiao_ble_nrf52840_sense.dts`，再叠加本项目的开发期引脚映射。

## XIAO 常用引脚表

| XIAO 引脚 | nRF52840 引脚 | 常见功能 |
|---|---|---|
| D0 | P0.02 | GPIO / AIN0 |
| D1 | P0.03 | GPIO / AIN1 |
| D2 | P0.28 | GPIO / AIN4 |
| D3 | P0.29 | GPIO / AIN5 |
| D4 | P0.04 | GPIO / SDA / AIN2 |
| D5 | P0.05 | GPIO / SCL / AIN3 |
| D6 | P1.11 | GPIO / UART TX |
| D7 | P1.12 | GPIO / UART RX |
| D8 | P1.13 | GPIO / SPI SCK |
| D9 | P1.14 | GPIO / SPI MISO |
| D10 | P1.15 | GPIO / SPI MOSI |
| NFC1 | P0.09 | NFC |
| NFC2 | P0.10 | NFC |
| USER_LED_R | P0.26 | 板载红灯，低电平点亮 |
| USER_LED_B | P0.06 | 板载蓝灯，低电平点亮 |
| USER_LED_G | P0.30 | 板载绿灯，低电平点亮 |

## 电池和板载外设注意点

- XIAO nRF52840 带 BQ25101 充电管理。
- `P0.14` 是 `READ_BAT_ENABLE`。
- `P0.31` 是电池电压 ADC 输入。
- 读取电池电压时要按 Seeed FAQ 处理 `P0.14`，避免 `P0.31` 输入超限。
- 板载 RGB LED 为 common-anode，低电平点亮。
- NFC1/NFC2 是 NFC 天线引脚，不作为普通 GPIO 规划给 PartRack 开发期信号。

## 烧录方式

UF2 是最简单的开发期烧录方式：

1. 双击 reset，让 XIAO 进入 bootloader。
2. 系统出现 `XIAO BLE` U 盘。
3. 将 `build/zephyr/zephyr.uf2` 拖入该 U 盘。

使用 J-Link/SWD 时，可在 nRF Connect SDK 环境中运行：

```bash
west flash
```

## 当前本机状态

当前本机 NCS workspace：

```text
/Users/wq/ncs
```

已验证命令：

```bash
ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
  /Users/wq/ncs/.venv/bin/west build -b xiao_ble/nrf52840/sense \
  /Users/wq/PartRack-Hardware/firmware/nrf52/app
```

UF2 烧录验证路径：

1. 双击 reset，系统出现 `/Volumes/XIAO-SENSE`。
2. 复制 `build/.../zephyr.uf2` 到 `/Volumes/XIAO-SENSE/zephyr.uf2`。
3. 启动盘自动弹出，设备重新枚举为 Zephyr CDC。

已用官方 Zephyr `samples/bluetooth/peripheral_hr` 对 `xiao_ble/nrf52840/sense` 做过基线验证，手机可扫描到 `Zephyr Heartrate Sensor`。

## 2026-06-11 BLE bring-up 记录

已在 Seeed Studio XIAO nRF52840 Sense 上完成以下验证：

- 官方 Zephyr `samples/bluetooth/peripheral_hr` 可被手机 nRF Connect 扫描到，设备名为 `Zephyr Heartrate Sensor`。
- PartRack 固件可被手机 nRF Connect 扫描到，设备名为 `VBRK-0000`。
- 手机可连接 `VBRK-0000`，连接成功后板载蓝灯常亮。
- 手机可发现两个 PartRack 自定义 GATT service：
  - Binding Table Service: `7f4b0001-8d1a-4d45-9a4e-2b4a7c000000`
  - Light Control Service: `7f4b0002-8d1a-4d45-9a4e-2b4a7c000000`
- `Table Info` 读取通过，实测返回 `0100 0000 2DE4 19`。
- `Light Status` 读取通过，空闲状态返回 `0000 00`。

本轮定位出的两个关键固件点：

- 启用 `CONFIG_BT_SETTINGS=y` 后，需要在 `bt_enable()` 后调用 `settings_load()`，否则广播启动会失败。
- connectable advertising 在建立连接后会停止；断开回调中需要重新调用 `bt_le_adv_start()`，否则一次连接尝试后手机会再次扫描不到设备。

板载 LED 诊断约定：

- 开机蓝灯快速闪 3 次：固件启动。
- 红灯闪：初始化或 BLE 启动失败。
- 绿灯慢闪：固件 ready，正在广播，未连接。
- 蓝灯常亮：BLE 已连接。

电脑侧测试辅助：

```bash
python3 tools/ble_gatt_smoke_test.py --print-vectors
```

如果本机可用 BLE，可安装开发依赖后尝试自动烟测：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke
```

自动烟测会连接 `VBRK-0000`，读取 `Table Info` / `Light Status`，开启 Binding Control Point notify，写入第 1 槽，发送 `READ_ONE` 校验读回的 16B 槽位记录，并发送 `READ_ALL` 校验结束帧 `02 00 FF`。

2026-06-12 当前本机执行结果：

```text
BLE backend is unavailable: CoreBluetooth reported 'BLE is unsupported'.
```

该错误发生在 macOS CoreBluetooth 后端初始化阶段，尚未进入 BLE 扫描，不能作为固件或设备广播失败判断。
