#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NCS_DIR="${NCS_DIR:-/Users/wq/ncs}"
ROOT_NAME="$(basename "${ROOT_DIR}")"
BUILD_DIR="${BUILD_DIR:-${NCS_DIR}/build-partrack-${ROOT_NAME}-xiao-sense}"
BOARD="${BOARD:-xiao_ble/nrf52840/sense}"
MODE="${1:---full-build}"

usage() {
  cat <<'USAGE'
Usage: tools/verify_host.sh [--host-only|--full-build]

  --host-only   Run C/Python host checks and git diff whitespace checks.
  --full-build  Run host checks, then build the Zephyr firmware target.

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
    tools/storage_snapshot_test.c protocol/viberack_protocol.c protocol/viberack_storage.c \
    -o /tmp/storage_snapshot_test
  /tmp/storage_snapshot_test

  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/binding_table_core_test.c protocol/viberack_protocol.c protocol/viberack_binding_table_model.c \
    -o /tmp/binding_table_core_test
  /tmp/binding_table_core_test

  python3 tools/protocol_check.py
  python3 tools/binding_table_model_test.py
  python3 tools/ble_gatt_smoke_test_test.py

  git diff --check
}

run_zephyr_build() {
  cd "${NCS_DIR}"
  ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew \
    "${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
    "${ROOT_DIR}/firmware/nrf52/app" \
    -d "${BUILD_DIR}"

  echo "UF2: ${BUILD_DIR}/app/zephyr/zephyr.uf2"
}

case "${MODE}" in
  --host-only)
    run_host_checks
    ;;
  --full-build)
    run_host_checks
    run_zephyr_build
    ;;
  -h|--help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
