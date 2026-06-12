# PartRack Parallel Development Batch 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first low-risk parallel development batch: integration control, hardware prototype package docs, APP BLE contract docs/vectors, firmware quality plan, and a CI-friendly host verification mode.

**Architecture:** Batch 1 is documentation-first. Each track gets its own files and avoids changing firmware behavior. `tools/verify_host.sh` receives a small mode switch so host-only verification can run without requiring the Zephyr toolchain.

**Tech Stack:** Markdown docs, JSON test vectors, Bash, existing C/Python host tests, Zephyr build command through `west`.

---

## File Map

- `docs/integration-control.md`: project control surface for prototype target, tracks, frozen interfaces, milestones, and decisions.
- `docs/milestones.md`: acceptance-only milestone wording; add M0 and update current storage semantics.
- `README.md`: replace stale current-path storage wording only where it claims FDS as the current implementation.
- `docs/hardware-prototype-package-v1.md`: hardware package index and open-decision list.
- `hardware/main-board/pinmap-nrf52832-v1.md`: target-board pinmap draft, explicitly separate from XIAO pins.
- `hardware/main-board/schematic-requirements-v1.md`: main-board schematic constraints.
- `hardware/main-board/bom-prototype-v1.md`: prototype BOM starter list.
- `hardware/led-strip/led-strip-spec-v1.md`: 25-slot WS2812 strip constraints.
- `hardware/mechanical/prototype-mechanical-constraints-v1.md`: base/bin/magnet/light-window constraints.
- `hardware/prototype-validation-checklist-v1.md`: return-from-fab validation checklist.
- `docs/app-ble-contract-v0.1.md`: Android-facing BLE contract.
- `docs/app-ble-test-vectors.md`: human-readable command and expected-notify vectors.
- `docs/app-ble-retry-and-pairing.md`: Android pairing, retry, reconnect, and timeout guidance.
- `docs/app-ble-smoke-checklist.md`: manual and scripted smoke checklist.
- `protocol/test-vectors.json`: machine-readable vectors for APP and host tests.
- `docs/firmware-quality-plan.md`: evidence boundary and future test plan.
- `tools/verify_host.sh`: add `--host-only` and `--full-build` mode handling while keeping existing default behavior compatible.

## Task 1: Integration Control And Milestones

**Files:**
- Create: `docs/integration-control.md`
- Modify: `docs/milestones.md`
- Modify: `README.md`

- [ ] **Step 1: Create the integration control document**

Create `docs/integration-control.md` with this exact structure and content:

```markdown
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
```

- [ ] **Step 2: Update milestones to add M0 and current storage wording**

Modify `docs/milestones.md` so it contains this content:

```markdown
# 联调里程碑

## M0 BLE 绑定表 + 灯控最小闭环

目标：不依赖 NFC，先完成 APP/手机通过 BLE 连接底盘、读写绑定表并下发灯控命令。

验收：

- APP 或 nRF Connect 能扫描并连接 `VBRK-0000`。
- APP 能读取 `Table Info`。
- `WRITE_ONE` 写入槽位后，`READ_ONE` 能读回同一 16B 记录。
- `READ_ALL` 返回 25 条记录和 `02 00 FF` 结束帧。
- `table_seq` 在写类操作后自增，且通过 Zephyr settings/NVS 持久化。
- APP 能下发 `FIND` 或 `PICK` 灯控帧。
- 固件按超时自动熄灯并关闭灯条电源门控。

## M1 NFC 点亮全链路

目标：硬件碰一下 NFC，APP 弹出或定向连接，连接底盘并点亮指定槽位。

验收：

- NFC NDEF URI 能路由到 APP。
- APP 能按 URI 中的设备标识定向连接。
- APP 下发 `FIND` 灯控帧。
- 固件按超时自动熄灯并关闭灯条电源门控。

## M2 绑定表读写与恢复

目标：固件可靠保存槽位绑定，APP 可通过协议从硬件恢复最小台账。

验收：

- `WRITE_ONE` 写入后通过 Zephyr settings/NVS 保存。
- 设备重启后 `READ_ONE` 能读回写入记录。
- `READ_ALL` 返回 25 条记录和结束帧。
- `table_seq` 自增并持久化。
- APP 能凭 `part_id` 重建物料详情。

## M3 BOM 拣料 Demo

目标：APP 仓库完成 BOM 匹配后，本仓库固件稳定执行多槽位 PICK 掩码。

验收：

- APP 生成 `PICK` 掩码。
- 多底盘按组下发。
- 勾选后重发剩余掩码。
- 完成后下发 `OFF`。

## 仓库边界

APP 页面、Room 数据模型、BOM 匹配、扫码入库和二维码打印由 `Yrd980/LCSC_android_erp` 仓库维护。本仓库只提供硬件、固件和 BLE/NFC 契约。
```

