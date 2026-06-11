# nRF52 固件

此目录用于智能底盘固件。

首版目标芯片为 nRF52832。nRF52811 是后续降成本备选，nRF52840 仅作为开发验证板或高配实验平台。

## 目标

- 低占空广播、快速广播、已连接、点灯中四态机。
- NT3H2111 FD 引脚唤醒，使用 GPIOTE PORT 事件。
- 绑定表持久化，使用 FDS 写穿。
- 灯效输出使用 PWM + EasyDMA，禁止 GPIO bit-bang。
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
- NFC FD GPIO 唤醒入口。

待接真实硬件：

- 绑定表 settings/NVS 持久化。
- WS2812 PWM + EasyDMA 输出。
- P-MOS 灯条电源门控。
- NT3H2111 I2C/NDEF 写入。
- 电池电量 ADC。
- MCUboot/DFU。

## 构建

安装 nRF Connect SDK 后，在本仓库根目录运行：

```bash
west build -b nrf52dk_nrf52832 firmware/nrf52/app
```

当前开发机未检测到 `west`/`cmake`，所以本轮只做了源码和协议脚本校验，未做 Zephyr 完整编译。

## 第一阶段任务

1. 引入 `../../protocol/viberack_protocol.h`。
2. 建立 GATT 服务：
   - Binding Table Service
   - Light Control Service
   - BAS
   - DIS
   - Secure DFU
3. 实现 `READ_ONE`、`READ_ALL`、`WRITE_ONE`、`SET_QTY`。
4. 实现 `FIND` 和 `OFF`。
5. 加入 table_seq 持久化和全表 CRC。

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
