# nRF52 固件

此目录用于智能底盘固件。

首版目标芯片为 nRF52832。nRF52811 是后续降成本备选，nRF52840 仅作为开发验证板或高配实验平台。

## 目标

- 低占空广播、快速广播、已连接、点灯中四态机。
- NT3H2111 FD 引脚唤醒，使用 GPIOTE PORT 事件。
- 绑定表持久化，使用 FDS 写穿。
- 灯效输出禁止 GPIO bit-bang。XIAO 开发板当前使用 Zephyr `worldsemi,ws2812-spi`；回到 nRF52832 目标硬件时再按引脚、功耗和资源复核 SPI/I2S/PWM 方案。
- Secure DFU 双区 OTA。

## 推荐起点

优先使用 nRF Connect SDK / Zephyr 创建新工程，BLE、settings/NVS、MCUboot/DFU 都走官方路径。首版工程按 nRF52832 资源预算开发，避免过早卡在 nRF52811 的 Flash/RAM 上。

WS2812 驱动可参考 `BartMassey/ws2812-nrf52833-pwm` 的 Apache-2.0 实现思路，但需要按 nRF52 和本项目灯效接口重写封装。

详细调研见 [../../docs/firmware-reference-survey.md](../../docs/firmware-reference-survey.md)。

## 当前工程

Zephyr app 位于：

```text
firmware/nrf52/app
```

已有模块：

- BLE 广播厂商字段。
- Binding Table Service。
- Light Control Service。
- 25 槽绑定表内存模型。
- `READ_ONE`、`READ_ALL`、`WRITE_ONE`、`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`SET_QTY`、`FACTORY_RESET`。
- 灯控 `OFF` 和模式调度框架。
- 绑定表 settings/NVS 持久化初版。
- 灯控 25 槽 RGB 帧生成、D3/P0.29 电源门控和 Zephyr `worldsemi,ws2812-spi` 输出绑定。
- NFC FD GPIO 唤醒入口。

## 当前开发板

当前优先使用 Seeed Studio XIAO nRF52840 Sense 做开发验证。该板在 Zephyr / nRF Connect SDK 中使用 `xiao_ble/nrf52840/sense` board target；普通 XIAO nRF52840 可使用 `xiao_ble` 或 `xiao_ble/nrf52840` target。

开发板资料、官方链接、完整引脚表和烧录路径见 [../../docs/xiao-nrf52840-bringup.md](../../docs/xiao-nrf52840-bringup.md)。

XIAO nRF52840 开发期引脚映射：

```text
WS2812 DATA       -> D2 / P0.28
灯条 P-MOS 控制   -> D3 / P0.29
NFC FD 模拟输入   -> D0 / P0.02
NT3H2111 I2C SDA  -> D4 / P0.04
NT3H2111 I2C SCL  -> D5 / P0.05
```

当前 XIAO 开发固件使用 `worldsemi,ws2812-spi`：

- D2 / P0.28 被路由为 SPI2 MOSI，只接 WS2812 DIN。
- D3 / P0.29 控制灯条电源门。
- WS2812 配置为 25 pixels、GRB 顺序、4 MHz SPI。
- 若构建 `xiao_ble/nrf52840/sense`，Zephyr 会加载 `boards/xiao_ble_nrf52840_sense.overlay`。

这些映射只用于开发板 bring-up。首版目标主控仍是 nRF52832，回到目标硬件时需要重新确认引脚、功耗和 Flash/RAM 余量。

待接真实硬件：

- 绑定表“写入 -> 重启 -> 读回”实机确认。
- WS2812 真实灯条输出验证。
- NT3H2111 I2C/NDEF 写入。
- 电池电量 ADC。
- MCUboot/DFU。

## 构建

安装 nRF Connect SDK 后，在 NCS workspace 中优先构建 XIAO nRF52840 Sense：

```bash
west build -b xiao_ble/nrf52840/sense /path/to/PartRack-Hardware/firmware/nrf52/app
```

普通 XIAO nRF52840 可使用：

```bash
west build -b xiao_ble /path/to/PartRack-Hardware/firmware/nrf52/app
```

回到 nRF52832 DK 验证时运行：

```bash
west build -b nrf52dk_nrf52832 firmware/nrf52/app
```

当前开发机的 NCS workspace 位于 `/Users/wq/ncs`，已完成 XIAO nRF52840 Sense 目标构建和 UF2 烧录验证。

本机完整验证入口：

```bash
tools/verify_host.sh
```

该脚本会运行协议/模型/烟测向量测试、`git diff --check` 和 XIAO nRF52840 Sense Zephyr 构建。

## 烧录

XIAO nRF52840 最简单的烧录方式是 UF2：

1. 双击板载 reset，让开发板进入 bootloader 模式。
2. 系统出现 `XIAO BLE` U 盘。
3. 将 `build/zephyr/zephyr.uf2` 拖入该 U 盘。

若使用 J-Link/SWD，也可以在 nRF Connect SDK 环境中使用 `west flash`。

## 第一阶段任务

1. Binding Table Service 和 Light Control Service 已建立。
2. `READ_ONE`、`READ_ALL`、`WRITE_ONE`、`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`SET_QTY`、`FACTORY_RESET` 已实现。
3. `FIND`、`PICK`、`SORT`、`STOCK_IN`、`OFF` 灯控帧和状态框架已实现。
4. table_seq 持久化和全表 CRC 已实现。
5. 后续补 BAS、DIS、Secure DFU、NT3H2111 和电池 ADC。

## APP 对接契约

固件不理解物料详情，只保存：

```text
slot -> part_id -> qty -> flags
```

`part_id` 是 APP 数据库主键。APP 负责联网补全物料详情、BOM 匹配和界面展示。

## 红线

- 不直接使用 NVMC 改写绑定表。
- 不在 SoftDevice 运行时 bit-bang WS2812B。
- MOS 只在灯效活动期间导通。
- 熄灯定时器独立于 BLE 连接状态。
