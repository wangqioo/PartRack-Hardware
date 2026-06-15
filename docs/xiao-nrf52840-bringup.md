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
| WS2812 DATA | D2 | P0.28 | 当前通过 Zephyr `worldsemi,ws2812-spi` 绑定为 SPI2 MOSI 输出。 |
| 灯条 P-MOS 控制 | D3 | P0.29 | 高电平导通，开发期约定。 |
| NFC FD 模拟输入 | D0 | P0.02 | 用按钮、跳线或 NT3H2111 FD 信号触发快速广播。 |
| NT3H2111 SDA | D4 | P0.04 | XIAO 默认 I2C SDA。 |
| NT3H2111 SCL | D5 | P0.05 | XIAO 默认 I2C SCL。 |

当前 overlay:

- `firmware/nrf52/app/boards/xiao_ble_nrf52840_sense.overlay`
- `firmware/nrf52/app/boards/xiao_ble.overlay`
- 共享映射：`firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi`

Zephyr 会在 `xiao_ble/nrf52840/sense` 构建中加载 `xiao_ble_nrf52840_sense.dts`，再叠加本项目的开发期引脚映射。

WS2812 开发期接线：

- XIAO D2 / P0.28 -> WS2812 DIN。
- XIAO D3 / P0.29 -> 灯条 P-MOS 电源控制。
- XIAO GND -> 灯条 GND，必须共地。
- 灯条供电按硬件电源路径接入，不直接用 GPIO 供电。

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

## XIAO BLE 恢复基线

2026-06-15 恢复结论：如果 XIAO nRF52840 Sense 出现“Zephyr/PartRack/官方 BLE app 都扫不到、但裸机 blink 能跑”的状态，优先怀疑 bootloader/SoftDevice 启动链，不要继续盲烧业务固件。

已验证可恢复基线：

- bootloader: Seeed 官方 `UF2 Bootloader 0.6.1`
- Board-ID: `Seeed_XIAO_nRF52840_Sense`
- SoftDevice: `S140 version 7.3.0`
- XIAO serial: `12EBF0B1B70E5B58`
- bootloader USB VID/PID: `2886:0045`
- app USB VID/PID: `2886:8045`
- 正确 XIAO 串口：`/dev/cu.usbmodem11301`

不要把 `/dev/cu.usbmodem0010000001` 当作 XIAO。该端口在本机是 CanMV，VID/PID 为 `1209:ABD1`，序列号为 `001000000`。

已确认的失败路线：

- `update-..._nosd.uf2` 只更新 bootloader，不重写完整 SoftDevice。
- 将 Seeed 完整 `.hex` 转为 UF2 再拖入 `XIAO-SENSE`，会触发设备断开，但不能保证完整写入。
- 块级校验显示，SoftDevice-only UF2 只匹配前 176 个 block；`0xc000..0x26500` 仍不匹配。
- macOS 对 UF2 盘复制时报 `Device not configured` 或 `Input/output error` 不能单独作为成功或失败判断，必须结合 `INFO_UF2.TXT` 和 `CURRENT.UF2` 校验。

已验证的正确恢复命令：

```bash
/Users/wq/Library/Arduino15/packages/Seeeduino/hardware/nrf52/1.1.12/tools/adafruit-nrfutil/macos/adafruit-nrfutil \
  --verbose dfu serial \
  -pkg /Users/wq/Library/Arduino15/packages/Seeeduino/hardware/nrf52/1.1.12/bootloader/Seeed_XIAO_nRF52840_Sense/Seeed_XIAO_nRF52840_Sense_bootloader-0.6.2_s140_7.3.0.zip \
  -p /dev/cu.usbmodem11301 \
  -b 115200 \
  --singlebank
```

成功输出应包含：

```text
Starting DFU upgrade of type 3, SoftDevice size: 152728, bootloader size: 39000, application size: 0
Activating new firmware
Device programmed.
```

恢复后双击 reset，`/Volumes/XIAO-SENSE/INFO_UF2.TXT` 应显示：

```text
UF2 Bootloader 0.6.1 ...
Board-ID: Seeed_XIAO_nRF52840_Sense
SoftDevice: S140 version 7.3.0
Date: Nov 12 2021
```

官方 BLE 基线复测使用 Seeed Arduino core：

