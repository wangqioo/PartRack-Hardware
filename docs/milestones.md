# 联调里程碑

## M1 点亮全链路

目标：硬件碰一下 NFC，APP 弹出，连接底盘并点亮指定槽位。

验收：

- NFC NDEF URI 能路由到 APP。
- APP 能按 MAC 定向连接。
- APP 下发 `FIND` 灯控帧。
- 固件按超时自动熄灯并断 MOS。

## M2 绑定表读写与恢复

目标：固件可靠保存槽位绑定，APP 可通过协议从硬件恢复最小台账。

验收：

- `WRITE_ONE` 写穿 FDS。
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

APP 页面、Room 数据模型、BOM 匹配、扫码入库和二维码打印由 `Yrd980/LCSC_android_erp` 仓库维护。本仓库只提供硬件、固件和 BLE 契约。