- [ ] **Step 3: Update README storage wording**

In `README.md`, replace this bullet:

```markdown
- 绑定表写操作走 FDS 写穿，不能裸调 NVMC。
```

with:

```markdown
- 绑定表写操作走 Zephyr settings/NVS snapshot，写入成功后再更新 RAM 和 `table_seq`，不能裸调 NVMC。
```

- [ ] **Step 4: Verify Task 1 docs**

Run:

```bash
rg -n "FDS|M0|settings/NVS|integration-control" README.md docs/milestones.md docs/integration-control.md
```

Expected:

- `README.md` has no stale current-path `FDS` bullet.
- `docs/milestones.md` contains `M0 BLE 绑定表 + 灯控最小闭环`.
- `docs/integration-control.md` contains the four tracks and decision log.

- [ ] **Step 5: Commit Task 1**

Run:

```bash
git add README.md docs/milestones.md docs/integration-control.md
git commit -m "docs: add integration control milestones"
```

## Task 2: Hardware Prototype Package

**Files:**
- Create: `docs/hardware-prototype-package-v1.md`
- Create: `hardware/main-board/pinmap-nrf52832-v1.md`
- Create: `hardware/main-board/schematic-requirements-v1.md`
- Create: `hardware/main-board/bom-prototype-v1.md`
- Create: `hardware/led-strip/led-strip-spec-v1.md`
- Create: `hardware/mechanical/prototype-mechanical-constraints-v1.md`
- Create: `hardware/prototype-validation-checklist-v1.md`

- [ ] **Step 1: Create hardware directories**

Run:

```bash
mkdir -p hardware/main-board hardware/led-strip hardware/mechanical
```

- [ ] **Step 2: Create the hardware package entrypoint**

Create `docs/hardware-prototype-package-v1.md`:

```markdown
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
```

- [ ] **Step 3: Create nRF52832 pinmap draft**

Create `hardware/main-board/pinmap-nrf52832-v1.md`:

```markdown
# nRF52832 主控板引脚草案 v1

本文档定义目标 nRF52832 主控板的引脚分配草案。画板前必须复核 nRF52832 封装、外设冲突、低功耗状态和 SWD/量产烧录需求。

## 设计原则

- 保留 SWDIO/SWCLK 和 RESET 测试点。
- WS2812 DATA 使用可被目标外设稳定驱动的 GPIO。
- NT3H2111 FD 接支持 GPIOTE/PORT 唤醒的 GPIO。
- 电池 ADC 使用 SAADC 可用输入，并通过高阻或开关降低静态泄漏。
- 灯条电源门控默认关闭，复位期间不能误点亮。
- XIAO 开发板引脚只作为验证参考，不能直接照抄。

## 草案表

| 功能 | 目标引脚 | 方向 | 外设/说明 | 状态 |
|---|---|---:|---|---|
| SWDIO | SWDIO | I/O | 调试和量产烧录 | 必须保留 |
| SWDCLK | SWDCLK | I | 调试和量产烧录 | 必须保留 |
| RESET | RESET | I | 复位测试点 | 建议保留 |
| WS2812 DATA | P0.xx | O | SPI/PWM/I2S/EasyDMA 路径待定 | 待冻结 |
| 灯条电源门控 | P0.xx | O | 默认关闭，极性取决于 MOS 拓扑 | 待冻结 |
| NT3H2111 SDA | P0.xx | I/O | I2C/TWI SDA，上拉待定 | 待冻结 |
| NT3H2111 SCL | P0.xx | O | I2C/TWI SCL，上拉待定 | 待冻结 |
| NT3H2111 FD | P0.xx | I | GPIOTE/PORT 唤醒，极性待定 | 待冻结 |
| 电池 ADC | AINx/P0.xx | I | SAADC，分压/开关待定 | 待冻结 |
| UART TX 日志 | P0.xx | O | 开发期可选，量产可不贴 | 可选 |
| UART RX 日志 | P0.xx | I | 开发期可选，量产可不贴 | 可选 |

## XIAO 验证映射

| 功能 | XIAO 映射 | 说明 |
|---|---|---|
| WS2812 DATA | D2 / P0.28 | Zephyr `worldsemi,ws2812-spi` 验证路径 |
| 灯条电源门控 | D3 / P0.29 | 开发期电源门控 GPIO |
| NFC FD | D0 / P0.02 | GPIO 中断入口 |
| I2C SDA | D4 / P0.04 | NT3H2111 计划 SDA |
| I2C SCL | D5 / P0.05 | NT3H2111 计划 SCL |

## 画板前检查

- 确认 WS2812 DATA 引脚支持选定外设。
- 确认电源门控 GPIO 在 reset/default 状态不会误导通。
- 确认 NT3H2111 FD 引脚可从低功耗唤醒。
- 确认 ADC 引脚没有和 NFC、灯条、调试口冲突。
- 确认所有量产测试点可接触。
```

