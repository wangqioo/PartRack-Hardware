#!/usr/bin/env python3
from __future__ import annotations

from ble_gatt_smoke_test import (
    SmokeValidationError,
    expected_slot1_record,
    explain_ble_backend_error,
    make_test_vectors,
    validate_read_all_notifications,
    validate_read_one_notifications,
)


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


def test_read_one_validation_requires_exact_slot_record() -> None:
    record = expected_slot1_record()
    notifications = [
        bytes.fromhex("10 00"),
        bytes([0x01, 0x00]) + record,
    ]

    validate_read_one_notifications(notifications, record)


def test_read_one_validation_rejects_wrong_payload() -> None:
    record = expected_slot1_record()
    wrong_record = bytearray(record)
    wrong_record[11] = 0x0D

    try:
        validate_read_one_notifications([bytes([0x01, 0x00]) + bytes(wrong_record)], record)
    except SmokeValidationError as exc:
        assert "READ_ONE success notification" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_read_all_validation_requires_end_marker() -> None:
    notifications = [
        bytes([0x02, 0x00]) + expected_slot1_record(),
        bytes.fromhex("02 00 FF"),
    ]

    validate_read_all_notifications(notifications)


def test_read_all_validation_rejects_missing_end_marker() -> None:
    try:
        validate_read_all_notifications([bytes([0x02, 0x00]) + expected_slot1_record()])
    except SmokeValidationError as exc:
        assert "READ_ALL end marker" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_explains_corebluetooth_unsupported_error() -> None:
    message = explain_ble_backend_error(RuntimeError("BLE is unsupported"))

    assert "CoreBluetooth" in message
    assert "restricted" in message
    assert "BLE is unsupported" in message


if __name__ == "__main__":
    test_vectors_match_manual_bringup_values()
    test_read_one_validation_requires_exact_slot_record()
    test_read_one_validation_rejects_wrong_payload()
    test_read_all_validation_requires_end_marker()
    test_read_all_validation_rejects_missing_end_marker()
    test_explains_corebluetooth_unsupported_error()
    print("ble gatt smoke test vectors passed")
