# PartRack Hardware

智能物料管理系统的硬件与固件项目仓库。

本仓库只负责和硬件相关的部分：智能底盘整机、nRF52 固件、BLE 协议、硬件设计、结构设计、共享取料坞。Android APP 主体不放在本仓库，APP 侧由外部仓库维护：

```text
https://github.com/Yrd980/LCSC_android_erp.git
```

当前执行计划见 [docs/development-plan.md](docs/development-plan.md)，验证事实状态以 [docs/verification-matrix.md](docs/verification-matrix.md) 为准。

## 当前状态

截至 2026-06-16，M0 在 Seeed Studio XIAO nRF52840 Sense + Mac CoreBluetooth/Bleak 范围内已经基本打通：

- BLE 扫描、连接、service discovery 已实机通过。
- `Table Info`、Binding Control Point、`WRITE_ONE -> READ_ONE`、paced `READ_ALL`、`SET_QTY` 已实机通过。
- settings/NVS 单槽重启恢复已实机通过。
- Light Command / Light Status、10s 超时自动 OFF 已实机通过。
- Device Health Service 已实机读取，样例 payload：`64 02 00 00`。
- 本机 host/model 测试和 XIAO bare/peripheral Zephyr build 已纳入 `tools/verify_host.sh`。
- 当前已给软件侧烧录 peripheral 完整版 XIAO UF2：
  `/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-peripherals/app/zephyr/zephyr.uf2`
- 当前 peripheral UF2 SHA-256：
  `25514abd08d93a7154a704e4b9a151acb4f7823dca6617c8cb043741ea972689`

当前阶段已经进入：

1. 破坏性绑定表实机验证。
2. Android APP BLE 联调。
3. 真实 WS2812 灯条、电源门控、NFC 和 nRF52832 目标板验证。

注意：XIAO nRF52840 Sense 的实机证据不能自动外推为 nRF52832 目标板或量产硬件证据。

## 软件侧交接

软件/Android 同学现在可以基于已烧录的 `VBRK-0000` 设备继续开发 BLE Central 逻辑。

优先测试：

1. 扫描并连接 `VBRK-0000`。
2. `discoverServices()` 后确认 Binding Table、Light Control、Device Health 三个 service。
3. 处理 Binding Control Point encrypted write 的配对/加密重试。
4. 开启 Binding CP notify，跑 `WRITE_ONE -> READ_ONE`。
5. 执行 `READ_ALL`，等待 `02 00 FF` 结束帧。
6. 执行 `SET_QTY`，确认 notify 和 `Table Info.table_seq` 变化。
7. 下发 Light Command，读取或订阅 `Light Status`。
8. 读取 Device Health，当前实测样例 `64 02 00 00`。

暂时不要依赖：

- NFC URI / NT3H2111 NDEF。
- OTA / Secure DFU。
- 真实电池 ADC 百分比。
- 未接灯条时的真实 WS2812 槽位颜色。
- 未进入测试窗口时的破坏性绑定表命令。

## 项目范围

本仓库 v1 交付范围：

- 智能底盘硬件：25 槽，首版采用 nRF52832 + NT3H2111 + WS2812B 灯条 + 锂电；nRF52811 作为后续降成本备选。
- 固件：四态机、settings/NVS 绑定表、NFC FD 唤醒、WS2812 硬件外设灯效、Secure DFU。
- BLE 协议：绑定表服务、灯控服务、标准 BAS/DIS/DFU，作为 APP 对接契约。
- 硬件资料：主控板、灯条 PCB、底盘/料盒结构、打样验证清单。

不在本仓库交付：

- Android APP 页面、Room 数据库、BOM 匹配、扫码入库、二维码打印、库存导入导出。
- 云端台账和多人协作服务。

v1.5/v2 见 [docs/roadmap.md](docs/roadmap.md)。

当前开发板 bring-up 资料见 [docs/xiao-nrf52840-bringup.md](docs/xiao-nrf52840-bringup.md)。

Android APP 对接指南见 [docs/android-ble-integration-guide.md](docs/android-ble-integration-guide.md)。

开发期 BLE/GATT 烟测脚本依赖见 [requirements-dev.txt](requirements-dev.txt)。

本机一键验证脚本：`tools/verify_host.sh`。

## 常用命令

主机侧协议、模型和脚本测试：

```bash
tools/verify_host.sh --host-only
```

构建 XIAO 裸板 BLE 验证固件：

```bash
tools/verify_host.sh --bare-build
```

构建 XIAO 外设 variant：

```bash
tools/verify_host.sh --peripheral-build
```

打印 BLE/GATT 测试帧：

```bash
python3 tools/ble_gatt_smoke_test.py --print-vectors
```

电脑连接真实设备做非破坏性 BLE 批量验证：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --run-batch
.venv/bin/python tools/ble_gatt_smoke_test.py --run-persistence-read
.venv/bin/python tools/ble_gatt_smoke_test.py --run-light-timeout
.venv/bin/python tools/ble_gatt_smoke_test.py --run-device-health
```

破坏性绑定表验证会清空或移动测试数据，只在明确测试窗口内执行：

```bash
.venv/bin/python tools/ble_gatt_smoke_test.py --run-destructive-binding
```

## 目录

```text
docs/               产品、协议、架构和里程碑文档
firmware/nrf52/     智能底盘固件工程骨架
hardware/           主控板、灯条、结构件资料入口
protocol/           跨端协议常量、帧格式和测试
tools/              本地开发/模拟/校验工具
```

## 当前开发优先级

1. 跑破坏性绑定表窗口：`CLEAR_ONE`、`INSERT_AT`、`MOVE_BLOCK`、`REMOVE_AT`、`FACTORY_RESET`，并补重启恢复证据。
2. Android APP 侧复验扫描、连接、配对/加密重试、Binding CP notify、`READ_ALL` 结束帧和断开重连。
3. 烧录 peripheral variant，验证 D3/P0.29 电源门控和 25 颗 WS2812 槽位/颜色/超时熄灯。
4. 接入 NT3H2111 I2C / NDEF / FD 唤醒，验证 NFC 触碰路由。
5. 回到 nRF52832 目标板迁移、功耗、电池 ADC、watchdog release 策略和 OTA/Secure DFU。

## 关键约束

- 硬件是槽位绑定关系的单一事实源。
- 结构化槽位操作必须在 MCU 端执行，APP 不自行重编号后覆盖。
- WS2812B 禁止 GPIO bit-bang，固件必须使用 nRF 硬件外设和 DMA 友好的驱动路径；XIAO 开发板当前使用 Zephyr `worldsemi,ws2812-spi`。
- 绑定表写操作走 Zephyr settings/NVS snapshot，写入成功后再更新 RAM 和 `table_seq`，不能裸调 NVMC。
- 默认 MTU 23 下所有单帧指令必须可达。
