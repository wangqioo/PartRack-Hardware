# 开发计划

本计划只覆盖硬件、固件、BLE 协议和 APP 对接契约。Android APP 本体由 `Yrd980/LCSC_android_erp` 仓库维护。

## 当前决策

- 首版主控：nRF52832。
- 后续降成本备选：nRF52811。
- 当前开发验证板：Seeed Studio XIAO nRF52840 Sense，使用 nRF Connect SDK / Zephyr 的 `xiao_ble/nrf52840/sense` board target。
- 高配实验/余量验证：nRF52840。
- 固件工具链：优先 nRF Connect SDK / Zephyr。
- XIAO nRF52840 资料、引脚和烧录路径沉淀在 [xiao-nrf52840-bringup.md](xiao-nrf52840-bringup.md)。
- 当前 XIAO 恢复基线：Seeed 官方 bootloader `0.6.1` + SoftDevice `S140 7.3.0`；救援时使用 Seeed 官方 `softdevice_bootloader` 串口 DFU 包，不使用 `0.11.0_nosd.uf2` 作为恢复方案。
- 已开发但待实机补证的功能集中记录在 [pending-verification-plan.md](pending-verification-plan.md)，验证事实状态以 [verification-matrix.md](verification-matrix.md) 为准。
- 本仓库不开发 APP 页面、Room 数据库、BOM 匹配、扫码入库、二维码打印和库存导入导出。

## 当前进度快照

已完成：

- 仓库已同步到 GitHub：`wangqioo/PartRack-Hardware`。
- nRF Connect SDK / Zephyr 本机工具链已可构建 XIAO nRF52840 Sense 固件。
- 已通过 UF2 烧录到 Seeed Studio XIAO nRF52840 Sense。
- 2026-06-15 已将 XIAO 启动链恢复到 Seeed 官方 `UF2 Bootloader 0.6.1` + `S140 7.3.0`，并用官方 Bluefruit BLEUART app 验证手机可扫描到 `XIAO nRF52840 Sense`。
- 官方 Zephyr `peripheral_hr` 样例已在同一开发板上验证，手机可扫描到 `Zephyr Heartrate Sensor`。
- PartRack 固件已实现并实机验证 BLE 广播、连接、GATT service discovery、`Table Info` 读取和 `Light Status` 读取。
- 已修复两个 bring-up 问题：
  - `CONFIG_BT_SETTINGS=y` 后必须在 `bt_enable()` 后调用 `settings_load()`。
  - 连接断开后必须重新启动 connectable advertising，否则手机再次扫描不到设备。
- 已加入板载 LED 诊断：
  - 蓝灯快速闪 3 次：固件启动。
  - 红灯闪：初始化或 BLE 启动失败。
  - 绿灯慢闪：固件 ready，正在广播，未连接。
  - 蓝灯常亮：BLE 已连接。
- 已新增电脑侧 BLE/GATT 烟测辅助脚本：`tools/ble_gatt_smoke_test.py`。
- BLE/GATT 烟测脚本已能校验 `WRITE_ONE -> READ_ONE` 的完整 16B 槽位记录、paced `READ_ALL` 结束帧、`SET_QTY` 后读回、Table Info seq/CRC、Light Status 状态变化和单槽重启恢复，并可选执行 `CLEAR_ONE` / `FACTORY_RESET`；2026-06-16 已在 Mac CoreBluetooth/Bleak + XIAO 实机上跑通非破坏性批量验证。
- 已新增本机一键验证脚本：`tools/verify_host.sh`。
- 已新增验证证据台账：[verification-matrix.md](verification-matrix.md)，后续 host/model/build/hardware 证据状态以该台账为准。
- 已实现 BLE lifecycle host model：
  - BLE 初始化、连接状态、广播 dirty 状态、断开后重启广播、运行期刷新失败保留 dirty 状态均有 host-side 覆盖。
  - Binding Table、灯控和 NFC FD 只上报状态变化，不直接调用 Zephyr Bluetooth 广播刷新。
  - Binding Control Point 结果 notify 保持同步顺序，成功变更后再报告 Table Info 变化。
- 已实现 Binding Table settings/NVS 持久化：
  - 25 个槽位和 `table_seq` 会编码为带 magic/version/CRC16 的 snapshot。
  - 固件启动时通过 Zephyr settings 读取 `vbrk/binding_table`。
  - 写入类操作会先保存到 flash，保存成功后才更新 RAM 和递增 `table_seq`。
  - 已加入 host-side snapshot 单测。
- 已实现灯条电源门控初版：
  - D3/P0.29 作为开发期 P-MOS 控制，非 OFF 灯控命令拉高，OFF/超时拉低。
  - D2/P0.28 作为 WS2812 DATA，已路由为 SPI2 MOSI 输出。
  - 灯控 timeout/default/FX cap 逻辑已抽到 host-side 可测试策略模块。
