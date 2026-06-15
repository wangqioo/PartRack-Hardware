#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import sys

from protocol_check import LightCommand, LightMode, SlotRecord, slot_mask


DEVICE_NAME = "VBRK-0000"
BINDING_CP_UUID = "7f4b1001-8d1a-4d45-9a4e-2b4a7c000000"
TABLE_INFO_UUID = "7f4b1002-8d1a-4d45-9a4e-2b4a7c000000"
LIGHT_COMMAND_UUID = "7f4b2001-8d1a-4d45-9a4e-2b4a7c000000"
LIGHT_STATUS_UUID = "7f4b2002-8d1a-4d45-9a4e-2b4a7c000000"
FACTORY_RESET_MAGIC_LE = bytes.fromhex("A5 A5 5A 5A")


class SmokeValidationError(RuntimeError):
    pass


def hex_bytes(data: bytes) -> str:
    return data.hex(" ").upper()


def expected_slot1_record(qty: int = 12) -> bytes:
    return SlotRecord(slot=1, part_id="C1234567", qty=qty, flags=0).encode()


def make_test_vectors() -> dict[str, bytes]:
    record = expected_slot1_record()
    return {
        "write_slot1": bytes([0x10]) + record,
        "read_slot1": bytes([0x01, 0x01]),
        "read_all": bytes([0x02]),
        "clear_slot1": bytes([0x11, 0x01]),
        "insert_slot1": bytes([0x20, 0x01]) + record,
        "remove_slot1": bytes([0x21, 0x01]),
        "move_slot1_to_2_len_1": bytes([0x22, 0x01, 0x02, 0x01]),
        "set_slot1_qty_42": bytes([0x30, 0x01, 0x2A, 0x00]),
        "factory_reset": bytes([0xF0]) + FACTORY_RESET_MAGIC_LE,
        "light_find_slot1_red_10s": LightCommand(
            mode=LightMode.FIND,
            mask_a=slot_mask(1),
            color_a=(255, 0, 0),
            timeout_s=10,
        ).encode(),
        "light_off": LightCommand(
            mode=LightMode.OFF,
            mask_a=0,
            color_a=(0, 0, 0),
            timeout_s=0,
        ).encode(),
    }


def print_test_vectors() -> None:
    for name, data in make_test_vectors().items():
        print(f"{name}: {hex_bytes(data)}")


def validate_read_one_notifications(notifications: list[bytes], expected_record: bytes) -> None:
    expected = bytes([0x01, 0x00]) + expected_record
    if expected not in notifications:
        observed = ", ".join(hex_bytes(data) for data in notifications) or "<none>"
        raise SmokeValidationError(
            f"READ_ONE success notification with expected payload was not observed; observed: {observed}"
        )


def validate_read_all_notifications(notifications: list[bytes]) -> None:
    expected_end = bytes([0x02, 0x00, 0xFF])
    if expected_end not in notifications:
        observed = ", ".join(hex_bytes(data) for data in notifications) or "<none>"
        raise SmokeValidationError(f"READ_ALL end marker was not observed; observed: {observed}")


def validate_read_all_partial_notifications(notifications: list[bytes], expected_record: bytes) -> None:
    expected_first = bytes([0x02, 0x00]) + expected_record
    if expected_first not in notifications:
        observed = ", ".join(hex_bytes(data) for data in notifications) or "<none>"
        raise SmokeValidationError(
            f"READ_ALL slot 1 payload was not observed; observed: {observed}"
        )


def validate_status_notification(notifications: list[bytes], op: int, status: int = 0) -> None:
    expected = bytes([op, status])
    if expected not in notifications:
        observed = ", ".join(hex_bytes(data) for data in notifications) or "<none>"
        raise SmokeValidationError(
            f"status notification {hex_bytes(expected)} was not observed; observed: {observed}"
        )


def validate_table_info(data: bytes) -> None:
    if len(data) != 7:
        raise SmokeValidationError(f"Table Info length should be 7 bytes; got {len(data)}")
    if data[6] != 25:
        raise SmokeValidationError(f"Table Info slot_count should be 25; got {data[6]}")


def validate_table_info_changed(before: bytes, after: bytes) -> None:
    validate_table_info(before)
    validate_table_info(after)
    if before[:6] == after[:6]:
        raise SmokeValidationError(
            f"Table Info did not change after binding mutation: {hex_bytes(after)}"
        )


def table_info_seq(data: bytes) -> int:
    validate_table_info(data)
    return int.from_bytes(data[:4], "little")


def table_info_crc(data: bytes) -> int:
    validate_table_info(data)
    return int.from_bytes(data[4:6], "little")


def validate_table_seq_increased(before: bytes, after: bytes) -> None:
    before_seq = table_info_seq(before)
    after_seq = table_info_seq(after)
    if after_seq <= before_seq:
        raise SmokeValidationError(
            f"Table Info seq did not increase: before={before_seq} after={after_seq}"
        )