```bash
PATH=/private/tmp/pyshim:$PATH arduino-cli upload \
  -p /dev/cu.usbmodem11301 \
  --fqbn Seeeduino:nrf52:xiaonRF52840Sense \
  --input-dir /private/tmp/seeed-xiao-official-bleuart-build
```

成功输出：

```text
Device programmed.
```

手机 nRF Connect 已确认可扫描到官方 Bluefruit BLEUART app，设备名为 `XIAO nRF52840 Sense`。

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

自动烟测会连接 `VBRK-0000`，读取 `Table Info` / `Light Status`，开启 Binding Control Point notify，写入第 1 槽，发送 `READ_ONE` 校验读回的 16B 槽位记录，发送 `READ_ALL` 校验至少返回第 1 槽记录，并发送 `SET_QTY` 校验状态 notify。

2026-06-12 当前本机执行结果：

```text
BLE backend is unavailable: CoreBluetooth reported 'BLE is unsupported'.
```

该错误发生在 macOS CoreBluetooth 后端初始化阶段，尚未进入 BLE 扫描，不能作为固件或设备广播失败判断。

## 2026-06-15 BLE lifecycle 和构建 variant 记录

当前固件将 BLE lifecycle 集中到 host-testable model：

- Binding Table、灯控和 NFC FD 只上报状态变化，不直接调用 Zephyr Bluetooth 广播刷新。
- `main.c` 显式执行 `bt_enable()`、`settings_load()`、初始 advertising start。
- 断开连接后的 advertising restart、connected 状态下的 advertising dirty 延迟、notify 失败不回滚 domain 操作均有 host-side 覆盖。

当前 XIAO 构建分为两个 variant：

```bash
tools/verify_host.sh --bare-build
tools/verify_host.sh --peripheral-build
```

- `--bare-build`：裸 XIAO BLE 验证，不加载外接 WS2812、灯条电源门控和 NFC FD。
- `--peripheral-build`：额外加载 `firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi`，编译 WS2812 SPI、灯条电源门控和 NFC FD adapter。

2026-06-15 本机验证结果：

- `tools/verify_host.sh --host-only` 通过。
- `tools/verify_host.sh --bare-build` 通过，生成 `build-partrack-PartRack-Hardware-xiao-sense-bare`，脚本确认 DTS 不含 PartRack 外设节点。
- `tools/verify_host.sh --peripheral-build` 通过，生成 `build-partrack-PartRack-Hardware-xiao-sense-peripherals`，脚本确认 DTS 含 `vbrk_ws2812`、`vbrk-led-strip`、`vbrk-nfc-fd`。
- peripheral build 编入 `ws2812_spi.c`、`spi_nrfx_spim.c` 和 `nrfx_spim.c`。

这些结果是 host/build 证据，不替代真实 BLE notify、真实 WS2812 灯条、真实 NFC 或功耗实测。

## 2026-06-15 M0 BLE smoke 尝试记录

本机已用项目虚拟环境确认 BLE smoke 依赖存在：

```bash
.venv/bin/python -m pip show bleak
```

结果：`bleak 0.22.3` 已安装在项目 `.venv`。

测试帧生成通过：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --print-vectors
```

输出包含 `WRITE_ONE`、`READ_ONE`、`READ_ALL`、`SET_QTY`、`FACTORY_RESET` 和灯控测试帧。

真实 BLE 自动烟测命令：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke
```

当前本机结果：

```text
BLE backend is unavailable: CoreBluetooth reported 'BLE is unsupported'. This usually means the current terminal/runtime is restricted from using the macOS Bluetooth stack, not that the nRF device failed. Original error: BLE is unsupported
```

该错误仍发生在 CoreBluetooth 后端初始化阶段，尚未进入 `VBRK-0000` 设备扫描，不能作为固件广播、连接或 GATT 失败判断。M0 下一步切到手机 nRF Connect 手工验证：订阅 Binding Control Point notify，执行 `WRITE_ONE -> READ_ONE`、`READ_ALL`、`SET_QTY`，并记录原始 notify 字节。

## 2026-06-16 M0 BLE smoke 通过项和缺口

2026-06-16 已将开发期配对容量从 1 个 peer 扩到 4 个 peer，并启用 key 满时覆盖最旧记录：

```text
CONFIG_BT_MAX_PAIRED=4
CONFIG_BT_KEYS_OVERWRITE_OLDEST=y
```

