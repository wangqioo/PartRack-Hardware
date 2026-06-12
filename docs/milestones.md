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