- 已实现灯控 RGB 帧生成：
  - 根据 `mask_a/mask_b` 生成 25 槽 RGB 帧。
  - `color_b` 会覆盖同槽位的 `color_a`，用于表达更高优先级。
  - 超出 25 槽的 mask bit 会忽略。
  - 已加入 host-side light frame 单测。
- 已接入灯控像素输出后端入口：
  - `vbrk_light_frame_copy_pixels()` 会把 25 槽 RGB frame 转为输出像素数组，并统计 active slot 数。
  - 固件已有可选 Zephyr `led_strip` 输出路径：存在 `vbrk-led-strip` devicetree alias 时调用 `led_strip_update_rgb()`。
- 已完成 XIAO nRF52840 Sense 的 WS2812 devicetree 绑定：
  - 默认 `xiao_ble_nrf52840_sense.overlay` 是裸板 BLE 验证 variant，不加载外接灯条/NFC 外设。
  - `tools/verify_host.sh --peripheral-build` 会额外加载项目共享的 `xiao_ble_part_rack.dtsi`。
  - peripheral build 中 `vbrk_ws2812` 使用 Zephyr 官方 `worldsemi,ws2812-spi` 驱动，25 像素，GRB 顺序，4 MHz SPI，`0x70/0x40` 符号。
  - Zephyr 构建已确认 peripheral variant 编入 `ws2812_spi.c`、`spi_nrfx_spim.c` 和 `nrfx_spim.c`。

当前仍未完成：

- Binding Table 单槽持久化已在 XIAO nRF52840 Sense 完成“写入 -> 单击 reset -> 读回”确认；多槽、移动/删除/清空类操作后的重启恢复仍需补证。
- 灯控已有 GATT 接口、状态框架、电源门控、25 槽 RGB 帧生成和 XIAO 上的 Zephyr WS2812 SPI 输出绑定，仍需接真实 25 颗 WS2812 灯条实测。
- BLE lifecycle 已 host/build verified，encrypted Binding CP、`WRITE_ONE -> READ_ONE`、paced `READ_ALL`、`SET_QTY` 读回和 Light Command 状态闭环已在 XIAO 实机通过；断开重连长稳、APP 侧配对重试和目标 nRF52832 radio 行为仍需复验。
- NFC / NT3H2111、电池 ADC、低功耗和 OTA 仍未接入。

下一步优先级：

