# 待验证计划

本文档记录已经开发、已经构建验证、仍待实机验证和仍未接入的功能。后续每完成一项实机验证，都应同步更新 `docs/verification-matrix.md`。

## 当前硬件基线

- 开发板：Seeed Studio XIAO nRF52840 Sense。
- 当前启动链：Seeed 官方 `UF2 Bootloader 0.6.1` + `S140 7.3.0`。
- 当前 PartRack 裸 XIAO BLE 固件：`/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-bare/app/zephyr/zephyr.uf2`。
- 2026-06-16 烧录后状态：设备可被手机和 Mac BLE smoke 脚本连接，设备名 `VBRK-0000`。
- 2026-06-16 READ_ALL paced/asynchronous notify 修复后 UF2 SHA-256：`53f357028fe87e618ade4e3dd77cf18296b8bdaeef463481108a4e8a2e291541`。
- 当前裸 XIAO variant 不加载外接 WS2812、灯条电源门控和 NFC FD；相关 warning 属于预期。

## 已开发并待实机验证

| 功能 | 已开发内容 | 当前证据 | 待验证步骤 | 通过标准 |
|---|---|---|---|---|
| BLE 扫描和连接回归 | 设备名 `VBRK-0000`、connectable advertising、断开后重启广播。 | 2026-06-15 恢复后手机已能连接；历史记录有扫描、连接和断开后恢复广播。 | 手机 nRF Connect 扫描、连接、断开、再次扫描并连接。 | 每次断开后 10 秒内能重新扫到 `VBRK-0000`，再次连接成功。 |
| GATT service discovery | Binding Table Service 和 Light Control Service 已实现。 | 历史手机 nRF Connect 已发现两个 service。 | 连接后执行 service discovery。 | 能看到 `7f4b0001-...` 和 `7f4b0002-...` 两个 service。 |
| Table Info 读取 | `table_seq + crc16 + slot_count` 特征已实现。 | 历史返回 `0100 0000 2DE4 19`。 | 读取 `7f4b1002-8d1a-4d45-9a4e-2b4a7c000000`。 | 返回 7B；写入绑定表后 `table_seq` 或 CRC 有变化。 |
| Binding Control Point notify | 绑定表命令响应通过 notify 返回。 | 2026-06-16 Mac 自动烟测已收到 `10 00`、`READ_ONE` payload、完整 `READ_ALL` payload/end marker、`30 00` 和 `SET_QTY` 后读回 payload。 | APP 侧开启 `7f4b1001-8d1a-4d45-9a4e-2b4a7c000000` notify 后复验。 | 写命令后能收到对应状态或数据 notify。 |
| `WRITE_ONE -> READ_ONE` | `WRITE_ONE`、`READ_ONE`、槽位记录编码和解码已实现。 | 2026-06-16 Mac 自动烟测已通过：`10 00` 后读回完整 slot 1 记录。 | APP/nRF52832 复验；必要时重复 Mac smoke。 | 先收到成功状态，再收到 `01 00 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18`。 |
| `READ_ALL` | 全表读取已改为 paced/asynchronous notify，并在 notify busy 时重试同一槽。 | 2026-06-16 Mac 自动烟测已通过：收到 slot 1、后续空槽记录和 `02 00 FF`。 | APP 侧复验超时/结束帧处理；nRF52832 目标板复验。 | APP 能同步 25 槽记录并以 `02 00 FF` 判断完成。 |
| `SET_QTY` | 单槽数量修改已实现。 | 2026-06-16 Mac 批量验证已收到 `30 00`，随后 `READ_ONE` 读回数量 `0x002A`。 | APP/nRF52832 复验。 | 收到 `30 00` 状态 notify；再次 `READ_ONE` 时数量为 `0x002A`；Table Info seq 增长。 |
| `CLEAR_ONE` | 单槽清空已实现。 | host/model 和 smoke vectors 已覆盖。 | 破坏性测试窗口内写 `11 01`，再 `READ_ONE slot 1`。 | slot 1 返回空记录，Table Info 变化。 |
| `INSERT_AT` | 插入并移动槽位已实现。 | host/model 已覆盖基础操作。 | 准备两条记录后执行 `INSERT_AT`，再 `READ_ALL`。 | 插入位置及后续槽位顺序符合协议。 |
| `REMOVE_AT` | 删除并前移槽位已实现。 | host/model 已覆盖基础操作。 | 准备多条记录后执行 `REMOVE_AT`，再 `READ_ALL`。 | 删除位置为空或后续记录前移符合协议。 |
| `MOVE_BLOCK` | 块移动已实现。 | host/model 已覆盖基础操作。 | 准备多条记录后执行 `MOVE_BLOCK`，再 `READ_ALL`。 | 槽位顺序符合协议，未影响块外数据。 |
| `FACTORY_RESET` | 全表清空命令已实现。 | host/model 和 smoke vectors 已覆盖。 | 破坏性测试窗口内写 `F0 A5 A5 5A 5A`。 | 全部槽位为空，Table Info 回到空表状态。 |
| settings/NVS 持久化 | `vbrk/binding_table` snapshot、magic/version/CRC16、损坏拒绝已实现。 | `tools/storage_snapshot_test.c` host verified；2026-06-16 XIAO 单击 reset 后 `READ_ONE slot 1` 读回 `C1234567 / qty=42`。 | 多槽、移动/删除/清空类操作后重启恢复；APP/nRF52832 复验。 | 重启后读回对应记录，`table_seq` 不回退。 |
| BLE 加密/配对 | Binding Control Point 配置为 encrypted write；固件连接后请求 `BT_SECURITY_L2`；开发期支持 10 个 paired peers，超出后覆盖最旧 key。 | 2026-06-16 串口记录 `security changed: level 2`，Mac 自动烟测 encrypted write 通过。 | APP 侧实现 authentication/encryption retry，并在 Android 真机复验；量产前重新决定主手机/维修设备/解绑策略。 | 配对后写入成功；APP 侧可据此实现 authentication/encryption retry。 |
| Light Status 读取 | `mode + remaining_s` 特征已实现。 | 历史空闲返回 `0000 00`；2026-06-16 Mac 批量验证 `FIND` 后返回 `01 0A 00`，OFF 后返回 `00 00 00`；`--run-light-timeout` 通过 10s 倒计时到 `00 00 00`。 | APP 侧订阅 notify；真实灯条复验物理熄灯。 | OFF 时返回 `0000 00`；灯控命令后 mode/remaining_s 变化；超时后回 OFF。 |
| Light Command | `FIND/PICK/SORT/STOCK_IN/OFF` 命令解析和状态框架已实现。 | 2026-06-16 XIAO bare 固件无外接灯条状态闭环通过：FIND 后 Light Status 进入 mode 1，OFF 后回 mode 0；10s 超时自动 OFF 已通过 Mac 自动验证。 | 接真实灯条验证颜色和槽位；APP/nRF52832 复验。 | Light Status 进入对应 mode，超时后回到 OFF；真实灯条按槽位亮灭。 |
| 灯条电源门控 | D3/P0.29 高电平上电，OFF/超时断电。 | 代码和 build 存在；裸 variant 当前不加载外设 alias。 | 烧录 peripheral build，万用表测 D3/P0.29。 | 非 OFF 命令拉高，OFF 或超时后拉低。 |
| WS2812 frame 生成 | `mask_a/mask_b` 生成 25 槽 RGB frame，B 覆盖 A。 | `tools/light_frame_test.c` host verified。 | 接灯条，发送单槽、多槽、A/B 重叠命令。 | 对应槽位颜色正确，B 覆盖 A，越界 bit 不影响 25 槽。 |
| WS2812 Zephyr 输出 | XIAO peripheral build 使用 `worldsemi,ws2812-spi`，25 像素，GRB，4 MHz。 | `tools/verify_host.sh --peripheral-build` 已确认编入驱动。 | 接真实 25 颗 WS2812，烧 peripheral build。 | 灯条按 Light Command 亮灭，颜色和槽位正确。 |
| NFC FD GPIO 唤醒入口 | `vbrk-nfc-fd` GPIO adapter 和 debounce 入口已实现。 | peripheral build 可编入 alias；未接真实 NT3H2111。 | 接 NT3H2111 FD 引脚，触碰 NFC。 | FD 中断触发后固件进入预期唤醒/广播刷新路径。 |
| Device Health Service | `7f4b0003...` service 和 `7f4b3001...` Device Health characteristic 已实现，返回 battery_pct、reset_reason、health_flags。 | `tools/device_health_test.c` host verified；裸 XIAO 和 peripheral build 均已编入 `hwinfo`/watchdog driver；2026-06-16 Mac 读取 Device Health 返回 `64 02 00 00`。 | 补单击 reset、软件 reset、看门狗启用窗口分别采样；目标板电池 ADC 回来后复验。 | payload 4B；reset_reason 与触发方式一致；watchdog enabled/fault flags 合理。 |
| 复位原因上报 | 启动时读取 Zephyr `hwinfo_get_reset_cause()`，映射到协议 bitmask 后清除硬件累计标志。 | `tools/device_health_test.c` 覆盖 payload 编码；Zephyr build verified；2026-06-16 Mac 读取 reset_reason `0x0002`。 | 实机分别做上电、reset pin、软件 reset、看门狗 reset。 | Device Health `reset_reason` 能区分对应原因，日志和 BLE 读取一致。 |
| 看门狗骨架 | Zephyr watchdog driver 已编译；固件有 `CONFIG_VBRK_WATCHDOG_ENABLE` 开关、8s timeout、主循环喂狗。默认不开启。 | 裸 XIAO/peripheral build verified；当前默认不会启动 watchdog。 | 打开开关烧录专用验证固件，正常运行不复位；刻意停止喂狗后复位。 | 正常喂狗稳定运行；异常复位后 Device Health 显示 watchdog bit。 |
| 电池 ADC 软件入口 | `vbrk-battery-adc` devicetree alias 存在时采样 ADC 并换算 0-100%；无 alias 时开发板返回 100。 | `tools/device_health_test.c` 覆盖百分比换算；当前 XIAO 裸板无 alias。 | 目标板分压回来后添加 alias，接电源表/可调电源做标定。 | 电压读数单调，百分比范围合理，低电量广播 flag 触发。 |