- [ ] **Step 4: Create schematic requirements**

Create `hardware/main-board/schematic-requirements-v1.md`:

```markdown
# 主控板原理图需求 v1

## 主控

- nRF52832 为首版主控。
- 预留 SWDIO、SWDCLK、RESET、GND、VCC 测试点。
- 射频、晶振、电源去耦按 Nordic 参考设计执行。
- UART 日志为开发可选接口，量产可不贴或只留测试点。

## 供电

- 电池：1S 锂电。
- MCU 电源：3.3V LDO。
- 灯条电源：电池侧经电源门控供电。
- LDO 选择必须关注静态电流、压差、峰值电流和输入/输出电容要求。

## 灯条电源门控

- 灯条默认断电。
- 复位、下载、低功耗期间不得误导通。
- 明确 MOS 拓扑、栅极默认态、控制极性、泄漏电流、ESD 和上电瞬态。
- 固件逻辑要求：非 OFF 灯控命令打开电源，OFF 或超时关闭电源。

## WS2812 数据

- 25 颗 WS2812B 串行数据。
- 数据线预留串联电阻位置。
- 灯条供电侧预留足够 bulk capacitance。
- 评估 3.3V DATA 驱动电池供电 WS2812 的 VIH 裕量；如不足，加入电平转换。

## NT3H2111

- I2C 接 nRF52832 TWI。
- SDA/SCL 上拉阻值待定。
- FD 接可唤醒 GPIO。
- 明确 FD 极性、电平、去抖和低功耗唤醒约束。
- 预留天线匹配和调试条件。

## 电池 ADC

- 使用 SAADC 输入。
- 分压网络需避免长期静态泄漏；优先考虑采样开关或高阻方案。
- 预留输入保护和滤波。
- 固件需完成标定曲线后才可作为量产电量显示。

## 连接器和测试点

- 灯条连接器至少包含 VBAT_LED、DIN、GND，是否增加备用脚由结构决定。
- 电池接口需防反接或明确装配防呆。
- 必留测试点：VBAT、3V3、GND、LED_PWR、WS2812_DATA、I2C SDA/SCL、NFC FD、ADC 输入、SWDIO、SWDCLK、RESET。
```

- [ ] **Step 5: Create prototype BOM draft**

Create `hardware/main-board/bom-prototype-v1.md`:

```markdown
# 主控板打样 BOM 草案 v1

| 类别 | 器件 | 数量 | 关键要求 | 状态 |
|---|---|---:|---|---|
| MCU | nRF52832 | 1 | BLE, SAADC, TWI, GPIO, 低功耗 | 待选具体料号 |
| NFC | NT3H2111 | 1 | I2C, FD, NDEF | 待确认封装 |
| 灯珠 | WS2812B 或兼容 | 25 | 3.7V 亮度和一致性需实测 | 待选型 |
| LDO | 3.3V LDO | 1 | 低 Iq, 足够 BLE 峰值电流, 低压差 | 待选型 |
| 电源门控 | P-MOS 或负载开关 | 1 | 低泄漏, 默认关断, 电流裕量足够 | 待决策 |
| 电池接口 | 1S 锂电接口 | 1 | 防呆、可靠连接 | 待选型 |
| ADC 分压 | 电阻/开关 | 1 组 | 低泄漏、可标定 | 待决策 |
| 调试 | SWD 测试点/座 | 1 组 | 可量产烧录 | 待布局 |
| 连接器 | 灯条连接器 | 1 | VBAT_LED, DIN, GND | 待结构确认 |
| 结构件 | 磁铁 | 50 | 每料盒 2 颗，强度 A/B 测试 | 待选型 |

## 采购备注

- 所有关键器件需记录供应商、封装、替代料和风险备注。
- 首批样品优先选择可手焊或易返修封装。
- WS2812B、MOS、LDO、磁铁建议至少准备 2 个替代方案。
```

- [ ] **Step 6: Create LED strip spec**

Create `hardware/led-strip/led-strip-spec-v1.md`:

```markdown
# 25 槽 WS2812 灯条规格 v1

## 功能

- 25 颗 WS2812B 对应 25 个槽位。
- bit0 对应 1 号槽，bit24 对应 25 号槽。
- 固件输出 RGB frame，灯珠实际色序按器件和驱动配置确认。

## 电气

- 供电：电池侧经灯条电源门控。
- 信号：单线 DIN，由主控板输出。
- 连接器：至少包含 VBAT_LED、DIN、GND。
- DIN 建议预留串联电阻位置。
- 灯条供电入口预留 bulk capacitance。
- 每颗灯珠按器件建议放置去耦电容。

## 机械

- 灯位必须与 25 个槽位建立一一对应编号。
- 丝印标注槽位方向：1 -> 25。
- 固定孔、定位边和连接器方向需配合底盘结构。
- 灯位中心到料盒可视窗口中心的偏移应可测量。

## 回板验证

- 发送 `FIND` 单槽命令，验证每个槽位编号正确。
- 发送多槽 `PICK` 掩码，验证 mask 到物理灯位映射。
- 验证 OFF 和超时后灯条断电。
- 验证低电量供电下亮度和颜色一致性。
```