原因：手机 nRF Connect 已可能占用唯一 pairing key 槽，Mac 端自动烟测作为第二个 peer 连接时，`bt_conn_set_security(conn, BT_SECURITY_L2)` 在固件串口中返回 `-12`。扩大 key 槽后，串口确认：

```text
app_ble: security changed: level 2
```

当前烧录的 bare UF2：

```text
/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-bare/app/zephyr/zephyr.uf2
SHA-256: d008247b146fe80d64e50386706d88ffdc5386db987461fff25d80d6b4a901ae
```

真实 BLE 自动烟测命令：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --run-smoke
```

2026-06-16 本机结果退出码为 0，关键输出：

```text
table_info: 03 00 00 00 9E 54 19
light_status: 00 00 00
binding_notify: 10 00
binding_notify: 01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
binding_notify: 02 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
binding_notify: 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
binding_notify: 30 00
```

已实机验证：

- Mac CoreBluetooth 可扫描并连接 `VBRK-0000`。
- Table Info / Light Status 读取通过。
- Binding Control Point encrypted write 可在 level 2 security 后写入。
- `WRITE_ONE` 返回 `10 00`。
- `READ_ONE slot 1` 返回写入的完整 16B 记录。
- `READ_ALL` 至少返回 slot 1 记录和后续空槽记录。
- `SET_QTY slot 1 -> 42` 返回 `30 00`。

仍需修复：

- 真实 BLE 下 `READ_ALL` 没有收到 `02 00 FF` 结束帧。当前 GATT write callback 内连续发送 26 个 notify，真实 TX 队列可能在第二帧后暂时不可用；固件需要将 `READ_ALL` 改为 paced/asynchronous notify，或在 notify busy 时排队重试。

## 2026-06-12 Binding Table 持久化记录

已完成固件侧 settings/NVS 持久化初版：

- settings key: `vbrk/binding_table`。
- 保存内容：`table_seq` + 25 个 `vbrk_slot_record_t`。
- 保存格式：`magic/version/length/table_seq/records/crc16`。
- CRC 失败、magic/version/length 不匹配时会丢弃 snapshot，并回到空表。
- 写入类操作会先写 flash，保存成功后才替换 RAM 表和递增 `table_seq`。

已验证的本机命令：

```bash
cc -std=c11 -Wall -Wextra -Iprotocol tools/storage_snapshot_test.c \
  protocol/viberack_protocol.c protocol/viberack_storage.c \
  -o /tmp/storage_snapshot_test
/tmp/storage_snapshot_test
python3 tools/protocol_check.py
python3 tools/binding_table_model_test.py
python3 tools/ble_gatt_smoke_test_test.py
```

固件构建命令：

```bash
cd /Users/wq/ncs
ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
  /Users/wq/ncs/.venv/bin/west build -p always -b xiao_ble/nrf52840/sense \
  /Users/wq/PartRack-Hardware/firmware/nrf52/app \
  -d /Users/wq/ncs/build-partrack-xiao-sense
```

最新 UF2：

```text
/Users/wq/ncs/build-partrack-xiao-sense/app/zephyr/zephyr.uf2
```

当前烧录状态：

- 2026-06-12 已复制最新 UF2 到 `/Volumes/XIAO-SENSE/zephyr.uf2`。
- 烧录后 bootloader 盘自动弹出，设备重新枚举为 `/dev/cu.usbmodem1101`。
- 串口读取到 SoftDevice Controller 启动日志。

持久化手工验证步骤：

1. 双击 reset，让系统出现 `/Volumes/XIAO-SENSE`。
2. 复制最新 `zephyr.uf2` 到 `/Volumes/XIAO-SENSE/zephyr.uf2`。
3. 手机 nRF Connect 连接 `VBRK-0000`。
4. 展开 Binding Table Service `7f4b0001-8d1a-4d45-9a4e-2b4a7c000000`。
5. 对 Binding Control Point `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000` 开启 notify。
6. 写入第 1 槽测试记录，格式选择 Hex：

```text
10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

7. 发送 `READ_ONE slot 1`：

```text
01 01
```

期望 notify：

```text
01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

8. 断开连接，重启开发板。
9. 再次连接 `VBRK-0000`，重新开启 Binding Control Point notify。
10. 再发送 `01 01`，期望仍然读回同一条记录。

如果串口可读，重启后应能看到类似日志：

```text
binding table restored: seq=...
```
