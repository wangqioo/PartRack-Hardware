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


def hex_bytes(data: bytes) -> str:
    return data.hex(" ").upper()


def make_test_vectors() -> dict[str, bytes]:
    return {
        "write_slot1": bytes([0x10])
        + SlotRecord(slot=1, part_id="C1234567", qty=12, flags=0).encode(),
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


async def run_smoke(device_name: str) -> None:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as exc:
        raise SystemExit("bleak is required for --run-smoke: python3 -m pip install bleak") from exc

    device = await BleakScanner.find_device_by_name(device_name, timeout=10.0)
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
        await asyncio.sleep(1.0)
        await client.stop_notify(BINDING_CP_UUID)

    expected_prefix = bytes([0x01, 0x00])
    if not any(data.startswith(expected_prefix) for data in notifications):
        raise SystemExit("READ_ONE success notification was not observed")


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
