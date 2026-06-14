#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NCS_DIR="${NCS_DIR:-/Users/wq/ncs}"
ROOT_NAME="$(basename "${ROOT_DIR}")"
BARE_BUILD_DIR="${BARE_BUILD_DIR:-${NCS_DIR}/build-partrack-${ROOT_NAME}-xiao-sense-bare}"
PERIPHERAL_BUILD_DIR="${PERIPHERAL_BUILD_DIR:-${NCS_DIR}/build-partrack-${ROOT_NAME}-xiao-sense-peripherals}"
PART_RACK_OVERLAY="${ROOT_DIR}/firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi"
BOARD="${BOARD:-xiao_ble/nrf52840/sense}"
MODE="${1:---full-build}"

usage() {
  cat <<'USAGE'
Usage: tools/verify_host.sh [--host-only|--bare-build|--peripheral-build|--full-build]

  --host-only         Run C/Python host checks and git diff whitespace checks.
  --bare-build        Build the bare XIAO BLE validation firmware.
  --peripheral-build  Build the XIAO firmware with PartRack peripherals enabled.
  --full-build        Run host checks, then build both Zephyr firmware variants.

Default: --full-build
USAGE
}

if [ "$#" -gt 1 ]; then
  usage >&2
  exit 2
fi

run_host_checks() {
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
    tools/light_state_test.c protocol/viberack_protocol.c protocol/viberack_light_policy.c protocol/viberack_light_state.c \
    -o /tmp/light_state_test
  /tmp/light_state_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/storage_snapshot_test.c protocol/viberack_protocol.c protocol/viberack_storage.c \
    -o /tmp/storage_snapshot_test
  /tmp/storage_snapshot_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/adv_payload_test.c protocol/viberack_adv_payload.c \
    -o /tmp/adv_payload_test
  /tmp/adv_payload_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/binding_table_core_test.c protocol/viberack_protocol.c protocol/viberack_binding_table_model.c \
    -o /tmp/binding_table_core_test
  /tmp/binding_table_core_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/ble_dispatcher_model_test.c protocol/viberack_protocol.c protocol/viberack_ble_dispatcher_model.c \
    -o /tmp/ble_dispatcher_model_test
  /tmp/ble_dispatcher_model_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/ble_lifecycle_test.c protocol/viberack_protocol.c protocol/viberack_ble_lifecycle.c \
    -o /tmp/ble_lifecycle_test
  /tmp/ble_lifecycle_test

  python3 tools/protocol_check.py
  python3 tools/binding_table_model_test.py
  python3 tools/ble_gatt_smoke_test_test.py

  git diff --check
}

run_bare_build() {
  cd "${NCS_DIR}"
  ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
    "${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
    "${ROOT_DIR}/firmware/nrf52/app" \
    -d "${BARE_BUILD_DIR}"

  if rg -q 'vbrk_ws2812|vbrk-led-strip|vbrk-nfc-fd' \
    "${BARE_BUILD_DIR}/app/zephyr/zephyr.dts"; then
    echo "bare build unexpectedly contains PartRack peripherals" >&2
    exit 1
  fi

  echo "Bare UF2: ${BARE_BUILD_DIR}/app/zephyr/zephyr.uf2"
}

run_peripheral_build() {
  cd "${NCS_DIR}"
  ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
    "${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
    "${ROOT_DIR}/firmware/nrf52/app" \
    -d "${PERIPHERAL_BUILD_DIR}" \
    -- -Dapp_EXTRA_DTC_OVERLAY_FILE="${PART_RACK_OVERLAY}"

  rg -q 'vbrk_ws2812' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"
  rg -q 'vbrk-led-strip' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"
  rg -q 'vbrk-nfc-fd' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"

  echo "Peripheral UF2: ${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.uf2"
}

case "${MODE}" in
  --host-only)
    run_host_checks
    ;;
  --bare-build)
    run_bare_build
    ;;
  --peripheral-build)
    run_peripheral_build
    ;;
  --full-build)
    run_host_checks
    run_bare_build
    run_peripheral_build
    ;;
  -h|--help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
