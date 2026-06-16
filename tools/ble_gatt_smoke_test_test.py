#!/usr/bin/env python3
from __future__ import annotations

from ble_gatt_smoke_test import (
    SmokeValidationError,
    expected_slot1_record,
    explain_ble_backend_error,
    is_encryption_error,
    make_test_vectors,
    validate_destructive_binding_flow,
    validate_device_health,
    validate_light_status,
    validate_light_timeout_off,
    validate_read_all_partial_notifications,
    validate_read_all_notifications,
    validate_read_one_notifications,
    validate_table_crc_changed,
    validate_status_notification,
    validate_table_info,
    validate_table_info_changed,
    validate_table_seq_increased,
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


def test_vectors_cover_binding_mutations_and_factory_reset() -> None:
    vectors = make_test_vectors()

    assert vectors["clear_slot1"] == bytes.fromhex("11 01")
    assert vectors["set_slot1_qty_42"] == bytes.fromhex("30 01 2A 00")
    assert vectors["insert_slot1"] == bytes.fromhex(
        "20 01 01 43 31 32 33 34 35 36 37 00 00 0C 00 00 00 18"
    )
    assert vectors["remove_slot1"] == bytes.fromhex("21 01")
    assert vectors["move_slot1_to_2_len_1"] == bytes.fromhex("22 01 02 01")
    assert vectors["factory_reset"] == bytes.fromhex("F0 A5 A5 5A 5A")


def test_read_one_validation_requires_exact_slot_record() -> None:
    record = expected_slot1_record()
    notifications = [
        bytes.fromhex("10 00"),
        bytes([0x01, 0x00]) + record,
    ]

    validate_read_one_notifications(notifications, record)


def test_read_one_validation_accepts_updated_quantity() -> None:
    record = expected_slot1_record(qty=42)

    validate_read_one_notifications([bytes([0x01, 0x00]) + record], record)


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


def test_read_all_partial_validation_requires_slot_one_payload() -> None:
    record = expected_slot1_record()

    validate_read_all_partial_notifications([bytes([0x02, 0x00]) + record], record)


def test_read_all_validation_rejects_missing_end_marker() -> None:
    try:
        validate_read_all_notifications([bytes([0x02, 0x00]) + expected_slot1_record()])
    except SmokeValidationError as exc:
        assert "READ_ALL end marker" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_status_validation_requires_exact_op_and_status() -> None:
    validate_status_notification([bytes.fromhex("10 00"), bytes.fromhex("30 00")], 0x30)


def test_table_info_validation_requires_shape_and_slot_count() -> None:
    validate_table_info(bytes.fromhex("05 00 00 00 21 F5 19"))

    try:
        validate_table_info(bytes.fromhex("05 00 00 00 21 F5 18"))
    except SmokeValidationError as exc:
        assert "slot_count" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_table_info_changed_requires_seq_or_crc_change() -> None:
    before = bytes.fromhex("05 00 00 00 21 F5 19")
    after = bytes.fromhex("06 00 00 00 10 44 19")

    validate_table_info_changed(before, after)

    try:
        validate_table_info_changed(before, before)
    except SmokeValidationError as exc:
        assert "Table Info did not change" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_table_seq_validation_requires_increase() -> None:
    before = bytes.fromhex("05 00 00 00 21 F5 19")
    after = bytes.fromhex("06 00 00 00 21 F5 19")

    validate_table_seq_increased(before, after)

    try:
        validate_table_seq_increased(after, before)
    except SmokeValidationError as exc:
        assert "seq did not increase" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_table_crc_validation_requires_change() -> None:
    before = bytes.fromhex("05 00 00 00 21 F5 19")
    after = bytes.fromhex("06 00 00 00 10 44 19")

    validate_table_crc_changed(before, after)

    try:
        validate_table_crc_changed(before, bytes.fromhex("06 00 00 00 21 F5 19"))
    except SmokeValidationError as exc:
        assert "CRC did not change" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_light_status_validation_checks_mode_and_remaining() -> None:
    validate_light_status(bytes.fromhex("00 00 00"), expected_mode=0)
    validate_light_status(bytes.fromhex("01 09 00"), expected_mode=1, min_remaining=1)

    try:
        validate_light_status(bytes.fromhex("01 00 00"), expected_mode=1, min_remaining=1)
    except SmokeValidationError as exc:
        assert "remaining" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_light_timeout_validation_requires_off_status() -> None:
    validate_light_timeout_off(
        [
            bytes.fromhex("01 0A 00"),
            bytes.fromhex("01 01 00"),
            bytes.fromhex("00 00 00"),
        ]
    )

    try:
        validate_light_timeout_off([bytes.fromhex("01 01 00")])
    except SmokeValidationError as exc:
        assert "Light Status timeout OFF" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_destructive_binding_flow_validation_checks_mutations() -> None:
    slot1 = expected_slot1_record(qty=12)
    slot1_qty42 = expected_slot1_record(qty=42)
    slot2 = bytearray(slot1_qty42)
    slot2[0] = 2
    slot2 = bytes(slot2)

    validate_destructive_binding_flow(
        {
            "clear": [bytes.fromhex("11 00"), bytes([0x01, 0x00]) + bytes(16)],
            "insert": [bytes.fromhex("20 00"), bytes([0x01, 0x00]) + slot1],
            "set_qty": [bytes.fromhex("30 00"), bytes([0x01, 0x00]) + slot1_qty42],
            "move": [bytes.fromhex("22 00"), bytes([0x01, 0x00]) + bytes(16),
                     bytes([0x01, 0x00]) + slot2],
            "remove": [bytes.fromhex("21 00"), bytes([0x01, 0x00]) + slot1_qty42],
            "factory_reset": [bytes.fromhex("F0 00"), bytes([0x01, 0x00]) + bytes(16)],
        }
    )


def test_destructive_binding_flow_validation_rejects_missing_phase() -> None:
    try:
        validate_destructive_binding_flow({})
    except SmokeValidationError as exc:
        assert "destructive phase" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_device_health_validation_checks_shape_and_battery_range() -> None:
    validate_device_health(bytes.fromhex("64 02 00 00"))

    try:
        validate_device_health(bytes.fromhex("65 02 00 00"))
    except SmokeValidationError as exc:
        assert "battery_pct" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")

    try:
        validate_device_health(bytes.fromhex("64 02 00"))
    except SmokeValidationError as exc:
        assert "Device Health length" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_status_validation_rejects_missing_status() -> None:
    try:
        validate_status_notification([bytes.fromhex("10 00")], 0x30)
    except SmokeValidationError as exc:
        assert "status notification" in str(exc)
    else:
        raise AssertionError("expected SmokeValidationError")


def test_explains_corebluetooth_unsupported_error() -> None:
    message = explain_ble_backend_error(RuntimeError("BLE is unsupported"))

    assert "CoreBluetooth" in message
    assert "restricted" in message
    assert "BLE is unsupported" in message


def test_detects_corebluetooth_encryption_errors() -> None:
    message = (
        'Error Domain=CBATTErrorDomain Code=15 "Encryption is insufficient." '
        "UserInfo={NSLocalizedDescription=Encryption is insufficient.}"
    )

    assert is_encryption_error(RuntimeError(message))


if __name__ == "__main__":
    test_vectors_match_manual_bringup_values()
    test_vectors_cover_binding_mutations_and_factory_reset()
    test_read_one_validation_requires_exact_slot_record()
    test_read_one_validation_accepts_updated_quantity()
    test_read_one_validation_rejects_wrong_payload()
    test_read_all_validation_requires_end_marker()
    test_read_all_partial_validation_requires_slot_one_payload()
    test_read_all_validation_rejects_missing_end_marker()
    test_status_validation_requires_exact_op_and_status()
    test_table_info_validation_requires_shape_and_slot_count()
    test_table_info_changed_requires_seq_or_crc_change()
    test_table_seq_validation_requires_increase()
    test_table_crc_validation_requires_change()
    test_light_status_validation_checks_mode_and_remaining()
    test_light_timeout_validation_requires_off_status()
    test_destructive_binding_flow_validation_checks_mutations()
    test_destructive_binding_flow_validation_rejects_missing_phase()
    test_device_health_validation_checks_shape_and_battery_range()
    test_status_validation_rejects_missing_status()
    test_explains_corebluetooth_unsupported_error()
    test_detects_corebluetooth_encryption_errors()
    print("ble gatt smoke test vectors passed")
