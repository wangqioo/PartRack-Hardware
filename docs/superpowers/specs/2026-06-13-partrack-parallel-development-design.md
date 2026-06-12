# PartRack Parallel Development Design

Date: 2026-06-13

## Context

PartRack-Hardware already has a working hardware/firmware boundary:

- The repository owns hardware, nRF52 firmware, BLE/NFC contracts, protocol helpers, and validation tools.
- The Android app lives in `Yrd980/LCSC_android_erp` and is outside this repository.
- The current XIAO nRF52840 Sense bring-up has validated BLE advertising, connection, GATT discovery, `Table Info` read, and `Light Status` read.
- Binding table persistence, light control frames, WS2812 SPI output binding, and host-side protocol tests exist, but several real-hardware checks remain.

The next phase should not wait on a single hardware validation path. Hardware prototype preparation, APP BLE contract hardening, firmware quality work, and project integration control can progress in parallel.

## Goals

1. Create a shared integration control layer so parallel work does not diverge.
2. Prepare a hardware prototype package that a board/structure/procurement collaborator can act on.
3. Harden the APP BLE contract into a stable, testable handoff.
4. Improve firmware confidence without pretending host tests are hardware validation.
5. Clarify milestone order so NFC does not block the BLE binding-table and light-control minimum loop.

## Non-Goals

- Do not implement Android app screens, Room models, BOM matching, barcode workflows, or inventory UI in this repository.
- Do not claim real BLE, WS2812, NFC, ADC, low-power, or nRF52832 behavior is verified unless it has been tested on actual hardware.
- Do not merge XIAO development-board pin choices into the nRF52832 target pinmap without explicit review.
- Do not change BLE opcodes or frame formats as part of this documentation pass unless a separate protocol change is approved.

## Work Tracks

### Track D: Integration Control

This track owns the project-level truth table for parallel work.

Primary deliverable:

- `docs/integration-control.md`

The document should define:

- Current prototype target.
- v1 must-have capabilities.
- v1 deferred-but-retained capabilities.
- Track A/B/C ownership and boundaries.
- Cross-track frozen interfaces: pins, GATT/opcodes, NFC URI, storage semantics, power assumptions.
- Milestone mapping:
  - M0: BLE binding table plus light-control minimum loop.
  - M1: NFC point-to-connect and point-to-light loop.
  - M2: binding table recovery.
  - M3: BOM PICK demo.
  - v1 release gate.
- Decision log with date, decision, affected files, and follow-up.

Track D also updates existing documentation terminology:

- Replace stale `FDS` acceptance wording with Zephyr `settings/NVS` where it describes the current firmware path.
- Keep `roadmap.md` version-scoped, `development-plan.md` progress-scoped, and `milestones.md` acceptance-scoped.
- Mark shared pickup dock work as repository long-term scope, but not part of the current prototype batch.

### Track A: Hardware Prototype Package

This track owns board, LED strip, mechanical, procurement, and validation inputs.

Deliverables:

- `docs/hardware-prototype-package-v1.md`
- `hardware/main-board/pinmap-nrf52832-v1.md`
- `hardware/main-board/schematic-requirements-v1.md`
- `hardware/main-board/bom-prototype-v1.md`
- `hardware/led-strip/led-strip-spec-v1.md`
- `hardware/mechanical/prototype-mechanical-constraints-v1.md`
- `hardware/prototype-validation-checklist-v1.md`

The package must separate:

- XIAO nRF52840 Sense development-board facts.
- nRF52832 target-board decisions.
- Open decisions that must not be treated as frozen.

Required topics:

- nRF52832 pinmap: WS2812 data, light power gate, NT3H2111 I2C/FD, battery ADC, SWD, reset, logs, test points.
- WS2812 target-board peripheral plan and fallback options.
- Light-strip power gate topology, default state, leakage, inrush, ESD, and control polarity.
- WS2812 power/data level margin, series resistor, bulk capacitance, and connector pinout.
- NT3H2111 address, pull-ups, FD polarity, power domain, antenna/field constraints, and NDEF role.
- Battery ADC divider, sampling switch, input protection, calibration, and leakage budget.
- LDO current, quiescent current, dropout, capacitors, and BLE peak current margin.
- SWD, production flashing, reset, UART/logs if used, and mandatory test points.
- 25-slot LED strip physical spec: slot direction, numbering, pitch, mounting, copper, silkscreen, connector.
- Mechanical constraints: base, bins, magnets, guide/light windows, tolerances, and slot-to-light offset validation.

### Track B: APP BLE Contract Package

This track owns the Android-facing BLE/NFC contract and test vectors.

Deliverables:

- `docs/app-ble-contract-v0.1.md`
- `docs/app-ble-test-vectors.md`
- `docs/app-ble-retry-and-pairing.md`
- `docs/app-ble-smoke-checklist.md`
- `protocol/test-vectors.json`

The contract must include:

