# PartRack BLE Lifecycle Design

## Context

The current BLE implementation centralizes Zephyr GATT declarations, but BLE
lifecycle knowledge still leaks into the binding table, light control, NFC FD,
and startup paths.

Callers currently need to know whether Bluetooth has been initialized, whether
advertising is active, whether the batch unit is connected, and whether their
execution context may call Zephyr Bluetooth functions directly. Examples
include:

- `nfc_wake.c` calling advertising refresh from the NFC FD GPIO callback.
- `light_control.c` calling Light Status notify and advertising refresh from
  command and timeout paths.
- `app_ble.c` loading global settings as an implicit part of BLE startup.
- binding table changes triggering Table Info notify and advertising refresh
  through dispatcher callbacks.

This makes the BLE lifecycle Module shallow. Its Interface exposes operations
such as advertising refresh, while callers must still understand most of the
Implementation conditions required to use them safely.

## Goal

Deepen the BLE lifecycle Module so it is the only owner of BLE initialization,
connection state, advertising state, advertising invalidation, and GATT
notifications.

Domain Modules report state changes without calling Zephyr Bluetooth operations
or depending on BLE execution-context rules.

## Scope

This design preserves the existing protocol and user-visible behavior while
allowing lifecycle defects to be corrected.

Included:

- BLE initialization and readiness.
- Advertising start, restart, invalidation, and coalesced refresh.
- Connection tracking.
- Binding Control Point, Table Info, and Light Status notifications.
- Context transfer from NFC FD callbacks and delayed work into a BLE-owned
  workqueue path.
- Explicit batch-unit startup ordering.
- Host tests for lifecycle state transitions and Adapter calls.
- Separate build evidence for bare XIAO and PartRack-peripheral XIAO variants.

Excluded:

- NFC-triggered 100 ms fast advertising for 30 seconds.
- Changes to BLE UUIDs, frame formats, permissions, pairing, or retry policy.
- Queuing notifications for disconnected or unsubscribed clients.
- Refactoring the board status LED loop.
- Refactoring the binding table or light control Module beyond replacing direct
  BLE calls with state-change reports.

## Domain Language

The canonical domain terms are recorded in the repository `CONTEXT.md`.

- A **batch unit** (`批次单元`) is the connected hardware device.
- The **binding table** (`绑定表`) is the batch unit's source of truth for slot
  records.
- **light control state** (`灯控状态`) is the current mode and remaining active
  time.
- **NFC FD** is the hardware field-detect signal.

## Architecture

### Deep BLE Lifecycle Module

The BLE lifecycle Module owns:

- Bluetooth initialization state.
- Connection ownership and connection reference lifetime.
- Advertising running and dirty state.
- Advertising payload reconstruction from the latest domain state.
- Advertising start after startup and restart after disconnect.
- Coalescing asynchronous state invalidations.
- Binding Control Point result notifications.
- Table Info notifications.
- Light Status notifications.
- Logging and retry eligibility for non-fatal BLE operation failures.

The Module does not own the binding table or light control state. It reads their
latest observable state when processing an invalidation.

### Domain Change Reports

The binding table, light control, and NFC FD Modules report that relevant state
has changed. These reports carry no snapshots.

The BLE lifecycle Module records invalidation flags and schedules one work item.
Multiple reports before the work item runs are coalesced. When the work item
runs, it reads the latest domain state, so stale intermediate snapshots cannot
be queued.

The invalidation categories are:

- binding table state changed
- light control state changed
- NFC FD occurred
- advertising data dirty

NFC FD is accepted before BLE readiness. It records an invalidation but does
not call Zephyr Bluetooth functions. This batch does not assign fast
advertising behavior to the event.

### Zephyr Bluetooth Adapter

Zephyr Bluetooth calls remain behind an internal Adapter seam. The Adapter
performs:

- `bt_enable`
- advertising start and data update
- connection reference and release
- `bt_gatt_notify`

Host tests use a fake Adapter through the same lifecycle Interface. The fake
Adapter is justified because the production Zephyr Adapter and host-test
Adapter provide two real uses of the seam.

### Batch-Unit Startup

Startup ordering becomes explicit outside the BLE lifecycle Implementation:

```text
initialize domain Modules
-> initialize Bluetooth
-> load global Zephyr settings
-> declare BLE lifecycle ready
-> start advertising
```