def validate_table_crc_changed(before: bytes, after: bytes) -> None:
    before_crc = table_info_crc(before)
    after_crc = table_info_crc(after)
    if before_crc == after_crc:
        raise SmokeValidationError(
            f"Table Info CRC did not change: before={hex_bytes(before)} after={hex_bytes(after)}"
        )


def validate_light_status(data: bytes, expected_mode: int, min_remaining: int | None = None) -> None:
    if len(data) != 3:
        raise SmokeValidationError(f"Light Status length should be 3 bytes; got {len(data)}")
    mode = data[0]
    remaining = int.from_bytes(data[1:3], "little")
    if mode != expected_mode:
        raise SmokeValidationError(
            f"Light Status mode should be {expected_mode}; got {mode} ({hex_bytes(data)})"
        )
    if min_remaining is not None and remaining < min_remaining:
        raise SmokeValidationError(
            f"Light Status remaining should be >= {min_remaining}; got {remaining}"
        )


def explain_ble_backend_error(exc: BaseException) -> str:
    message = str(exc)
    if "BLE is unsupported" in message:
        return (
            "BLE backend is unavailable: CoreBluetooth reported 'BLE is unsupported'. "
            "This usually means the current terminal/runtime is restricted from using the macOS "
            f"Bluetooth stack, not that the nRF device failed. Original error: {message}"
        )
    return f"BLE backend error: {message}"


def is_encryption_error(exc: BaseException) -> bool:
    message = str(exc).lower()
    return (
        "encryption is insufficient" in message
        or "insufficient encryption" in message
        or "authentication" in message
    )


async def write_gatt_char_with_pairing_retry(client, char_uuid: str, data: bytes) -> None:
    try:
        await client.write_gatt_char(char_uuid, data, response=True)
        return
    except Exception as exc:
        if not is_encryption_error(exc):
            raise

        print(
            "encrypted write rejected; waiting for OS pairing/encryption, then retrying once",
            file=sys.stderr,
        )
        await asyncio.sleep(5.0)
        await client.write_gatt_char(char_uuid, data, response=True)


async def wait_for_read_all_end(notifications: list[bytes], timeout_s: float = 5.0) -> None:
    expected_end = bytes([0x02, 0x00, 0xFF])
    deadline = asyncio.get_running_loop().time() + timeout_s

    while asyncio.get_running_loop().time() < deadline:
        if expected_end in notifications:
            return
        await asyncio.sleep(0.05)


async def find_device(device_name: str):
    try:
        from bleak import BleakScanner
        from bleak.exc import BleakError
    except ImportError as exc:
        raise SystemExit("bleak is required for BLE runs: python3 -m pip install bleak") from exc

    try:
        device = await BleakScanner.find_device_by_name(device_name, timeout=10.0)
    except BleakError as exc:
        raise SystemExit(explain_ble_backend_error(exc)) from exc

    if device is None:
        raise SystemExit(f"device not found: {device_name}")
    return device


def make_binding_notify_collector(notifications: list[bytes]):
    def on_binding_notify(_sender: int, data: bytearray) -> None:
        notifications.append(bytes(data))
        print(f"binding_notify: {hex_bytes(data)}")

    return on_binding_notify


async def run_smoke(device_name: str, include_destructive: bool) -> None:
    try:
        from bleak import BleakClient
    except ImportError as exc:
        raise SystemExit("bleak is required for --run-smoke: python3 -m pip install bleak") from exc

    device = await find_device(device_name)

    notifications: list[bytes] = []
    on_binding_notify = make_binding_notify_collector(notifications)

    async with BleakClient(device) as client:
        table_info = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info: {hex_bytes(table_info)}")
        validate_table_info(table_info)

        light_status = await client.read_gatt_char(LIGHT_STATUS_UUID)
        print(f"light_status: {hex_bytes(light_status)}")
        validate_light_status(light_status, expected_mode=0)

        await client.start_notify(BINDING_CP_UUID, on_binding_notify)
        vectors = make_test_vectors()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["write_slot1"])
        await asyncio.sleep(0.2)
        validate_status_notification(notifications, 0x10)

        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["read_slot1"])
        await asyncio.sleep(0.5)
        validate_read_one_notifications(notifications, expected_slot1_record())

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["read_all"])
        await wait_for_read_all_end(notifications)
        validate_read_all_partial_notifications(notifications, expected_slot1_record())
        validate_read_all_notifications(notifications)

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["set_slot1_qty_42"])
        await asyncio.sleep(0.2)
        validate_status_notification(notifications, 0x30)

        if include_destructive:
            notifications.clear()
            await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["clear_slot1"])
            await asyncio.sleep(0.2)
            validate_status_notification(notifications, 0x11)

            notifications.clear()
            await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["factory_reset"])
            await asyncio.sleep(0.5)
            validate_status_notification(notifications, 0xF0)

        await client.stop_notify(BINDING_CP_UUID)


