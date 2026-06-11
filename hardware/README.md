# 硬件资料

## 智能底盘

- 主控：首版 nRF52832；nRF52811 作为量产降成本备选；nRF52840 仅作开发验证。
- NFC：NT3H2111，I2C 接 MCU，FD 接 GPIOTE PORT。
- 供电：1S 锂电，MCU 经 3.3V LDO，灯条经 P-MOS 由锂电直供。
- 灯条：25 x WS2812B，底盘前缘 PCB。
- 料盒：纯结构件 + 2 颗磁铁。

## 打样验证

- 25 盒满排后灯位与盒位偏移。
- WS2812B 在 3.7V 下亮度和一致性。
- P-MOS 整体断电后的静态电流。
- 磁铁强度 A/B 档手感和磁敏感物料风险。
- 各状态电流，校核 20uA 日常加权预算。

## 文件建议

```text
hardware/
  main-board/
    schematic/
    pcb/
    bom/
  led-strip/
    schematic/
    pcb/
    bom/
  mechanical/
    base/
    bins/
```

## 与 APP 的边界

硬件侧只保证：

- 槽位与灯位物理一致。
- MCU 中保存最小可恢复绑定表。
- BLE 协议行为稳定。
- 点灯和读写绑定表可被 APP 调用。

物料详情、BOM 匹配、扫码、库存展示和二维码打印均由 APP 仓库完成。
