# PartRack BLE Dispatcher Model Design

## Context

Batch 2 moved binding-table mutation into `protocol/viberack_binding_table_model.[ch]`
and proved it with host tests. The next firmware-quality gap is the BLE Binding
Control Point dispatcher: opcode length checks, status mapping, notify payload
shape, `READ_ALL` end marker, and Table Info notify behavior.

The current implementation lives directly inside
`firmware/nrf52/app/src/app_ble.c`. That code works, but its command dispatch
logic can only be exercised through Zephyr BLE callbacks or live BLE smoke
tests. This batch extracts the command-level logic into a pure C model so the
same behavior can be tested on the host.

## Goal

Create a host-testable BLE Binding Control Point dispatcher model used by
firmware and verified by `tools/verify_host.sh --host-only`.

## Non-Goals

- Do not change BLE UUIDs, opcodes, frame formats, status codes, or APP-facing
  contracts.
- Do not implement the advertising payload model in this batch.
- Do not implement the light state machine model in this batch.
- Do not replace Zephyr GATT, connection, notification, or advertising APIs.
- Do not claim real BLE, pairing, or notify timing is verified by host tests.

## Design

Add `protocol/viberack_ble_dispatcher_model.[ch]` as a pure C dispatcher for
Binding Control Point request frames. The model accepts a request buffer and a
table operation interface. It validates opcode-specific request lengths,
invokes table callbacks, maps errno-style results to protocol status codes, and
emits response frames through caller-provided callbacks.

`READ_ONE` emits one Binding Control Point notify:

```text
op + status + record-on-success
```

`READ_ALL` emits up to 25 record notifies and then the existing end marker:

```text
02 00 + record(16B)
02 00 FF
```

If any slot read fails during `READ_ALL`, the dispatcher emits the failing
status frame and stops without the end marker.

Write-class commands emit one status notify. When the operation succeeds and
changes table state, the dispatcher sets a `table_changed` flag and invokes the
caller-provided Table Info notify callback. Firmware refreshes advertising from
that same successful-table-change callback, exactly as it does today.

## Model Boundary

The pure dispatcher owns:

- opcode recognition
- opcode-specific request length validation
- little-endian parsing for `SET_QTY` and `FACTORY_RESET`
- status mapping from errno to protocol status
- Binding Control Point response frame construction
- `READ_ALL` record notify sequence and end marker
- Table Info notify decision for successful mutating commands
- preserving current notify-failure semantics: callback return values do not
  turn a handled command into a GATT write failure

The Zephyr adapter keeps:

- GATT write callback signature and `BT_GATT_ERR(...)` handling
- `bt_gatt_notify(...)`
- current connection state
- Table Info payload generation
- advertising refresh
- encryption permissions and CCC descriptors
- live BLE timing and pairing behavior

## Interfaces

The dispatcher model exposes a context struct with table callbacks and notify
callbacks. `table_changed` is implemented by the firmware adapter as Table Info
notify plus advertising refresh:

```c
typedef struct {
    int (*read_one)(uint8_t slot, vbrk_slot_record_t *record, void *user_data);
    int (*write_one)(const vbrk_slot_record_t *record, void *user_data);
    int (*clear_one)(uint8_t slot, void *user_data);
    int (*insert_at)(uint8_t slot, const vbrk_slot_record_t *record, void *user_data);
    int (*remove_at)(uint8_t slot, void *user_data);
    int (*move_block)(uint8_t from, uint8_t to, uint8_t len, void *user_data);
    int (*set_qty)(uint8_t slot, uint16_t qty, void *user_data);
    int (*factory_reset)(uint32_t magic, void *user_data);
    int (*notify_binding)(uint8_t op, uint8_t status, const void *payload,
                          uint16_t len, void *user_data);
    void (*table_changed)(void *user_data);
    void *user_data;
} vbrk_ble_dispatcher_t;
```

The main function returns `0` for a handled request and `-EINVAL` for a malformed
request frame. Malformed frames still produce a Binding Control Point status
notify when an opcode byte exists, matching the current firmware behavior.
Notify callback errors are intentionally ignored after the callback is attempted,
matching the current firmware behavior where `binding_cp_write()` returns the
write length even if `bt_gatt_notify()` fails.

```c
int vbrk_ble_dispatch_binding_cp(vbrk_ble_dispatcher_t *dispatcher,
                                 const uint8_t *frame, uint16_t len);
```

## Host Verification

Add `tools/ble_dispatcher_model_test.c` and wire it into
`tools/verify_host.sh --host-only`.

Tests must cover:

- empty frame returns `-EINVAL` and emits no notify
- unknown opcode emits `status=VBRK_STATUS_ERR_PARAM`
- opcode length errors emit `VBRK_STATUS_ERR_PARAM`
- `READ_ONE` success emits `op,status,record`
- `READ_ALL` emits 25 records plus `02 00 FF`
- `READ_ALL` stops without end marker on read failure
- `WRITE_ONE` success emits status notify and Table Info notify
- `WRITE_ONE` CRC failure maps to `VBRK_STATUS_ERR_CRC`
- `INSERT_AT` full-table failure maps to `VBRK_STATUS_ERR_FULL`
- `SET_QTY` parses little-endian quantity
- `FACTORY_RESET` parses little-endian magic bytes
- Table Info notify is not emitted for failed mutating commands or read
  commands
- callback notify failures do not prevent dispatcher success or a successful
  table-change callback from being attempted

## Documentation

After implementation, update `docs/firmware-quality-plan.md` so the BLE
dispatcher model moves from `下一批 host/model 验证` to existing host/model
coverage.

## Success Criteria

- `tools/verify_host.sh --host-only` passes.
- `tools/verify_host.sh --full-build` passes.
- `firmware/nrf52/app/src/app_ble.c` keeps the same external BLE behavior while
  delegating Binding Control Point dispatch to the pure model.
- `docs/firmware-quality-plan.md` accurately describes the new evidence
  boundary.