- [ ] **Step 7: Create mechanical constraints**

Create `hardware/mechanical/prototype-mechanical-constraints-v1.md`:

```markdown
# 结构打样约束 v1

## 底盘

- 25 槽线性排列。
- 槽位编号必须和灯条编号一致。
- 需要为灯条、主控板、电池和 NFC 天线保留安装空间。
- 需要预留调试和充电/换电操作空间。

## 料盒

- 料盒为纯结构件，不包含电子触点。
- 每个料盒使用 2 颗磁铁定位。
- 料盒插入后不能遮挡对应槽位的可视灯效。

## 磁铁

- 需要测试至少两档磁力。
- 关注手感、取放阻力、相邻槽位影响和对磁敏感物料的风险。
- 结构图需明确磁铁尺寸、极性和装配方向。

## 导光和开窗

- 灯位应能清晰指示对应槽位。
- 需要避免相邻槽位串光造成误判。
- 灯位中心和槽位中心偏移应作为回板验证项记录。

## 验收

- 满 25 盒后，所有料盒可顺畅取放。
- 每个槽位的灯效可被明确识别。
- 底盘移动或轻微震动不应导致料盒脱位。
- NFC 触碰区域应有明确标识，并不被金属或磁铁明显干扰。
```

- [ ] **Step 8: Create prototype validation checklist**

Create `hardware/prototype-validation-checklist-v1.md`:

```markdown
# 回板验证清单 v1

## 上电和调试

- [ ] 目检焊接、方向、短路风险。
- [ ] 测量 VBAT、3V3、GND。
- [ ] SWD 可识别芯片。
- [ ] 固件可烧录。
- [ ] 复位后设备进入预期广播状态。

## BLE 和绑定表

- [ ] 手机可扫描设备。
- [ ] 手机可连接并 discover services。
- [ ] `Table Info` 可读。
- [ ] `WRITE_ONE -> READ_ONE` 通过。
- [ ] `READ_ALL` 返回 25 条记录和 `02 00 FF`。
- [ ] 重启后写入记录仍可读回。

## 灯条

- [ ] 灯条电源默认关闭。
- [ ] 非 OFF 命令打开灯条电源。
- [ ] OFF 命令关闭灯条电源。
- [ ] 超时自动关闭灯条电源。
- [ ] 1-25 槽编号和物理灯位一致。
- [ ] 多槽 PICK 掩码显示正确。

## NFC

- [ ] NT3H2111 I2C 可通信。
- [ ] NDEF URI 可写入。
- [ ] 手机触碰可读取 URI。
- [ ] FD 引脚触发唤醒或快速广播。

## 电源和功耗

- [ ] 灯条开启时电流在预期范围。
- [ ] 灯条关闭后静态电流在预期范围。
- [ ] BLE 广播/连接电流记录。
- [ ] 电池 ADC 读数随电压变化单调。

## 结构

- [ ] 25 盒满排无干涉。
- [ ] 每槽灯位和料盒位置对应。
- [ ] 磁铁吸附力可接受。
- [ ] NFC 触碰区域可被用户自然找到。
```

- [ ] **Step 9: Verify Task 2 docs**

Run:

```bash
rg -n "P0\\.xx|待冻结|待决策|XIAO|nRF52832|WS2812|NT3H2111" docs/hardware-prototype-package-v1.md hardware/main-board hardware/led-strip hardware/mechanical hardware/prototype-validation-checklist-v1.md
```

Expected:

- `pinmap-nrf52832-v1.md` explicitly marks unresolved target pins as `P0.xx`.
- The docs clearly separate XIAO mappings from nRF52832 target decisions.
- Hardware docs mention WS2812, NT3H2111, battery ADC, SWD, and validation.

- [ ] **Step 10: Commit Task 2**

Run:

```bash
git add docs/hardware-prototype-package-v1.md hardware/main-board hardware/led-strip hardware/mechanical hardware/prototype-validation-checklist-v1.md
git commit -m "docs: add hardware prototype package"
```

## Task 3: APP BLE Contract Package

**Files:**
- Create: `docs/app-ble-contract-v0.1.md`
- Create: `docs/app-ble-test-vectors.md`
- Create: `docs/app-ble-retry-and-pairing.md`
- Create: `docs/app-ble-smoke-checklist.md`
- Create: `protocol/test-vectors.json`

