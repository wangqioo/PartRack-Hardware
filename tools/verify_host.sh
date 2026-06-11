#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NCS_DIR="${NCS_DIR:-/Users/wq/ncs}"
BUILD_DIR="${BUILD_DIR:-${NCS_DIR}/build-partrack-xiao-sense}"
BOARD="${BOARD:-xiao_ble/nrf52840/sense}"

cd "${ROOT_DIR}"

cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/light_frame_test.c protocol/viberack_protocol.c protocol/viberack_light_frame.c \
  -o /tmp/light_frame_test
/tmp/light_frame_test

cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/light_policy_test.c protocol/viberack_protocol.c protocol/viberack_light_policy.c \
  -o /tmp/light_policy_test
/tmp/light_policy_test

cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/storage_snapshot_test.c protocol/viberack_protocol.c protocol/viberack_storage.c \
  -o /tmp/storage_snapshot_test
/tmp/storage_snapshot_test

python3 tools/protocol_check.py
python3 tools/binding_table_model_test.py
python3 tools/ble_gatt_smoke_test_test.py

git diff --check

cd "${NCS_DIR}"
ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
  "${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
  "${ROOT_DIR}/firmware/nrf52/app" \
  -d "${BUILD_DIR}"

echo "UF2: ${BUILD_DIR}/app/zephyr/zephyr.uf2"
