# 固件质量计划

本文档定义 PartRack-Hardware 固件证据边界。host/model/build 通过不能替代真实硬件验证。

当前证据状态的单一事实源见 [verification-matrix.md](verification-matrix.md)。本文只定义证据边界和测试入口。

## 已有 host 验证

| 文件 | 覆盖 |
|---|---|
| `tools/protocol_check.py` | CRC8、slot mask、16B 槽位记录、17B 灯控帧 |
| `tools/storage_snapshot_test.c` | snapshot magic、version、CRC、损坏拒绝 |
| `tools/adv_payload_test.c` | 广播 manufacturer data 的 company id、proto_ver、battery、status flags、`table_seq` 低 16 位 |
| `tools/binding_table_core_test.c` | 绑定表 write/read/clear/insert/remove/move/set qty/factory reset、`table_seq`、CRC 拒绝、保存失败原子性 |
| `tools/ble_dispatcher_model_test.c` | Binding Control Point opcode 长度、状态码、notify payload、`READ_ALL` end marker、Table Info notify |
| `tools/ble_lifecycle_test.c` | BLE lifecycle 状态、广播 dirty/coalesce、connected/deconnected 行为、notify 在线规则 |
| `tools/light_policy_test.c` | OFF、默认 30s、上限 300s、FX 上限 10s、非法 mode |
| `tools/light_state_test.c` | fake time 下的超时、remaining、OFF、重复命令刷新 |
| `tools/light_frame_test.c` | mask 着色、B 覆盖 A、越界 bit、active slot 统计 |

## 已有 model 验证

| 文件 | 覆盖 |
|---|---|
| `tools/ble_gatt_smoke_test_test.py` | BLE smoke 脚本测试向量和 notify 校验，不代表真实 BLE 通过 |
| `tools/binding_table_model_test.py` | `MOVE_BLOCK` Python 模型 |

## 下一批 host/model 验证

当前软件模型验证已收口；新增软件行为时在本节追加下一批 host/model 验证项。

## Zephyr build 验证

`tools/verify_host.sh --full-build` 应运行 host tests、Python tests、`git diff --check`、裸 XIAO build 和外设启用 XIAO build。构建成功只证明代码能编译进目标应用、对应 devicetree variant 可生成，不证明真实 BLE、灯条、NFC 或功耗行为。

- `--bare-build`：裸 XIAO BLE 验证 variant，确认未启用 PartRack 外设节点。
- `--peripheral-build`：加载 `xiao_ble_part_rack.dtsi`，确认 WS2812、灯条电源门控和 NFC FD 节点存在并可构建。

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