async def run_batch(device_name: str) -> None:
    try:
        from bleak import BleakClient
    except ImportError as exc:
        raise SystemExit("bleak is required for --run-batch: python3 -m pip install bleak") from exc

    device = await find_device(device_name)
    notifications: list[bytes] = []
    on_binding_notify = make_binding_notify_collector(notifications)

    async with BleakClient(device) as client:
        table_info_before = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info_before: {hex_bytes(table_info_before)}")
        validate_table_info(table_info_before)

        light_status = await client.read_gatt_char(LIGHT_STATUS_UUID)
        print(f"light_status_initial: {hex_bytes(light_status)}")
        validate_light_status(light_status, expected_mode=0)

        await client.start_notify(BINDING_CP_UUID, on_binding_notify)
        vectors = make_test_vectors()

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["write_slot1"])
        await asyncio.sleep(0.2)
        validate_status_notification(notifications, 0x10)

        table_info_after_write = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info_after_write: {hex_bytes(table_info_after_write)}")
        validate_table_seq_increased(table_info_before, table_info_after_write)
        validate_table_crc_changed(table_info_before, table_info_after_write)

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["read_slot1"])
        await asyncio.sleep(0.5)
        validate_read_one_notifications(notifications, expected_slot1_record())

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["read_all"])
        await wait_for_read_all_end(notifications)
        validate_read_all_partial_notifications(notifications, expected_slot1_record())
        validate_read_all_notifications(notifications)

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["set_slot1_qty_42"])
        await asyncio.sleep(0.2)
        validate_status_notification(notifications, 0x30)

        notifications.clear()
        await write_gatt_char_with_pairing_retry(client, BINDING_CP_UUID, vectors["read_slot1"])
        await asyncio.sleep(0.5)
        validate_read_one_notifications(notifications, expected_slot1_record(qty=42))

        table_info_after = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info_after: {hex_bytes(table_info_after)}")
        validate_table_seq_increased(table_info_after_write, table_info_after)

        await client.write_gatt_char(
            LIGHT_COMMAND_UUID, vectors["light_find_slot1_red_10s"], response=False
        )
        await asyncio.sleep(0.2)
        light_status = await client.read_gatt_char(LIGHT_STATUS_UUID)
        print(f"light_status_find: {hex_bytes(light_status)}")
        validate_light_status(light_status, expected_mode=1, min_remaining=1)

        await client.write_gatt_char(LIGHT_COMMAND_UUID, vectors["light_off"], response=False)
        await asyncio.sleep(0.2)
        light_status = await client.read_gatt_char(LIGHT_STATUS_UUID)
        print(f"light_status_off: {hex_bytes(light_status)}")
        validate_light_status(light_status, expected_mode=0)

        await client.stop_notify(BINDING_CP_UUID)


async def run_persistence_read(device_name: str) -> None:
    try:
        from bleak import BleakClient
    except ImportError as exc:
        raise SystemExit(
            "bleak is required for --run-persistence-read: python3 -m pip install bleak"
        ) from exc

    device = await find_device(device_name)
    notifications: list[bytes] = []
    on_binding_notify = make_binding_notify_collector(notifications)

    async with BleakClient(device) as client:
        table_info = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info_after_reboot: {hex_bytes(table_info)}")
        validate_table_info(table_info)

        await client.start_notify(BINDING_CP_UUID, on_binding_notify)
        await write_gatt_char_with_pairing_retry(
            client, BINDING_CP_UUID, make_test_vectors()["read_slot1"]
        )
        await asyncio.sleep(0.5)
        validate_read_one_notifications(notifications, expected_slot1_record(qty=42))
        await client.stop_notify(BINDING_CP_UUID)


def main() -> int:
    parser = argparse.ArgumentParser(description="PartRack BLE GATT smoke helper")
    parser.add_argument("--device-name", default=DEVICE_NAME)
    parser.add_argument("--print-vectors", action="store_true")
    parser.add_argument("--run-smoke", action="store_true")
    parser.add_argument("--run-batch", action="store_true")
    parser.add_argument("--run-persistence-read", action="store_true")
    parser.add_argument(
        "--include-destructive",
        action="store_true",
        help="also run CLEAR_ONE and FACTORY_RESET; this erases test binding data",
    )
    args = parser.parse_args()

    if args.print_vectors or not (args.run_smoke or args.run_batch or args.run_persistence_read):
        print_test_vectors()

    if args.run_smoke:
        asyncio.run(run_smoke(args.device_name, args.include_destructive))
    if args.run_batch:
        asyncio.run(run_batch(args.device_name))
    if args.run_persistence_read:
        asyncio.run(run_persistence_read(args.device_name))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