- [ ] **Step 1: Create APP BLE contract**

Create `docs/app-ble-contract-v0.1.md`:

```markdown
# APP BLE 契约 v0.1

本文档是 Android APP 对接 PartRack 智能底盘的稳定契约。APP 主体由 `Yrd980/LCSC_android_erp` 维护，本仓库只定义硬件/固件/BLE/NFC 契约。

## 开发期状态

- 当前开发板设备名：`VBRK-0000`。
- 量产扫描建议：按 `VBRK-` 前缀过滤。
- Manufacturer Company ID：开发期 `0xFFFF`。
- `batch_id`：当前开发值 `1`。
- NFC URI、OTA、电池 ADC 尚未完成 APP 可依赖闭环。

## 广播 Manufacturer Data

原始 Manufacturer Specific Data：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 2 | `company_id` | 小端，开发期 `FF FF` |
| 2 | 1 | `proto_ver` | 当前 `0x01` |
| 3 | 2 | `batch_id` | 小端 |
| 5 | 1 | `battery_pct` | 0-100 |
| 6 | 1 | `status_flags` | bit0 低电量，bit1 未绑定槽位，bit2 正在点灯，bit3 故障 |
| 7 | 2 | `table_seq` | 低 16 位，小端 |
| 9 | 2 | reserved | 当前 `00 00` |

Android `ScanRecord.getManufacturerSpecificData(0xFFFF)` 通常返回去掉 Company ID 后的数据：

| Android 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 1 | `proto_ver` |
| 1 | 2 | `batch_id` |
| 3 | 1 | `battery_pct` |
| 4 | 1 | `status_flags` |
| 5 | 2 | `table_seq` |
| 7 | 2 | reserved |

## GATT

### Binding Table Service

- Service UUID: `7f4b0001-8d1a-4d45-9a4e-2b4a7c000000`
- Binding Control Point: `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Write, Notify
  - Permission: encrypted write
- Table Info: `7f4b1002-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Read, Notify

### Light Control Service

- Service UUID: `7f4b0002-8d1a-4d45-9a4e-2b4a7c000000`
- Light Command: `7f4b2001-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Write Without Response
- Light Status: `7f4b2002-8d1a-4d45-9a4e-2b4a7c000000`
  - Properties: Read, Notify

## Table Info

7 字节：

| 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 4 | `table_seq` 小端 |
| 4 | 2 | `crc16` 小端 |
| 6 | 1 | `slot_count`，当前 25 |

## 槽位记录

16 字节：

| 偏移 | 长度 | 字段 | 说明 |
|---|---:|---|---|
| 0 | 1 | `slot` | 1-25，0 表示空/无效 |
| 1 | 10 | `part_id` | ASCII，不足补 `00` |
| 11 | 2 | `qty` | uint16 小端 |
| 13 | 1 | `flags` | bit0 MSD，bit1 低库存，bit2 自定义 |
| 14 | 1 | reserved | 写 `00` |
| 15 | 1 | `crc8` | CRC-8/MAXIM over bytes 0-14 |

## Binding Control Point

响应格式：

```text
op(1B) + status(1B) + payload
```

状态码：

| 值 | 含义 | APP 建议 |
|---:|---|---|
| `0x00` | 成功 | 继续流程 |
| `0x01` | 参数错误 | 检查长度、slot、CRC 或 opcode |
| `0x02` | 槽满 | 提示用户整理槽位 |
| `0x03` | flash 忙 | 延迟后重试 |
| `0x04` | CRC 错误 | 重新生成记录后重试 |

操作码：

| Op | 名称 | 请求载荷 |
|---:|---|---|
| `0x01` | `READ_ONE` | `slot(1B)` |
| `0x02` | `READ_ALL` | 无 |
| `0x10` | `WRITE_ONE` | `record(16B)` |
| `0x11` | `CLEAR_ONE` | `slot(1B)` |
| `0x20` | `INSERT_AT` | `slot(1B) + record(16B)` |
| `0x21` | `REMOVE_AT` | `slot(1B)` |
| `0x22` | `MOVE_BLOCK` | `from(1B) + to(1B) + len(1B)` |
| `0x30` | `SET_QTY` | `slot(1B) + qty(2B)` |
| `0xF0` | `FACTORY_RESET` | `magic(4B) = 5A 5A A5 A5` 小端按线序 |

`READ_ALL` 成功时，固件发送 25 条 notify：

```text
02 00 + record(16B)
```

最后发送结束帧：

```text
02 00 FF
```

## Notify 顺序

写类操作成功并改变绑定表时：

1. Binding Control Point 状态 notify。
2. Table Info notify。

APP 不应假设两个 notify 在同一个 BLE connection event 内到达。

## 灯控

Light Command 固定 17 字节：

