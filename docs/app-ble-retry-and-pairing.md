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