## 已做工具和模型但待真实环境补证

| 工具 / 模型 | 已完成内容 | 当前限制 | 后续验证 |
|---|---|---|---|
| `tools/ble_gatt_smoke_test.py` | 生成测试帧；可自动连接、订阅 notify；`--run-smoke` 覆盖基础非破坏性 smoke；`--run-batch` 覆盖 Table Info、`SET_QTY` 读回和 Light Status；`--run-persistence-read` 覆盖重启后读回；`--run-destructive-binding` 覆盖 CLEAR/INSERT/SET_QTY/MOVE/REMOVE/FACTORY_RESET；`--run-light-timeout` 覆盖超时 OFF；`--run-device-health` 覆盖 Device Health 4B 只读检查。 | 2026-06-16 已可在当前 Mac 跑通 `--run-batch`、`--run-persistence-read`、`--run-light-timeout` 和 Device Health 读取；destructive 流程已有 host-side validator，尚未真实 BLE 执行。 | 后续用它做 APP 对照、nRF52832 目标板复验、破坏性命令窗口和灯控超时验证。 |
| BLE lifecycle host model | 覆盖断开后恢复广播、connected 状态延迟刷新、notify 失败不回滚 domain 操作。 | 不证明真实 radio、手机栈、实际 notify 顺序。 | 手机实测断开重连、写入后广播数据刷新和 notify 顺序。 |
| Binding Table host/model tests | 覆盖槽位操作、记录编码、CRC snapshot。 | 不证明 Zephyr settings/NVS 与 flash 实际写入成功。 | 做“写入 -> 重启 -> 读回”真实设备闭环。 |
| Light policy/frame/state tests | 覆盖超时、模式、25 槽 RGB frame 和状态切换。 | 不证明 GPIO、电源门控和真实 WS2812 时序。 | 接真实硬件测 GPIO 和灯条。 |