1. 做破坏性绑定表窗口：`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`FACTORY_RESET`，并追加重启恢复验证。
2. 烧录灯条电源门控固件，验证 D3/P0.29 会随灯控命令拉高/拉低。
3. 用 APP 侧复验配对/加密重试、`READ_ALL` 结束帧处理和断开重连。
4. 接真实 25 颗 WS2812 灯条，验证 `FIND/PICK/SORT/STOCK_IN/OFF` 的颜色、槽位和超时熄灯。
5. 接入 NT3H2111 I2C / NDEF / FD 唤醒。
6. 回到 nRF52832 目标资源预算和硬件约束验证。

## 给 APP / 手机开发同学的同步

当前 APP 可以开始并行接入 BLE，不需要等最终 nRF52832 硬件回来。开发期先以 XIAO nRF52840 Sense 上的 `VBRK-0000` 为目标设备。

APP 现在可以做：

- 扫描设备名 `VBRK-0000`，后续量产按 `VBRK-` 前缀过滤。
- 解析广播 Manufacturer Data：开发期 Company ID 为 `0xFFFF`，包含 `proto_ver`、`batch_id`、`battery_pct`、`status_flags`、`table_seq`。
- 连接后执行 service discovery，找到 Binding Table Service 和 Light Control Service。
- 读取 `Table Info`，用 `table_seq + crc16 + slot_count` 判断硬件侧绑定表版本。
- 开启 Binding Control Point、Table Info、Light Status 的 notify。
- 通过 Binding Control Point 做 `READ_ALL`，同步硬件侧 25 槽绑定表。
- 通过 `WRITE_ONE` / `READ_ONE` 做单槽写入和读回比对。
- 通过 `SET_QTY` 修改库存数量，并观察 Binding Control Point / Table Info notify。
- 通过 Light Command 下发 `FIND` / `PICK` / `SORT` / `STOCK_IN` / `OFF` 灯控命令。

推荐联调最小闭环：

1. `connectGatt()`。
2. `discoverServices()`。
3. 读取 `Table Info`。
4. 开启 Binding Control Point notify。
5. `WRITE_ONE` 写入 slot 1。
6. `READ_ONE` 读回 slot 1，并比较 16B 槽位记录。
7. `READ_ALL` 同步全表，等待 `02 00 FF` 结束帧。
8. `SET_QTY` 修改数量，并确认状态 notify 和 `table_seq` 变化。
9. 发送 Light Command，并读取或订阅 `Light Status`。

APP 侧需要注意：

- Binding Control Point 当前是 encrypted write；Android 写入前要处理配对/加密，遇到 authentication/encryption 错误后触发系统配对再重试。
- `CLEAR_ONE` 和 `FACTORY_RESET` 属于破坏性测试，只能在测试设备和测试数据上执行。
- `VBRK-0000`、Company ID `0xFFFF`、`batch_id = 1` 都是开发期占位值，APP 不要写死为量产假设。
- 当前 NFC URI、OTA 和电池 ADC 还没有接入，APP 先不要依赖这些能力。
- Binding Table 持久化和 WS2812 灯条输出固件侧已实现，但仍需要真实设备完成“写入 -> 重启 -> 读回”和真实灯条颜色验证。

APP 同学主要看这几份文档：

- [android-ble-integration-guide.md](android-ble-integration-guide.md)：APP 接入流程、权限、UUID、帧格式。
- [ble-protocol-v0.1.md](ble-protocol-v0.1.md)：协议定义源文档。
- `tools/ble_gatt_smoke_test.py --print-vectors`：可直接复用的测试帧样例。

## 阶段 0：协议冻结

目标：让 APP、固件、测试工具对同一份二进制协议达成一致。

已完成：

- 固定自定义 128-bit UUID。
- 固定 Binding Table Service 和 Light Control Service 特征 UUID。
- 固定 `READ_ALL` 结束帧格式。
- 固定 Table Info：`table_seq`、全表 `crc16`、`slot_count`。
- 固定灯控状态 Notify：`mode + remaining_s`。
- 固定基础错误码。
- 输出协议头文件、协议校验脚本和 nRF Connect 手工测试帧。
- 输出 BLE/GATT 烟测脚本的闭环校验逻辑。
- 输出 BLE lifecycle host model 测试。

待办：

- 固定 APP 侧重试策略。
- 在可用 BLE 后端上跑通电脑侧自动 BLE/GATT 烟测。

交付物：

- [ble-protocol-v0.1.md](ble-protocol-v0.1.md)
- `protocol/viberack_protocol.h`
- `tools/protocol_check.py`
- `tools/ble_lifecycle_test.c`

## 阶段 1：固件最小闭环

目标：开发板上完成“广播 -> 连接 -> 写绑定表 -> 读绑定表 -> 点灯命令 -> 超时熄灯”的最小流程。

已完成初版：

- Zephyr app 工程骨架。
- BLE 广播厂商字段。
- Binding Table Service。
- Light Control Service。
- 25 槽绑定表内存模型。
- `READ_ONE`、`READ_ALL`、`WRITE_ONE`、`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`SET_QTY`、`FACTORY_RESET`。
- 灯控模式调度和超时熄灯框架。
- 灯条电源门控：D3/P0.29 高电平上电，OFF/超时断电；D2/P0.28 作为 WS2812 SPI2 MOSI 输出。
- 灯控 RGB 帧生成：`mask_a/mask_b` -> 25 槽 RGB buffer，`color_b` 覆盖 `color_a`。
- 灯控像素输出入口：frame -> 25 槽 RGB pixels -> 可选 Zephyr `led_strip_update_rgb()`。
- XIAO WS2812 SPI 输出绑定：D2/P0.28 作为 SPI2 MOSI，25 像素 GRB，Zephyr 官方 WS2812 SPI 驱动。
- NFC FD GPIO 唤醒入口。
- settings/NVS 绑定表持久化初版。
- 绑定表 snapshot 编码、CRC 校验和损坏数据拒绝测试。
- 灯控策略 host-side 测试：默认 30s、最大 300s、FX 最大 10s、OFF 断电、非法 mode 拒绝。
- 灯控帧 host-side 测试：OFF 清空、A/B mask 着色、B 覆盖 A、越界 bit 忽略。
- 灯控像素 host-side 测试：frame copy、active slot 统计、非法参数和 buffer 长度拒绝。
- BLE lifecycle host-side 测试：NFC FD 事件延迟处理、connected 状态不刷新广播、disconnect 后重启广播、notify 失败不回滚 domain 操作。

已完成实机验证：