| 偏移 | 长度 | 字段 |
|---|---:|---|
| 0 | 1 | `mode` |
| 1 | 4 | `mask_a` 小端 |
| 5 | 4 | `mask_b` 小端 |
| 9 | 3 | `color_a` RGB |
| 12 | 3 | `color_b` RGB |
| 15 | 2 | `timeout_s` 小端 |

模式：

| 值 | 名称 |
|---:|---|
| `0x00` | `OFF` |
| `0x01` | `FIND` |
| `0x02` | `PICK` |
| `0x03` | `SORT` |
| `0x04` | `STOCK_IN` |
| `0x05` | `FX` |

Light Status 固定 3 字节：

```text
mode(1B) + remaining_s(2B little endian)
```

Light Command 是 Write Without Response。APP 下发后应读取或订阅 Light Status，用 `mode + remaining_s` 判断固件是否接受了当前状态。
```

- [ ] **Step 2: Create human-readable test vectors**

Create `docs/app-ble-test-vectors.md`:

```markdown
# APP BLE 测试向量 v0.1

## 槽位记录

写入第 1 槽，`part_id=C1234567`，`qty=12`：

```text
10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18
```

读取第 1 槽：

```text
01 01
```

读取全表：

```text
02
```

`READ_ALL` 结束帧：

```text
02 00 FF
```

## 灯控

`FIND` 第 1 槽，绿色，30 秒：

```text
01 01 00 00 00 00 00 00 00 00 FF 00 00 00 00 1E 00
```

`PICK` 第 1、7、25 槽，绿色，30 秒：

```text
02 41 00 00 01 00 00 00 00 00 FF 00 00 00 00 1E 00
```

`OFF`：

```text
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 破坏性用例

`FACTORY_RESET` 会清空绑定表，只能在测试设备执行：

```text
F0 5A 5A A5 A5
```
```

- [ ] **Step 3: Create pairing and retry guide**

Create `docs/app-ble-retry-and-pairing.md`:

```markdown
# APP BLE 配对与重试策略

## 加密要求

Binding Control Point 当前要求 encrypted write。Android 写入出现 authentication、encryption、insufficient authorization 相关错误时，APP 应触发系统配对或引导用户重新连接后重试。

Light Command 不用于写入绑定表，当前为 Write Without Response。它不替代绑定表写入确认。

## 推荐连接流程

1. 扫描 `VBRK-` 设备。
2. 连接目标设备。
3. 执行 service discovery。
4. 读取 Table Info。
5. 开启 Binding Control Point notify。
6. 开启 Table Info notify。
7. 开启 Light Status notify。
8. 执行 `READ_ALL`。

## 写操作重试

- 参数错误 `0x01`：不自动重试，先检查请求长度、slot、CRC 和 opcode。
- 槽满 `0x02`：不自动重试，提示用户整理槽位。
- flash 忙 `0x03`：等待 300-500ms 后重试，最多 3 次。
- CRC 错误 `0x04`：重新编码记录后重试 1 次。
- 连接断开：重新连接、重新 discover、重新开启 notify，再重发未确认命令。

## READ_ALL 超时

APP 发送 `READ_ALL` 后等待 25 条 `02 00 + record` 和 `02 00 FF` 结束帧。建议总超时 5 秒。超时后重新发送 `READ_ALL`，最多 2 次；仍失败则断开重连。

## Light Command 确认

APP 写 Light Command 后读取或等待 Light Status notify：

- `mode` 应匹配请求模式。
- `remaining_s` 应大于 0，`OFF` 时为 0。
- 如果 1 秒内没有状态变化，APP 可读取 Light Status 再判断。
```

- [ ] **Step 4: Create smoke checklist**

Create `docs/app-ble-smoke-checklist.md`:

```markdown
# APP BLE 烟测清单

## nRF Connect 手工烟测

- [ ] 扫描到 `VBRK-0000`。
- [ ] 连接成功。
- [ ] 发现 Binding Table Service。
- [ ] 发现 Light Control Service。
- [ ] 读取 Table Info。
- [ ] 开启 Binding Control Point notify。
- [ ] 写入 `WRITE_ONE` 测试帧。
- [ ] 发送 `READ_ONE` 并确认返回记录。
- [ ] 发送 `READ_ALL` 并确认结束帧 `02 00 FF`。
- [ ] 发送 Light Command 并读取 Light Status。

## Android APP 烟测

- [ ] Android 12+ 权限申请完整。
- [ ] 扫描回调能解析 manufacturer data。
- [ ] 连接后能 discover services。
- [ ] 遇到加密写失败能触发配对或重试流程。
- [ ] `WRITE_ONE -> READ_ONE` 闭环通过。
- [ ] `READ_ALL` 超时和结束帧处理正确。
- [ ] Light Command 后能用 Light Status 确认。

## 电脑脚本烟测

- [ ] `python3 tools/ble_gatt_smoke_test.py --print-vectors` 可输出测试帧。
- [ ] 可用 BLE 后端环境中再运行真实 `--run-smoke`。
- [ ] macOS CoreBluetooth 后端不可用时，不能把脚本单测当成真实 BLE 通过。
```

