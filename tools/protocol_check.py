#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum


SLOT_RECORD_SIZE = 16
LIGHT_COMMAND_SIZE = 17
SLOT_COUNT = 25


class LightMode(IntEnum):
    OFF = 0x00
    FIND = 0x01
    PICK = 0x02
    SORT = 0x03
    STOCK_IN = 0x04
    FX = 0x05


def crc8_maxim(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x01:
                crc = ((crc >> 1) ^ 0x8C) & 0xFF
            else:
                crc >>= 1
    return crc


def slot_mask(*slots: int) -> int:
    mask = 0
    for slot in slots:
        if slot < 1 or slot > SLOT_COUNT:
            raise ValueError(f"slot out of range: {slot}")
        mask |= 1 << (slot - 1)
    return mask


@dataclass(frozen=True)
class SlotRecord:
    slot: int
    part_id: str
    qty: int
    flags: int = 0

    def encode(self) -> bytes:
        if self.slot < 0 or self.slot > SLOT_COUNT:
            raise ValueError(f"slot out of range: {self.slot}")
        part = self.part_id.encode("ascii")
        if len(part) > 10:
            raise ValueError("part_id must be <= 10 ASCII bytes")
        if self.qty < 0 or self.qty > 0xFFFF:
            raise ValueError(f"qty out of range: {self.qty}")

        body = bytes([self.slot])
        body += part.ljust(10, b"\x00")
        body += self.qty.to_bytes(2, "little")
        body += bytes([self.flags & 0xFF, 0x00])
        return body + bytes([crc8_maxim(body)])


@dataclass(frozen=True)
class LightCommand:
    mode: LightMode
    mask_a: int
    mask_b: int = 0
    color_a: tuple[int, int, int] = (0, 255, 0)
    color_b: tuple[int, int, int] = (0, 0, 0)
    timeout_s: int = 30

    def encode(self) -> bytes:
        if self.timeout_s < 0 or self.timeout_s > 300:
            raise ValueError(f"timeout_s out of range: {self.timeout_s}")
        for color in (*self.color_a, *self.color_b):
            if color < 0 or color > 255:
                raise ValueError(f"color out of range: {color}")

        frame = bytes([self.mode])
        frame += self.mask_a.to_bytes(4, "little")
        frame += self.mask_b.to_bytes(4, "little")
        frame += bytes(self.color_a)
        frame += bytes(self.color_b)
        frame += self.timeout_s.to_bytes(2, "little")
        return frame


def self_test() -> None:
    record = SlotRecord(slot=3, part_id="C1234567", qty=120, flags=0).encode()
    assert len(record) == SLOT_RECORD_SIZE
    assert crc8_maxim(record[:15]) == record[15]

    command = LightCommand(
        mode=LightMode.FIND,
        mask_a=slot_mask(3),
        color_a=(255, 0, 0),
        timeout_s=45,
    ).encode()
    assert len(command) == LIGHT_COMMAND_SIZE
    assert command[1:5] == (1 << 2).to_bytes(4, "little")

    pick = LightCommand(
        mode=LightMode.PICK,
        mask_a=slot_mask(1, 7, 25),
        color_a=(0, 255, 0),
    ).encode()
    assert len(pick) == LIGHT_COMMAND_SIZE


if __name__ == "__main__":
    self_test()
    print("protocol checks passed")