## 未开发或未完整接入

| 功能 | 当前状态 | 下一步 |
|---|---|---|
| NT3H2111 I2C / NDEF | 尚未完整接入。 | 确认 I2C 地址和引脚，读取 tag memory，写入 `lcscerp://device?...` URI，验证手机触碰路由。 |
| 电池 ADC 标定和目标板 alias | 软件入口已接入；当前 XIAO 裸板没有 `vbrk-battery-adc` alias，仍返回 100%。 | 目标板分压方案回来后添加 devicetree alias，按 Seeed/目标板注意事项处理 ADC 量程、分压和校准曲线。 |
| 低功耗 | 尚未形成实测方案。 | 定义广播、连接、灯条关闭、灯条点亮等电流测试场景，接电源表记录。 |
| OTA / Secure DFU | 尚未集成，是 v1 release gate。 | 决定 beta 是否需要 OTA；集成 MCUboot/DFU，验证升级、失败处理和回滚策略。 |
| nRF52832 目标迁移 | 当前主要在 XIAO nRF52840 Sense 验证。 | 在 `nrf52dk_nrf52832` 或目标板上构建/烧录，复核 Flash/RAM、引脚、功耗和灯条驱动方案。 |
| 看门狗 release 策略 | 软件骨架已接入，默认未启用。 | 定义量产是否开启、timeout、调试暂停策略、异常复位上报和验证窗口。 |

## 推荐验证顺序

1. BLE 回归：扫描、连接、service discovery、Table Info、Light Status。
2. Binding CP notify：开启 notify，执行 `WRITE_ONE -> READ_ONE`。
3. 断开重连：断开后再次扫描连接，确认广播恢复。
4. 破坏性绑定表测试：`CLEAR_ONE`、`INSERT_AT`、`REMOVE_AT`、`MOVE_BLOCK`、`FACTORY_RESET`，并追加重启后恢复验证。
5. APP 侧 `READ_ALL` 复验：确认 APP 能等待 25 槽和 `02 00 FF`，并处理超时重试。
6. 灯控无灯条补证：验证 FIND 超时自动 OFF 和 Light Status notify。
7. 外设 build 验证：烧 peripheral build，测 D3/P0.29 电源门控。
8. 真实 WS2812：接 25 颗灯条，验证颜色、槽位、B 覆盖 A、超时熄灯。
9. NFC FD 和 NT3H2111 NDEF。
10. 电池 ADC、低功耗、OTA/DFU。
11. nRF52832 目标板迁移和回板验证。

## 证据记录格式

每次验证后追加以下信息到 `docs/verification-matrix.md` 对应行：

```text
日期：
固件 UF2 路径 / commit：
设备：
启动链：
接线：
测试工具：
操作步骤：
原始结果：
结论：
剩余风险：
```

当前已知固件路径：

```text
/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-bare/app/zephyr/zephyr.uf2
/Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense-peripherals/app/zephyr/zephyr.uf2
```