`settings_load()` remains global because Zephyr Bluetooth settings and binding
table settings share the same settings subsystem. It is no longer hidden
inside a function whose name only promises BLE startup.

NFC FD may be armed before BLE is ready because its callback only records an
invalidation. The BLE lifecycle Module processes pending invalidations after it
becomes ready.

## State and Event Rules

### Disconnected

When disconnected and ready:

- A binding table or light control invalidation marks advertising dirty.
- The scheduled work item reads the latest state and updates advertising.
- Consecutive invalidations before the work item runs produce one refresh.

### Connected

When connected:

- Domain changes mark advertising dirty.
- Advertising data is not updated while advertising is stopped.
- Relevant notifications may be attempted immediately.
- Dirty advertising state remains pending.

### Disconnect

On disconnect:

- The current connection reference is released.
- The latest binding table and light control state is projected into the
  advertising payload.
- Advertising restarts with the latest payload.
- A prior dirty state is cleared only after a successful restart.

### Notifications

Notifications describe online changes; they are not a durable event stream.

- A notification is attempted only when a connection exists.
- No notification is queued for later delivery.
- An unsubscribed client, missing connection, or notification error does not
  roll back binding table or light control state.
- Clients recover current state through GATT Read or reconnection.

### Binding Control Point Exception

Binding Control Point writes remain synchronous in the GATT write callback.

The callback:

1. validates and dispatches the request
2. performs the binding table operation
3. attempts the Binding Control Point result notification
4. reports binding table invalidation after a successful mutation

The request payload is not copied into the lifecycle workqueue. This preserves
response ordering and avoids introducing queued request-buffer lifetimes.

Advertising refresh caused by the successful mutation is asynchronous and
coalesced.

## Failure Semantics

Fatal startup failures:

- Bluetooth initialization failure.
- Initial advertising start failure.

These propagate to the existing batch-unit startup error path and red status
LED behavior.

Non-fatal runtime failures:

- Advertising data refresh failure.
- Advertising restart failure after disconnect.
- GATT notification failure.

Runtime failures are logged. Advertising remains dirty after a failed refresh
or restart so a later state change can retry. Notification failures do not
change domain-operation results.

## Board Configuration Evidence

The current default XIAO Sense overlay represents a bare-board BLE validation
variant, while `xiao_ble_part_rack.dtsi` represents the PartRack peripherals.
The build Interface must distinguish these intents.

Verification must provide separate evidence for:

- bare XIAO BLE startup without external peripherals
- XIAO with WS2812 strip, strip power gate, and NFC FD Adapter compiled in

The exact board configuration mechanism is an implementation-plan decision,
but a single ambiguous build must not be treated as evidence for both variants.

## Testing

The lifecycle Interface is the primary host-test surface.

Host tests must prove:

- NFC FD before BLE readiness performs no Zephyr Bluetooth call.
- Pending invalidations are processed after BLE becomes ready.
- Multiple disconnected-state changes coalesce into one advertising refresh.
- Connected-state changes mark advertising dirty without updating advertising.
- Disconnect restarts advertising using the latest binding table and light
  control state.
- A successful restart clears dirty state.
- A failed refresh or restart preserves dirty state.
- Notification failure does not change the originating domain-operation result.
- No notification is queued when disconnected.
- Binding Control Point result ordering remains synchronous.
- Startup orders Bluetooth initialization before `settings_load()` and initial
  advertising after `settings_load()`.

Existing protocol, binding table, light control, smoke-vector, and Zephyr build
checks must continue to pass.

## Documentation

Implementation must update:

- `docs/development-plan.md` to distinguish lifecycle host evidence from live
  BLE evidence.
- `docs/xiao-nrf52840-bringup.md` to name the bare-board and peripheral-enabled
  build variants.
- `firmware/nrf52/README.md` with the resulting build commands.

No ADR is required for this design. The choices are localized and reversible;
the design records sufficient context for the implementation.

## Success Criteria

- Domain Modules no longer call advertising update functions directly.
- NFC FD callbacks never call Zephyr Bluetooth functions.
- BLE lifecycle state and retry semantics have one Locality.
- Connected-state changes never attempt advertising updates.
- Disconnect advertising uses the latest domain state.
- Notification failures never roll back successful domain changes.
- Startup ordering is explicit and testable.
- Bare-board and peripheral-enabled XIAO builds provide distinct evidence.
- APP-facing BLE behavior and protocol bytes remain unchanged.