- [ ] **Step 5: Create machine-readable vectors**

Create `protocol/test-vectors.json`:

```json
{
  "version": "0.1",
  "slot_count": 25,
  "slot_record_size": 16,
  "light_command_size": 17,
  "binding": {
    "write_one_slot_1_c1234567_qty_12": {
      "hex": "10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18",
      "description": "WRITE_ONE slot 1, part_id C1234567, qty 12"
    },
    "read_one_slot_1": {
      "hex": "01 01",
      "description": "READ_ONE slot 1"
    },
    "read_all": {
      "hex": "02",
      "description": "READ_ALL all 25 records"
    },
    "read_all_end": {
      "hex": "02 00 FF",
      "description": "READ_ALL success end marker"
    },
    "factory_reset": {
      "hex": "F0 5A 5A A5 A5",
      "description": "FACTORY_RESET destructive test command"
    }
  },
  "light": {
    "find_slot_1_green_30s": {
      "hex": "01 01 00 00 00 00 00 00 00 00 FF 00 00 00 00 1E 00",
      "description": "FIND slot 1, green, 30 seconds"
    },
    "pick_slots_1_7_25_green_30s": {
      "hex": "02 41 00 00 01 00 00 00 00 00 FF 00 00 00 00 1E 00",
      "description": "PICK slots 1, 7, 25, green, 30 seconds"
    },
    "off": {
      "hex": "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00",
      "description": "OFF all lights"
    }
  }
}
```

- [ ] **Step 6: Verify Task 3 docs and JSON**

Run:

```bash
python3 -m json.tool protocol/test-vectors.json >/tmp/partrack-test-vectors.pretty.json
rg -n "VBRK-0000|0xFFFF|READ_ALL|02 00 FF|encrypted|Light Status" docs/app-ble-contract-v0.1.md docs/app-ble-test-vectors.md docs/app-ble-retry-and-pairing.md docs/app-ble-smoke-checklist.md
```

Expected:

- `python3 -m json.tool` exits 0.
- Docs mention development placeholders, `READ_ALL`, encryption, and Light Status confirmation.

- [ ] **Step 7: Commit Task 3**

Run:

```bash
git add docs/app-ble-contract-v0.1.md docs/app-ble-test-vectors.md docs/app-ble-retry-and-pairing.md docs/app-ble-smoke-checklist.md protocol/test-vectors.json
git commit -m "docs: add app ble contract package"
```

## Task 4: Firmware Quality Plan And Verify Modes

**Files:**
- Create: `docs/firmware-quality-plan.md`
- Modify: `tools/verify_host.sh`

- [ ] **Step 1: Create firmware quality plan**

Create `docs/firmware-quality-plan.md`:

```markdown
# 固件质量计划

本文档定义 PartRack-Hardware 固件证据边界。host/model/build 通过不能替代真实硬件验证。

## 已有 host 验证

| 文件 | 覆盖 |
|---|---|
| `tools/protocol_check.py` | CRC8、slot mask、16B 槽位记录、17B 灯控帧 |
| `tools/ble_gatt_smoke_test_test.py` | BLE smoke 脚本测试向量和 notify 校验 |
| `tools/binding_table_model_test.py` | `MOVE_BLOCK` Python 模型 |
| `tools/storage_snapshot_test.c` | snapshot magic、version、CRC、损坏拒绝 |
| `tools/light_policy_test.c` | OFF、默认 30s、上限 300s、FX 上限 10s、非法 mode |
| `tools/light_frame_test.c` | mask 着色、B 覆盖 A、越界 bit、active slot 统计 |

## 下一批 host/model 验证

- Binding table core：所有表操作、CRC 拒绝、满表、空槽、重排、`table_seq`。
- Fake persistence：保存失败时 RAM 和 `table_seq` 不变。
- BLE dispatcher model：opcode 长度、状态码、notify payload、`READ_ALL` end marker、Table Info notify。
- Advertising payload：company id、proto_ver、battery、status flags、`table_seq` 低 16 位。
- Light state machine：fake time 下的超时、remaining、OFF、重复命令刷新。

## Zephyr build 验证

`tools/verify_host.sh --full-build` 应运行 host tests、Python tests、`git diff --check` 和 Zephyr build。构建成功只证明代码能编译进目标应用，不证明真实 BLE、灯条、NFC 或功耗行为。

## 必须实机验证

- 真实 BLE 扫描、连接、配对/加密写、notify 时序、断开后重新广播。
- `WRITE_ONE -> 重启 -> READ_ONE` settings/NVS 持久化闭环。
- 真实 WS2812 颜色、槽位、SPI 时序、P-MOS 上下电和电流。
- NT3H2111 I2C、NDEF URI、FD 唤醒和快速广播。
- 电池 ADC 标定和低功耗。
- nRF52832 目标板资源、引脚和功耗预算。

## 证据标签

- Host verified：纯 C/Python 测试通过。
- Model verified：模拟 Zephyr/BLE/时间边界的模型测试通过。
- Zephyr build verified：目标应用构建通过。
- Hardware required：必须在真实设备上补证。
```

