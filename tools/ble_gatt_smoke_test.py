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


class SmokeValidationError(RuntimeError):
    pass


def hex_bytes(data: bytes) -> str:
    return data.hex(" ").upper()


def expected_slot1_record() -> bytes:
    return SlotRecord(slot=1, part_id="C1234567", qty=12, flags=0).encode()


def make_test_vectors() -> dict[str, bytes]:
    return {
        "write_slot1": bytes([0x10]) + expected_slot1_record(),
        "read_slot1": bytes([0x01, 0x01]),
        "read_all": bytes([0x02]),
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


def explain_ble_backend_error(exc: BaseException) -> str:
    message = str(exc)
    if "BLE is unsupported" in message:
        return (
            "BLE backend is unavailable: CoreBluetooth reported 'BLE is unsupported'. "
            "This usually means the current terminal/runtime is restricted from using the macOS "
            f"Bluetooth stack, not that the nRF device failed. Original error: {message}"
        )
    return f"BLE backend error: {message}"


async def run_smoke(device_name: str) -> None:
    try:
        from bleak import BleakClient, BleakScanner
        from bleak.exc import BleakError
    except ImportError as exc:
        raise SystemExit("bleak is required for --run-smoke: python3 -m pip install bleak") from exc

    try:
        device = await BleakScanner.find_device_by_name(device_name, timeout=10.0)
    except BleakError as exc:
        raise SystemExit(explain_ble_backend_error(exc)) from exc

    if device is None:
        raise SystemExit(f"device not found: {device_name}")

    notifications: list[bytes] = []

    def on_binding_notify(_sender: int, data: bytearray) -> None:
        notifications.append(bytes(data))
        print(f"binding_notify: {hex_bytes(data)}")

    async with BleakClient(device) as client:
        table_info = await client.read_gatt_char(TABLE_INFO_UUID)
        print(f"table_info: {hex_bytes(table_info)}")

        light_status = await client.read_gatt_char(LIGHT_STATUS_UUID)
        print(f"light_status: {hex_bytes(light_status)}")

        await client.start_notify(BINDING_CP_UUID, on_binding_notify)
        vectors = make_test_vectors()
        await client.write_gatt_char(BINDING_CP_UUID, vectors["write_slot1"], response=True)
        await asyncio.sleep(0.2)
        await client.write_gatt_char(BINDING_CP_UUID, vectors["read_slot1"], response=True)
        await asyncio.sleep(0.5)
        validate_read_one_notifications(notifications, expected_slot1_record())

        notifications.clear()
        await client.write_gatt_char(BINDING_CP_UUID, vectors["read_all"], response=True)
        await asyncio.sleep(1.0)
        validate_read_all_notifications(notifications)

        await client.stop_notify(BINDING_CP_UUID)


def main() -> int:
    parser = argparse.ArgumentParser(description="PartRack BLE GATT smoke helper")
    parser.add_argument("--device-name", default=DEVICE_NAME)
    parser.add_argument("--print-vectors", action="store_true")
    parser.add_argument("--run-smoke", action="store_true")
    args = parser.parse_args()

    if args.print_vectors or not args.run_smoke:
        print_test_vectors()

    if args.run_smoke:
        asyncio.run(run_smoke(args.device_name))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