- Device scan rules and development placeholders.
- Manufacturer data layout, including Android `getManufacturerSpecificData()` offset differences.
- Binding Table Service UUID, Light Control Service UUID, characteristic UUIDs, permissions, and expected properties.
- 7-byte `Table Info` format.
- 16-byte slot record format and CRC-8/MAXIM rule.
- Binding opcodes, payload lengths, status codes, and response payloads.
- `READ_ALL` behavior: 25 record notifications plus `02 00 FF` end marker.
- Light Command 17-byte frame and Light Status 3-byte frame.
- Binding write encryption requirement and Android pairing/retry guidance.
- Notify ordering for write operations: Binding Control Point status notify, then Table Info notify when table state changes.
- Light Command confirmation rule: APP should read or subscribe to Light Status after Write Without Response.
- Error-code-to-APP-action mapping.
- Explicit warning that `VBRK-0000`, Company ID `0xFFFF`, `batch_id = 1`, NFC behavior, OTA, and battery ADC are development-stage or not-yet-integrated unless otherwise stated.

The machine-readable vectors in `protocol/test-vectors.json` should be canonical enough for Android tests and repository host tests to share.

### Track C: Firmware Quality Package

This track owns host-side confidence, test boundaries, and CI readiness.

Deliverables:

- `docs/firmware-quality-plan.md`
- `tools/binding_table_core_test.c`
- `tools/ble_dispatcher_model_test.c`
- `tools/adv_payload_test.c`
- `tools/light_state_machine_test.c`
- Updates to `tools/verify_host.sh` so host-only checks and Zephyr build checks can be run separately.

Immediate quality goals:

- Extract or model binding table pure operations for host tests:
  - `READ_ONE`
  - `READ_ALL`
  - `WRITE_ONE`
  - `CLEAR_ONE`
  - `INSERT_AT`
  - `REMOVE_AT`
  - `MOVE_BLOCK`
  - `SET_QTY`
  - `FACTORY_RESET`
- Test persistence atomicity with a fake persistence boundary:
  - Save success updates RAM and increments `table_seq`.
  - Save failure leaves RAM and `table_seq` unchanged.
- Model BLE dispatcher behavior without Zephyr BLE:
  - opcode length checks
  - unknown opcode handling
  - status mapping
  - notify payload shape
  - `READ_ALL` end marker
  - Table Info notify behavior
- Test advertising payload construction:
  - protocol version
  - company ID placeholder
  - battery percent
  - unbound-slot flag
  - light-active flag
  - low 16 bits of `table_seq`
- Test light state behavior with fake time:
  - command application
  - timeout to OFF
  - remaining seconds
  - OFF cancellation
  - repeated command timeout refresh

The quality plan must clearly label evidence categories:

- Host verified.
- Simulator/model verified.
- Zephyr build verified.
- Hardware required.

## Cross-Track Boundaries

- Track A consumes BLE and firmware constraints but does not redefine opcodes, APP behavior, or inventory workflows.
- Track B defines APP-facing behavior but does not promise hardware production status.
- Track C verifies firmware behavior but does not change board or product scope.
- Track D resolves terminology, milestone, and interface conflicts before they leak across tracks.

## First Implementation Batch

Batch 1 should be documentation-heavy and low-risk:

1. Add `docs/integration-control.md`.
2. Update stale milestone and storage wording in existing docs.
3. Add hardware prototype package entrypoint and nRF52832 pinmap draft.
4. Add APP BLE contract document and canonical test vector JSON.
5. Add firmware quality plan.
6. Split `tools/verify_host.sh` into host-only and full-build modes if it can be done without disrupting the current workflow.

Batch 2 can then add tests and deeper refactors:

1. Binding table core host tests.
2. BLE dispatcher host model tests.
3. Advertising payload host tests.
4. Light state machine fake-time tests.
5. CI workflow once host-only verification is stable.

## Acceptance Criteria

- A new reader can open `docs/integration-control.md` and understand the current prototype target, track boundaries, and milestone order.
- Hardware collaborators can open `docs/hardware-prototype-package-v1.md` and find every board, LED strip, mechanical, BOM, and validation document needed for prototype preparation.
- APP collaborators can open `docs/app-ble-contract-v0.1.md` and know exactly what is stable, what is a development placeholder, and how to handle pairing, retries, notifications, and light confirmation.
- Firmware contributors can open `docs/firmware-quality-plan.md` and see which behavior is already host/model/build verified and which behavior requires actual hardware.
- Existing docs no longer use stale `FDS` wording for current Zephyr `settings/NVS` acceptance criteria.
- M0 exists as a non-NFC minimum loop so work can continue before the NFC path is ready.

## Open Decisions

- Final nRF52832 pin assignments.
- WS2812 target-board peripheral path: SPI, PWM, I2S, or fallback design.
- Light-strip P-MOS topology and active polarity.
- Whether WS2812 data needs level shifting from 3.3 V when the strip is battery-powered.
- NT3H2111 FD polarity, antenna constraints, and NDEF write/update flow.
- LDO and battery ADC component choices.
- Whether OTA DFU is required for v1 beta or only for v1 release gate.