- [ ] **Step 2: Replace verify script with mode-aware version**

Replace `tools/verify_host.sh` with:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NCS_DIR="${NCS_DIR:-/Users/wq/ncs}"
BUILD_DIR="${BUILD_DIR:-${NCS_DIR}/build-partrack-xiao-sense}"
BOARD="${BOARD:-xiao_ble/nrf52840/sense}"
MODE="${1:---full-build}"

usage() {
  cat <<'USAGE'
Usage: tools/verify_host.sh [--host-only|--full-build]

  --host-only   Run C/Python host checks and git diff whitespace checks.
  --full-build  Run host checks, then build the Zephyr firmware target.

Default: --full-build
USAGE
}

run_host_checks() {
  cd "${ROOT_DIR}"

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/light_frame_test.c protocol/viberack_protocol.c protocol/viberack_light_frame.c \
    -o /tmp/light_frame_test
  /tmp/light_frame_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/light_policy_test.c protocol/viberack_protocol.c protocol/viberack_light_policy.c \
    -o /tmp/light_policy_test
  /tmp/light_policy_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/storage_snapshot_test.c protocol/viberack_protocol.c protocol/viberack_storage.c \
    -o /tmp/storage_snapshot_test
  /tmp/storage_snapshot_test

  python3 tools/protocol_check.py
  python3 tools/binding_table_model_test.py
  python3 tools/ble_gatt_smoke_test_test.py

  git diff --check
}

run_zephyr_build() {
  cd "${NCS_DIR}"
  ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
    "${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
    "${ROOT_DIR}/firmware/nrf52/app" \
    -d "${BUILD_DIR}"

  echo "UF2: ${BUILD_DIR}/app/zephyr/zephyr.uf2"
}

case "${MODE}" in
  --host-only)
    run_host_checks
    ;;
  --full-build)
    run_host_checks
    run_zephyr_build
    ;;
  -h|--help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
```

- [ ] **Step 3: Verify host-only mode**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected:

- Existing C host tests pass.
- Existing Python host tests pass.
- `git diff --check` passes.
- No Zephyr build is attempted.

- [ ] **Step 4: Verify help output**

Run:

```bash
tools/verify_host.sh --help
```

Expected output includes:

```text
Usage: tools/verify_host.sh [--host-only|--full-build]
```

- [ ] **Step 5: Commit Task 4**

Run:

```bash
git add docs/firmware-quality-plan.md tools/verify_host.sh
git commit -m "docs: add firmware quality plan"
```

## Task 5: Final Batch 1 Review

**Files:**
- Review all files from Tasks 1-4.

- [ ] **Step 1: Run repository status**

Run:

```bash
git status --short --branch
```

Expected:

- Branch is clean after Task 4 commit.

- [ ] **Step 2: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected:

- All host checks pass.

- [ ] **Step 3: Check documentation cross-links**

Run:

```bash
rg -n "docs/integration-control.md|docs/hardware-prototype-package-v1.md|docs/app-ble-contract-v0.1.md|docs/firmware-quality-plan.md|protocol/test-vectors.json" .
```

Expected:

- New documents are referenced from either `docs/integration-control.md`, the plan, or package entrypoints.

- [ ] **Step 4: Check stale storage terminology**

Run:

```bash
rg -n "FDS|settings/NVS|NVMC" README.md docs
```

Expected:

- Any remaining `FDS` mention is historical or explicitly marked non-current.
- Current acceptance wording uses `settings/NVS`.

- [ ] **Step 5: Create final summary commit if needed**

If Task 5 reveals only small doc link fixes, commit them:

```bash
git add README.md docs hardware protocol tools
git commit -m "docs: connect parallel development package"
```

If no changes are needed, do not create an empty commit.

## Self-Review Notes

- Spec coverage: Batch 1 covers integration control, hardware package docs, APP contract docs/vectors, firmware quality plan, and host-only verification mode. Batch 2 test extraction is intentionally deferred and documented.
- Placeholder scan: unresolved hardware decisions are explicitly marked as open decisions, not plan placeholders.
- Type/path consistency: all new file paths match the spec except Batch 2-only test files, which are deferred from this plan.
