# PartRack 总集成控制台

本文档是 PartRack-Hardware 多轨并行开发的控制面。它不替代详细设计文档，只记录当前批次目标、轨道边界、跨轨冻结项、里程碑和决策。

## 当前版本目标

### 当前打样批次

- 目标硬件：nRF52832 智能底盘主控板 + 25 槽 WS2812 灯条 + NT3H2111 NFC。
- 开发验证板：Seeed Studio XIAO nRF52840 Sense，仅用于固件和协议验证。
- 当前优先闭环：BLE 绑定表读写、灯控命令、灯条电源门控、APP 接入契约、首版硬件打样资料。

### v1 必须交付

- 25 槽智能底盘硬件。
- nRF52 固件四态机。
- BLE 协议 v0.1。
- Binding Table Service。
- Light Control Service。
- NFC NDEF URI 和 FD 唤醒。
- APP 联调测试向量或测试夹具。
- OTA DFU release gate。

### v1 延后但保留

- nRF52811 降成本评估。
- 共享取料坞。该能力属于本仓库长期范围，但不进入当前打样批次。
- 更强配对安全，例如 NDEF passkey 或 Passkey 配对。

## 轨道 A：硬件打样包

- 负责范围：主控板、灯条 PCB、结构约束、BOM、回板验证清单。
- 输入依赖：BLE/灯控/存储约束、XIAO 开发板验证事实、nRF52832 目标限制。
- 交付物：
  - `docs/hardware-prototype-package-v1.md`
  - `hardware/main-board/pinmap-nrf52832-v1.md`
  - `hardware/main-board/schematic-requirements-v1.md`
  - `hardware/main-board/bom-prototype-v1.md`
  - `hardware/led-strip/led-strip-spec-v1.md`
  - `hardware/mechanical/prototype-mechanical-constraints-v1.md`
  - `hardware/prototype-validation-checklist-v1.md`
- 不负责范围：APP 页面、库存业务、BLE opcode 重新设计。
- 验收证据：画板/结构/采购可按文档提出明确问题或进入打样准备。

## 轨道 B：APP BLE 契约包

- 负责范围：广播、GATT、opcode、帧格式、notify、配对/重试、测试向量。
- 冻结项：UUID、16B 槽位记录、17B 灯控帧、7B Table Info、`READ_ALL` 结束帧。
- 可变项：量产 Company ID、设备名后缀、batch 分配方式、NFC URI 细节、OTA 时机。
- 交付物：
  - `docs/app-ble-contract-v0.1.md`
  - `docs/app-ble-test-vectors.md`
  - `docs/app-ble-retry-and-pairing.md`
  - `docs/app-ble-smoke-checklist.md`
  - `protocol/test-vectors.json`
- APP 不应写死的开发期占位：`VBRK-0000`、Company ID `0xFFFF`、`batch_id = 1`。

## 轨道 C：固件质量包

- 负责范围：host-side 测试、协议向量一致性、CI 入口、硬件证据边界。
- 自动测试项：协议编码、绑定表模型、snapshot、灯控策略、灯控帧、BLE smoke 脚本自测。
- 实机验证项：真实 BLE、settings/NVS 重启读回、真实 WS2812、真实 NFC、ADC、低功耗、nRF52832 资源预算。
- 交付物：
  - `docs/firmware-quality-plan.md`
  - host-only verification mode in `tools/verify_host.sh`

## 跨轨接口冻结表

| 接口 | 当前状态 | 变更规则 |
|---|---|---|
| BLE UUID/opcode | v0.1 已实现 | 改动必须同步协议、APP 文档、测试向量和固件 |
| 槽位记录 | 16B + CRC-8/MAXIM | 改动必须出 v0.2 |
| 灯控帧 | 17B Write Without Response | 改动必须出 v0.2 |
| Table Info | 7B `table_seq + crc16 + slot_count` | 改动必须出 v0.2 |
| 存储语义 | Zephyr settings/NVS snapshot | 写入成功后更新 RAM 和 `table_seq` |
| XIAO 引脚 | 开发验证事实 | 不能直接当作 nRF52832 目标 pinmap |
| nRF52832 引脚 | Batch 1 草案 | 画板前必须冻结 |
| NFC URI | v1 目标 | 当前未完成实机闭环 |
| OTA DFU | v1 release gate | 是否进入 beta 仍待决策 |

## Milestone 映射

| Milestone | 目标 | 验收摘要 |
|---|---|---|
| M0 | BLE 绑定表 + 灯控最小闭环 | 无 NFC 前提下完成扫描、连接、Table Info、绑定表读写、灯控状态 |
| M1 | NFC 点亮全链路 | NFC 触发 APP 定向连接并下发 FIND |
| M2 | 绑定表读写与恢复 | 重启后从硬件恢复 25 槽绑定表 |
| M3 | BOM PICK Demo | APP 生成多槽 PICK 掩码，固件稳定执行 |
| v1 Release Gate | 可交付首版 | 硬件、APP 契约、固件质量和回板验证完成 |

## 决策日志

| 日期 | 决策 | 影响文档 | 后续动作 |
|---|---|---|---|
| 2026-06-13 | 四轨并行推进：总集成、硬件打样、APP 契约、固件质量 | `docs/superpowers/specs/2026-06-13-partrack-parallel-development-design.md` | 执行 Batch 1 |
| 2026-06-13 | 增加 M0，避免 NFC 阻塞 BLE/灯控最小闭环 | `docs/milestones.md` | 补 M0 验收清单 |
| 2026-06-13 | 当前存储实现按 Zephyr settings/NVS 表述 | `README.md`, `docs/milestones.md`, `docs/development-plan.md` | 清理旧 FDS 口径 |
