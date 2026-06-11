# PartRack Hardware

智能物料管理系统的硬件与固件项目仓库。

本仓库只负责和硬件相关的部分：智能底盘整机、nRF52 固件、BLE 协议、硬件设计、结构设计、共享取料坞。Android APP 主体不放在本仓库，APP 侧由外部仓库维护：

```text
https://github.com/Yrd980/LCSC_android_erp.git
```

当前仓库根据两份项目文档初始化：

- `智能物料管理系统_项目技术文档_v1.0`
- `智能底盘BLE接口规格_v0.1`

## 项目范围

本仓库 v1 交付范围：

- 智能底盘硬件：25 槽，首版采用 nRF52832 + NT3H2111 + WS2812B 灯条 + 锂电；nRF52811 作为后续降成本备选。
- 固件：四态机、FDS 绑定表、NFC FD 唤醒、PWM + EasyDMA 灯效、Secure DFU。
- BLE 协议：绑定表服务、灯控服务、标准 BAS/DIS/DFU，作为 APP 对接契约。
- 硬件资料：主控板、灯条 PCB、底盘/料盒结构、打样验证清单。

不在本仓库交付：

- Android APP 页面、Room 数据库、BOM 匹配、扫码入库、二维码打印、库存导入导出。
- 云端台账和多人协作服务。

v1.5/v2 见 [docs/roadmap.md](docs/roadmap.md)。

当前执行计划见 [docs/development-plan.md](docs/development-plan.md)。

当前开发板 bring-up 资料见 [docs/xiao-nrf52840-bringup.md](docs/xiao-nrf52840-bringup.md)。

## 目录

```text
docs/               产品、协议、架构和里程碑文档
firmware/nrf52/     智能底盘固件工程骨架
hardware/           主控板、灯条、结构件资料入口
protocol/           跨端协议常量、帧格式和测试
tools/              本地开发/模拟/校验工具
```

## 开发优先级

1. 固化 BLE 协议和二进制帧测试。
2. 固件实现绑定表、灯控和 READ_ALL/WRITE_ONE 最小闭环。
3. 主控板、灯条 PCB 和结构件打样资料成型。
4. 提供 BLE 模拟器或测试夹具，供 APP 仓库联调。

## 关键约束

- 硬件是槽位绑定关系的单一事实源。
- 结构化槽位操作必须在 MCU 端执行，APP 不自行重编号后覆盖。
- WS2812B 禁止 GPIO bit-bang，固件必须使用 PWM + EasyDMA。
- 绑定表写操作走 FDS 写穿，不能裸调 NVMC。
- 默认 MTU 23 下所有单帧指令必须可达。
