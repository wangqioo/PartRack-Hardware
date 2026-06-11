#!/usr/bin/env python3
from __future__ import annotations


SLOT_COUNT = 25


def move_block(items: list[int], from_slot: int, to_slot: int, length: int) -> list[int]:
    if from_slot < 1 or to_slot < 1 or length < 1:
        raise ValueError("invalid move")
    if from_slot + length - 1 > len(items) or to_slot + length - 1 > len(items):
        raise ValueError("invalid move")

    start = from_slot - 1
    block = items[start : start + length]
    rest = items[:start] + items[start + length :]
    insert_at = to_slot - 1
    return rest[:insert_at] + block + rest[insert_at:]


def self_test() -> None:
    base = list(range(1, SLOT_COUNT + 1))

    assert move_block(base, 2, 5, 2)[:8] == [1, 4, 5, 6, 2, 3, 7, 8]
    assert move_block(base, 5, 2, 2)[:8] == [1, 5, 6, 2, 3, 4, 7, 8]
    assert move_block(base, 1, 24, 2)[-4:] == [24, 25, 1, 2]
    assert move_block(base, 24, 1, 2)[:4] == [24, 25, 1, 2]


if __name__ == "__main__":
    self_test()
    print("binding table model checks passed")
