#!/usr/bin/env python3
from __future__ import annotations

from ble_gatt_smoke_test import make_test_vectors


def test_vectors_match_manual_bringup_values() -> None:
    vectors = make_test_vectors()

    assert vectors["write_slot1"] == bytes.fromhex(
        "10 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18"
    )
    assert vectors["read_slot1"] == bytes.fromhex("01 01")
    assert vectors["read_all"] == bytes.fromhex("02")
    assert vectors["light_find_slot1_red_10s"] == bytes.fromhex(
        "01 01 00 00 00 00 00 00 00 FF 00 00 00 00 00 0A 00"
    )
    assert vectors["light_off"] == bytes.fromhex(
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    )


if __name__ == "__main__":
    test_vectors_match_manual_bringup_values()
    print("ble gatt smoke test vectors passed")