- XIAO nRF52840 Sense 可扫描到 `VBRK-0000`。
- 手机 nRF Connect 可连接，连接后板载蓝灯常亮。
- 手机可发现 Binding Table Service 和 Light Control Service。
- `Table Info` 读取通过，实测返回 `0100 0000 2DE4 19`。
- `Light Status` 读取通过，空闲状态返回 `0000 00`。
- 断开连接后设备会恢复广播。
- Mac CoreBluetooth/Bleak 自动烟测已验证 encrypted Binding CP：`WRITE_ONE -> READ_ONE`、paced `READ_ALL` 结束帧和 `SET_QTY` 状态 notify。
- `tools/ble_gatt_smoke_test.py` 已生成并校验 `WRITE_ONE`、`READ_ONE`、`READ_ALL`、灯控命令测试帧。
- `tools/verify_host.sh --bare-build` 已通过裸 XIAO Sense Zephyr build；`--peripheral-build` 已通过外设启用 build。构建通过只证明固件可编译和 DTS 节点存在，不证明真实灯条/NFC 硬件通过。

当前待验证：

- 执行 `WRITE_ONE` 写入槽位，重启设备，再通过 `READ_ONE` 读回同一槽位。
- APP 侧复验 `WRITE_ONE -> READ_ONE`、`READ_ALL` 结束帧和 `SET_QTY`。
- 可破坏测试窗口内执行 `CLEAR_ONE`、`FACTORY_RESET` 的端到端验证。
- 灯控命令实机验证：发送非 OFF 后 D3/P0.29 拉高，发送 OFF 或超时后拉低。

待开发：

- 接真实 WS2812 灯条，并用手机灯控命令验证 Zephyr `led_strip` 驱动输出。
- 再回到 `nrf52dk_nrf52832` 做目标芯片资源、引脚和功耗验证。
- 接入电池 ADC。
- 完成 Android APP 侧 BLE 加密/配对重试策略验证。
- 增加复位原因上报。

交付物：

- `firmware/nrf52/app`

## 阶段 2：灯条和电源控制

目标：完成 WS2812 实际输出和灯条整断电。

已完成：

- 开发期 P-MOS 电源门控：D3/P0.29 高电平导通。
- WS2812 DATA：D2/P0.28 已路由为 SPI2 MOSI。
- 25 槽 RGB 帧生成：固件收到灯控命令后会构建 frame 并记录 active slot 数。
- Zephyr `led_strip` 输出后端入口：DTS 中定义 `vbrk-led-strip` 后，固件会把 25 槽 RGB pixels 交给 `led_strip_update_rgb()`。
- XIAO nRF52840 Sense devicetree 绑定：`worldsemi,ws2812-spi`，25 pixels，GRB，4 MHz SPI。

待办：

- 接线并实测真实 WS2812 灯条：D2 -> DIN，D3 -> 灯条 P-MOS 控制，GND 共地。
- 验证 25 槽 RGB frame 输出到真实 WS2812 灯条。
- 回到 nRF52832 目标硬件时复核 SPI/I2S/PWM 方案；如果官方 SPI 驱动不满足功耗或引脚约束，再回退自研 PWM + EasyDMA。
- 测试 3.7V 灯条亮度和一致性。
- 测试静态断电电流。

## 阶段 3：NFC / NT3H2111

目标：完成 NFC 碰醒和 NDEF URI 写入。

待办：

- 确认 NT3H2111 I2C 地址和引脚。
- 实现 I2C 读写基础驱动。
- 写入 NDEF URI：`lcscerp://device?mac=...&batch=...&ver=1`。
- FD 引脚唤醒低功耗验证。
- NFC 碰醒后快速广播 30 秒。

## 阶段 4：硬件打样

目标：输出可以画板和打样的硬件约束。

待办：

- nRF52832 引脚分配表。
- 主控板原理图需求。
- NT3H2111 I2C / FD / 供电设计。
- WS2812 DATA / P-MOS 控制设计。
- 电池 ADC 分压和低功耗处理。
- SWD 调试口。
- 测试点清单。
- 灯条 PCB 规格。
- 底盘槽距、导光、凸台定位设计。
- 料盒尺寸和磁铁规格。

## 阶段 5：APP 联调

目标：朋友的 APP 仓库可以稳定对接硬件。

待办：

- APP 扫描 `VBRK-XXXX` 并解析广播厂商字段。
- APP NFC URI 定向连接。
- APP 调用 `READ_ALL` 从硬件恢复绑定表。
- APP 调用 `WRITE_ONE` / `SET_QTY` 更新硬件台账。
- APP 调用 `FIND` / `PICK` / `SORT` / `STOCK_IN` / `OFF`。
- 联调错误码和重试策略。

## 阶段 6：OTA / 可靠性

目标：固件可现场升级，异常可定位。

待办：

- MCUboot / DFU 集成。
- 看门狗。
- 复位原因 Notify。
- 绑定表写入掉电测试。
- flash 写入寿命评估。
- 低功耗广播实测。
- 连接参数实测。

## 暂不做

- APP 主体开发。
- 云端多人协作。
- 共享取料坞。
- 湿敏 MSD 完整时效管理。
- 量产降成本到 nRF52811。
