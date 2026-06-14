# PartRack BLE Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the BLE lifecycle Module the sole owner of BLE readiness, connection state, advertising invalidation, restart behavior, and notifications without changing APP-facing protocol behavior.

**Architecture:** Add a host-testable pure C lifecycle Module that owns state transitions and calls a small Adapter for scheduling, advertising, and notification effects. `app_ble.c` supplies the Zephyr Adapter; binding table, light control, and NFC FD report domain changes through the lifecycle Interface. Split BLE initialization from initial advertising so `main.c` explicitly runs `bt_enable() -> settings_load() -> advertising`.

**Tech Stack:** C11 host models, Zephyr/nRF Connect SDK, Zephyr Bluetooth GATT, Zephyr workqueue, devicetree overlays, shell verification.

---

## Starting State And File Structure

The worktree already contains user changes in:

- `firmware/nrf52/app/boards/xiao_ble.overlay`
- `firmware/nrf52/app/boards/xiao_ble_nrf52840_sense.overlay`
- `firmware/nrf52/app/src/light_control.c`

Keep those changes. The overlay edits establish the bare-XIAO variant, and the
`light_control.c` edits make physical output optional. Read the current diff
before every task that touches these files; do not revert or overwrite it.

New files:

- `protocol/viberack_ble_lifecycle.h` — lifecycle Interface, state, invalidation flags, Adapter contract.
- `protocol/viberack_ble_lifecycle.c` — pure lifecycle Implementation.
- `tools/ble_lifecycle_test.c` — host tests through the lifecycle Interface.

Modified files:

- `firmware/nrf52/app/src/app_ble.h` — domain-facing BLE lifecycle Interface.
- `firmware/nrf52/app/src/app_ble.c` — Zephyr Adapter and GATT integration.
- `firmware/nrf52/app/src/main.c` — explicit startup ordering.
- `firmware/nrf52/app/src/light_control.c` — report current light control state once per transition.
- `firmware/nrf52/app/src/nfc_wake.c` — report NFC FD without direct Zephyr BLE calls.
- `firmware/nrf52/app/CMakeLists.txt` — compile the lifecycle Module.
- `tools/ble_dispatcher_model_test.c` — preserve synchronous response ordering evidence.
- `tools/verify_host.sh` — run lifecycle tests and distinct XIAO build variants.
- `docs/development-plan.md` — record lifecycle evidence and remaining live-BLE gaps.
- `docs/xiao-nrf52840-bringup.md` — document bare and peripheral-enabled builds.
- `firmware/nrf52/README.md` — document build commands and startup ordering.

## Task 1: Build The Pure BLE Lifecycle Module

**Files:**
- Create: `protocol/viberack_ble_lifecycle.h`
- Create: `protocol/viberack_ble_lifecycle.c`
- Create: `tools/ble_lifecycle_test.c`
- Modify: `tools/verify_host.sh:60-67`

- [ ] **Step 1: Write the failing lifecycle tests**

Create `tools/ble_lifecycle_test.c` with a fake Adapter that counts scheduling,
advertising, and notification calls. Start with these state-transition tests:

```c
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "viberack_ble_lifecycle.h"

typedef struct {
    unsigned schedule_count;
    unsigned advertising_count;
    unsigned binding_notify_count;
    unsigned table_notify_count;
    unsigned light_notify_count;
    bool last_restart;
    bool invalidate_during_apply;
    int advertising_result;
    int notify_result;
    vbrk_ble_lifecycle_t *lifecycle;
} fake_ble_adapter_t;

static unsigned int fake_lock(void *user_data)
{
    (void)user_data;
    return 0;
}

static void fake_unlock(unsigned int key, void *user_data)
{
    (void)key;
    (void)user_data;
}

static void fake_schedule(void *user_data)
{
    fake_ble_adapter_t *fake = user_data;
    fake->schedule_count++;
}

static int fake_apply_advertising(bool restart, void *user_data)
{
    fake_ble_adapter_t *fake = user_data;
    fake->advertising_count++;
    fake->last_restart = restart;
    if (fake->invalidate_during_apply) {
        fake->invalidate_during_apply = false;
        vbrk_ble_lifecycle_report_nfc_fd(fake->lifecycle);
    }
    return fake->advertising_result;
}

static int fake_notify_binding(uint8_t op, uint8_t status,
                               const void *payload, uint16_t len,
                               void *user_data)
{
    fake_ble_adapter_t *fake = user_data;
    (void)op;
    (void)status;
    (void)payload;
    (void)len;
    fake->binding_notify_count++;
    return fake->notify_result;
}

static int fake_notify_table_info(void *user_data)
{
    fake_ble_adapter_t *fake = user_data;
    fake->table_notify_count++;
    return fake->notify_result;
}

static int fake_notify_light_status(uint8_t mode, uint16_t remaining_s,
                                    void *user_data)
{
    fake_ble_adapter_t *fake = user_data;
    (void)mode;
    (void)remaining_s;
    fake->light_notify_count++;
    return fake->notify_result;
}
```

Define a `make_lifecycle()` helper using this Adapter:

```c
static vbrk_ble_lifecycle_t make_lifecycle(fake_ble_adapter_t *fake)
{
    vbrk_ble_lifecycle_t lifecycle;
    vbrk_ble_lifecycle_adapter_t adapter = {
        .lock = fake_lock,
        .unlock = fake_unlock,
        .schedule_work = fake_schedule,
        .apply_advertising = fake_apply_advertising,
        .notify_binding = fake_notify_binding,
        .notify_table_info = fake_notify_table_info,
        .notify_light_status = fake_notify_light_status,
        .user_data = fake,
    };

    vbrk_ble_lifecycle_init(&lifecycle, &adapter);
    return lifecycle;
}
```

Add tests with these assertions:

```c
static void test_nfc_before_ready_defers_all_ble_calls(void)
{
    fake_ble_adapter_t fake = {0};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_report_nfc_fd(&lifecycle);

    assert(fake.schedule_count == 0);
    assert(fake.advertising_count == 0);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_initial_start_is_synchronous_and_fatal(void)
{
    fake_ble_adapter_t fake = {.advertising_result = -EIO};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_bluetooth_initialized(&lifecycle);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == -EIO);
    assert(fake.advertising_count == 1);
    assert(fake.last_restart);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_disconnected_invalidations_coalesce(void)
{
    fake_ble_adapter_t fake = {0};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_bluetooth_initialized(&lifecycle);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_FIND, 30);

    assert(fake.schedule_count == 1);
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.advertising_count == 2);
    assert(!fake.last_restart);
}

static void test_connected_change_waits_for_disconnect(void)
{
    fake_ble_adapter_t fake = {0};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_bluetooth_initialized(&lifecycle);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_connected(&lifecycle);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(fake.advertising_count == 1);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));

    vbrk_ble_lifecycle_disconnected(&lifecycle);
    assert(fake.schedule_count == 2);
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.advertising_count == 2);
    assert(fake.last_restart);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_runtime_failure_preserves_dirty_state(void)
{
    fake_ble_adapter_t fake = {0};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_bluetooth_initialized(&lifecycle);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    fake.advertising_result = -EIO;
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_invalidation_during_update_is_not_lost(void)
{
    fake_ble_adapter_t fake = {0};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    fake.lifecycle = &lifecycle;
    vbrk_ble_lifecycle_bluetooth_initialized(&lifecycle);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    fake.invalidate_during_apply = true;
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_notifications_are_online_only_and_non_durable(void)
{
    fake_ble_adapter_t fake = {.notify_result = -ENOTCONN};
    vbrk_ble_lifecycle_t lifecycle = make_lifecycle(&fake);

    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_PICK, 10);
    assert(fake.table_notify_count == 0);
    assert(fake.light_notify_count == 0);

    vbrk_ble_lifecycle_connected(&lifecycle);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_PICK, 10);
    assert(fake.table_notify_count == 1);
    assert(fake.light_notify_count == 1);
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/ble_lifecycle_test.c \
  protocol/viberack_protocol.c \
  protocol/viberack_ble_lifecycle.c \
  -o /tmp/ble_lifecycle_test
```

Expected: compilation fails because `viberack_ble_lifecycle.h` and its
Implementation do not exist.

- [ ] **Step 3: Define the lifecycle Interface**

Create `protocol/viberack_ble_lifecycle.h`:

```c
#ifndef VIBERACK_BLE_LIFECYCLE_H
#define VIBERACK_BLE_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    unsigned int (*lock)(void *user_data);
    void (*unlock)(unsigned int key, void *user_data);
    void (*schedule_work)(void *user_data);
    int (*apply_advertising)(bool restart, void *user_data);
    int (*notify_binding)(uint8_t op, uint8_t status,
                          const void *payload, uint16_t len,
                          void *user_data);
    int (*notify_table_info)(void *user_data);
    int (*notify_light_status)(uint8_t mode, uint16_t remaining_s,
                               void *user_data);
    void *user_data;
} vbrk_ble_lifecycle_adapter_t;

typedef struct {
    vbrk_ble_lifecycle_adapter_t adapter;
    bool bluetooth_initialized;
    bool ready;
    bool connected;
    bool advertising_running;
    bool work_pending;
    uint32_t advertising_generation;
    uint32_t applied_generation;
} vbrk_ble_lifecycle_t;

void vbrk_ble_lifecycle_init(vbrk_ble_lifecycle_t *lifecycle,
                             const vbrk_ble_lifecycle_adapter_t *adapter);
void vbrk_ble_lifecycle_bluetooth_initialized(vbrk_ble_lifecycle_t *lifecycle);
int vbrk_ble_lifecycle_start(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_connected(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_disconnected(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_nfc_fd(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_binding_changed(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_light_changed(vbrk_ble_lifecycle_t *lifecycle,
                                             uint8_t mode,
                                             uint16_t remaining_s);
int vbrk_ble_lifecycle_notify_binding(vbrk_ble_lifecycle_t *lifecycle,
                                      uint8_t op, uint8_t status,
                                      const void *payload, uint16_t len);
void vbrk_ble_lifecycle_process(vbrk_ble_lifecycle_t *lifecycle);
bool vbrk_ble_lifecycle_is_advertising_dirty(
    const vbrk_ble_lifecycle_t *lifecycle);

#endif
```

- [ ] **Step 4: Implement the minimal lifecycle state machine**

Create `protocol/viberack_ble_lifecycle.c`. Use these rules:

```c
static void mark_dirty(vbrk_ble_lifecycle_t *lifecycle)
{
    bool schedule = false;
    unsigned int key = lifecycle->adapter.lock(
        lifecycle->adapter.user_data);

    lifecycle->advertising_generation++;
    if (lifecycle->ready && !lifecycle->work_pending) {
        lifecycle->work_pending = true;
        schedule = true;
    }

    lifecycle->adapter.unlock(key, lifecycle->adapter.user_data);
    if (schedule) {
        lifecycle->adapter.schedule_work(lifecycle->adapter.user_data);
    }
}
```

`vbrk_ble_lifecycle_start()` must synchronously call
`apply_advertising(true, ...)`, return its error, and set `ready`,
`advertising_running`, and `applied_generation` only after success.

`vbrk_ble_lifecycle_process()` must:

```c
unsigned int key;
bool restart;
uint32_t target_generation;
int err;

key = lifecycle->adapter.lock(lifecycle->adapter.user_data);
lifecycle->work_pending = false;
if (!lifecycle->ready || lifecycle->connected ||
    lifecycle->advertising_generation == lifecycle->applied_generation) {
    lifecycle->adapter.unlock(key, lifecycle->adapter.user_data);
    return;
}

restart = !lifecycle->advertising_running;
target_generation = lifecycle->advertising_generation;
lifecycle->adapter.unlock(key, lifecycle->adapter.user_data);

err = lifecycle->adapter.apply_advertising(
    restart, lifecycle->adapter.user_data);

key = lifecycle->adapter.lock(lifecycle->adapter.user_data);
if (err == 0) {
    lifecycle->advertising_running = true;
    lifecycle->applied_generation = target_generation;
}
lifecycle->adapter.unlock(key, lifecycle->adapter.user_data);
```

`connected()` sets `connected=true` and `advertising_running=false`.
`disconnected()` sets `connected=false`, keeps advertising dirty, and schedules
work. The three report functions mark advertising dirty; binding and light
reports additionally attempt their notification only when connected.
`notify_binding()` returns `-ENOTCONN` when disconnected and never queues data.

Every lifecycle state read or write must be inside the Adapter's `lock` /
`unlock` pair. Copy the effect to perform into local variables, unlock, and only
then call `schedule_work`, `apply_advertising`, or a notification Adapter.
This keeps Zephyr Bluetooth calls outside the interrupt-locked critical
section. Generation counters prevent an advertising change reported during an
in-flight update from being cleared by the older update's success.

- [ ] **Step 5: Run the lifecycle test and fix only state-machine defects**

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/ble_lifecycle_test.c \
  protocol/viberack_protocol.c \
  protocol/viberack_ble_lifecycle.c \
  -o /tmp/ble_lifecycle_test &&
/tmp/ble_lifecycle_test
```

Expected: exit code `0`, no output.

- [ ] **Step 6: Add the lifecycle test to the host verification entry**

In `tools/verify_host.sh`, add:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/ble_lifecycle_test.c \
  protocol/viberack_protocol.c protocol/viberack_ble_lifecycle.c \
  -o /tmp/ble_lifecycle_test
/tmp/ble_lifecycle_test
```

Run:

```bash
tools/verify_host.sh --host-only
```

Expected ending:

```text
protocol checks passed
binding table model checks passed
ble gatt smoke test vectors passed
```

- [ ] **Step 7: Commit the pure lifecycle Module**

```bash
git add protocol/viberack_ble_lifecycle.h \
  protocol/viberack_ble_lifecycle.c \
  tools/ble_lifecycle_test.c \
  tools/verify_host.sh
git commit -m "feat: add ble lifecycle model"
```

## Task 2: Install The Zephyr Adapter And Split Startup

**Files:**
- Modify: `firmware/nrf52/app/src/app_ble.h`
- Modify: `firmware/nrf52/app/src/app_ble.c`
- Modify: `firmware/nrf52/app/src/main.c`
- Modify: `firmware/nrf52/app/CMakeLists.txt`

- [ ] **Step 1: Compile the lifecycle Module into firmware**

Add to `firmware/nrf52/app/CMakeLists.txt`:

```cmake
  ../../../protocol/viberack_ble_lifecycle.c
```

- [ ] **Step 2: Replace the public BLE Interface**

Change `firmware/nrf52/app/src/app_ble.h` to expose:

```c
int app_ble_init(void);
int app_ble_start(void);
int app_ble_notify_binding_result(uint8_t op, uint8_t status,
                                  const void *payload, uint16_t len);
void app_ble_report_binding_changed(void);
void app_ble_report_light_changed(void);
void app_ble_report_nfc_fd(void);
```

Remove these shallow operations:

```c
void app_ble_refresh_advertising(void);
int app_ble_notify_table_info(void);
int app_ble_notify_light_status(uint8_t mode, uint16_t remaining_s);
void app_ble_set_light_active(bool active);
```

- [ ] **Step 3: Add the lifecycle state and work item to `app_ble.c`**

Add:

```c
#include <zephyr/irq.h>
#include "viberack_ble_lifecycle.h"

static vbrk_ble_lifecycle_t ble_lifecycle;
static struct k_work ble_lifecycle_work;
```

Implement a work handler:

```c
static void ble_lifecycle_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    vbrk_ble_lifecycle_process(&ble_lifecycle);
}
```

Implement the scheduling Adapter:

```c
static unsigned int lifecycle_lock(void *user_data)
{
    ARG_UNUSED(user_data);
    return irq_lock();
}

static void lifecycle_unlock(unsigned int key, void *user_data)
{
    ARG_UNUSED(user_data);
    irq_unlock(key);
}

static void lifecycle_schedule_work(void *user_data)
{
    ARG_UNUSED(user_data);
    (void)k_work_submit(&ble_lifecycle_work);
}
```

- [ ] **Step 4: Move advertising effects behind the Adapter**

Replace direct refresh logic with:

```c
static int lifecycle_apply_advertising(bool restart, void *user_data)
{
    ARG_UNUSED(user_data);
    fill_adv_msd();

    if (restart) {
        return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                               ad, ARRAY_SIZE(ad),
                               sd, ARRAY_SIZE(sd));
    }

    return bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}
```

Change `fill_adv_msd()` so `light_active` comes from the authoritative light
control state:

```c
.light_active = light_control_mode() != VBRK_LIGHT_OFF,
```

Delete the duplicated `static bool light_active`.

- [ ] **Step 5: Move all notification effects behind the Adapter**

Convert the existing notification bodies into static Adapter functions:

```c
static int lifecycle_notify_binding(uint8_t op, uint8_t status,
                                    const void *payload, uint16_t len,
                                    void *user_data);
static int lifecycle_notify_table_info(void *user_data);
static int lifecycle_notify_light_status(uint8_t mode, uint16_t remaining_s,
                                         void *user_data);
```

Each function keeps the same attribute index and frame layout currently used by
`bt_gatt_notify()`. They return `-ENOTCONN` when `current_conn == NULL`.

Make the public synchronous result function delegate to the lifecycle:

```c
int app_ble_notify_binding_result(uint8_t op, uint8_t status,
                                  const void *payload, uint16_t len)
{
    return vbrk_ble_lifecycle_notify_binding(
        &ble_lifecycle, op, status, payload, len);
}
```

- [ ] **Step 6: Split BLE initialization from initial advertising**

Implement:

```c
int app_ble_init(void)
{
    vbrk_ble_lifecycle_adapter_t adapter = {
        .lock = lifecycle_lock,
        .unlock = lifecycle_unlock,
        .schedule_work = lifecycle_schedule_work,
        .apply_advertising = lifecycle_apply_advertising,
        .notify_binding = lifecycle_notify_binding,
        .notify_table_info = lifecycle_notify_table_info,
        .notify_light_status = lifecycle_notify_light_status,
        .user_data = NULL,
    };
    int err;

    k_work_init(&ble_lifecycle_work, ble_lifecycle_work_handler);
    vbrk_ble_lifecycle_init(&ble_lifecycle, &adapter);

    err = bt_enable(NULL);
    if (err != 0) {
        return err;
    }

    vbrk_ble_lifecycle_bluetooth_initialized(&ble_lifecycle);
    LOG_INF("Bluetooth initialized");
    return 0;
}

int app_ble_start(void)
{
    int err = vbrk_ble_lifecycle_start(&ble_lifecycle);

    if (err == 0) {
        LOG_INF("advertising started");
    }
    return err;
}
```

Remove `settings_load()` from `app_ble.c`.

- [ ] **Step 7: Make startup ordering explicit in `main.c`**

Add:

```c
#include <zephyr/settings/settings.h>
```

Replace the single old BLE start with:

```c
err = app_ble_init();
if (err != 0) {
    /* existing BLE error loop */
}

if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
    err = settings_load();
    if (err != 0) {
        /* use the existing BLE error loop */
    }
}

err = app_ble_start();
if (err != 0) {
    /* existing BLE error loop */
}
```

Create a private `fatal_ble_error(int err)` helper containing the existing
three-red-blink loop, and use it for `app_ble_init()`, `settings_load()`, and
`app_ble_start()` failures. Do not change the status-light behavior.

- [ ] **Step 8: Route connection callbacks through the lifecycle**

After acquiring `current_conn`, call:

```c
vbrk_ble_lifecycle_connected(&ble_lifecycle);
```

On disconnect, release `current_conn` first and then call:

```c
vbrk_ble_lifecycle_disconnected(&ble_lifecycle);
```

Delete direct `start_advertising()` from the disconnect callback. Restart now
occurs through the lifecycle work item.

- [ ] **Step 9: Build the bare XIAO firmware**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected: host checks pass and the XIAO Sense build produces:

```text
UF2: /Users/wq/ncs/build-partrack-PartRack-Hardware-xiao-sense/app/zephyr/zephyr.uf2
```

- [ ] **Step 10: Commit the Zephyr Adapter and startup split**

```bash
git add firmware/nrf52/app/src/app_ble.h \
  firmware/nrf52/app/src/app_ble.c \
  firmware/nrf52/app/src/main.c \
  firmware/nrf52/app/CMakeLists.txt
git commit -m "refactor: centralize ble lifecycle"
```

## Task 3: Migrate Domain Modules To Change Reports

**Files:**
- Modify: `firmware/nrf52/app/src/app_ble.c`
- Modify: `firmware/nrf52/app/src/light_control.c`
- Modify: `firmware/nrf52/app/src/nfc_wake.c`
- Modify: `tools/ble_dispatcher_model_test.c`

- [ ] **Step 1: Preserve Binding Control Point response ordering in its test**

Extend `fake_ble_t` in `tools/ble_dispatcher_model_test.c`:

```c
uint8_t callback_order[4];
uint8_t callback_order_count;
```

In `fake_notify_binding()` append `1`; in `fake_table_changed()` append `2`.
Then add to the successful write assertions:

```c
assert(fake.callback_order_count == 2);
assert(fake.callback_order[0] == 1);
assert(fake.callback_order[1] == 2);
```

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/ble_dispatcher_model_test.c \
  protocol/viberack_protocol.c protocol/viberack_ble_dispatcher_model.c \
  -o /tmp/ble_dispatcher_model_test &&
/tmp/ble_dispatcher_model_test
```

Expected: PASS. This locks in synchronous result notification before the
binding-table change report.

- [ ] **Step 2: Route binding table changes into the lifecycle**

Replace `dispatcher_table_changed()` with:

```c
static void dispatcher_table_changed(void *user_data)
{
    ARG_UNUSED(user_data);
    app_ble_report_binding_changed();
}
```

Implement:

```c
void app_ble_report_binding_changed(void)
{
    vbrk_ble_lifecycle_report_binding_changed(&ble_lifecycle);
}
```

The lifecycle Module attempts Table Info notify when connected and schedules
advertising invalidation separately.

- [ ] **Step 3: Route light control state into one report**

Implement in `app_ble.c`:

```c
void app_ble_report_light_changed(void)
{
    vbrk_ble_lifecycle_report_light_changed(
        &ble_lifecycle,
        light_control_mode(),
        light_control_remaining_s());
}
```

In `light_control.c`, replace each group of:

```c
app_ble_set_light_active(...);
app_ble_notify_light_status(...);
app_ble_refresh_advertising();
```

with:

```c
app_ble_report_light_changed();
```

Call it after the light state and physical output have reached their new state.
For timeout/off, the report observes mode `OFF` and remaining time `0`.

- [ ] **Step 4: Route NFC FD into one context-safe report**

In `nfc_wake.c`, replace:

```c
app_ble_refresh_advertising();
```

with:

```c
app_ble_report_nfc_fd();
```

Implement in `app_ble.c`:

```c
void app_ble_report_nfc_fd(void)
{
    vbrk_ble_lifecycle_report_nfc_fd(&ble_lifecycle);
}
```

The GPIO callback must contain no Zephyr Bluetooth function call.

- [ ] **Step 5: Prove the shallow operations are gone**

Run:

```bash
rg -n "app_ble_refresh_advertising|app_ble_set_light_active|app_ble_notify_table_info|app_ble_notify_light_status" \
  firmware/nrf52/app/src
```

Expected: no matches.

Run:

```bash
tools/verify_host.sh --host-only
```

Expected: all host checks pass.

- [ ] **Step 6: Build firmware after all caller migrations**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected: host checks and XIAO Sense firmware build pass.

- [ ] **Step 7: Commit the domain report migration**

```bash
git add firmware/nrf52/app/src/app_ble.c \
  firmware/nrf52/app/src/light_control.c \
  firmware/nrf52/app/src/nfc_wake.c \
  tools/ble_dispatcher_model_test.c
git commit -m "refactor: report domain changes to ble lifecycle"
```

## Task 4: Make Board Variant Evidence Explicit

**Files:**
- Modify: `tools/verify_host.sh`
- Preserve: `firmware/nrf52/app/boards/xiao_ble.overlay`
- Preserve: `firmware/nrf52/app/boards/xiao_ble_nrf52840_sense.overlay`
- Preserve: `firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi`

- [ ] **Step 1: Keep the bare-board overlays explicit**

Confirm the two XIAO board overlays keep the PartRack include disabled and
explain that they are the bare-board BLE variant:

```c
/* #include "xiao_ble_part_rack.dtsi" */
```

Do not re-enable the include in these files.

- [ ] **Step 2: Add distinct build directories and modes**

In `tools/verify_host.sh`, replace the single build directory with:

```bash
BARE_BUILD_DIR="${BARE_BUILD_DIR:-${NCS_DIR}/build-partrack-${ROOT_NAME}-xiao-sense-bare}"
PERIPHERAL_BUILD_DIR="${PERIPHERAL_BUILD_DIR:-${NCS_DIR}/build-partrack-${ROOT_NAME}-xiao-sense-peripherals}"
PART_RACK_OVERLAY="${ROOT_DIR}/firmware/nrf52/app/boards/xiao_ble_part_rack.dtsi"
```

Support:

```text
--host-only
--bare-build
--peripheral-build
--full-build
```

`--full-build` runs host checks and both builds.

- [ ] **Step 3: Implement the two build Adapter commands**

Bare build:

```bash
"${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
  "${ROOT_DIR}/firmware/nrf52/app" \
  -d "${BARE_BUILD_DIR}"
```

Peripheral-enabled build:

```bash
"${NCS_DIR}/.venv/bin/west" build -b "${BOARD}" \
  "${ROOT_DIR}/firmware/nrf52/app" \
  -d "${PERIPHERAL_BUILD_DIR}" \
  -- -Dapp_EXTRA_DTC_OVERLAY_FILE="${PART_RACK_OVERLAY}"
```

This exact `app_EXTRA_DTC_OVERLAY_FILE` form is verified against the installed
NCS 3.3.1 sysbuild. It must appear as a discovered overlay during CMake
configuration and in the generated `app/zephyr/zephyr.dts`.

- [ ] **Step 4: Assert the generated hardware evidence**

After the bare build:

```bash
if rg -q 'vbrk_ws2812|vbrk-led-strip|vbrk-nfc-fd' \
  "${BARE_BUILD_DIR}/app/zephyr/zephyr.dts"; then
  echo "bare build unexpectedly contains PartRack peripherals" >&2
  exit 1
fi
```

After the peripheral build:

```bash
rg -q 'vbrk_ws2812' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"
rg -q 'vbrk-led-strip' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"
rg -q 'vbrk-nfc-fd' "${PERIPHERAL_BUILD_DIR}/app/zephyr/zephyr.dts"
```

- [ ] **Step 5: Run each build separately**

Run:

```bash
tools/verify_host.sh --bare-build
tools/verify_host.sh --peripheral-build
```

Expected:

- Bare build succeeds and contains no PartRack peripheral nodes.
- Peripheral build succeeds and contains WS2812, power-gate, and NFC FD nodes.

- [ ] **Step 6: Run the combined verification**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected: host checks plus both firmware variants pass.

- [ ] **Step 7: Commit explicit board-variant evidence**

Stage the pre-existing overlay changes together with the verification change,
because they now form one coherent board-variant commit:

```bash
git add tools/verify_host.sh \
  firmware/nrf52/app/boards/xiao_ble.overlay \
  firmware/nrf52/app/boards/xiao_ble_nrf52840_sense.overlay
git commit -m "build: verify xiao hardware variants"
```

## Task 5: Update Evidence Documentation

**Files:**
- Modify: `docs/development-plan.md`
- Modify: `docs/xiao-nrf52840-bringup.md`
- Modify: `firmware/nrf52/README.md`

- [ ] **Step 1: Update the development evidence**

In `docs/development-plan.md`, record:

- BLE lifecycle state transitions have host-model coverage.
- NFC FD no longer calls Zephyr Bluetooth from the GPIO callback.
- Advertising refreshes coalesce and defer while connected.
- Notification failure remains non-fatal to domain operations.
- Live pairing, subscription behavior, disconnect timing, and radio behavior
  still require hardware evidence.

Replace statements that imply the default XIAO build compiles WS2812 with text
that names the peripheral-enabled variant.

- [ ] **Step 2: Document both XIAO build commands**

In `docs/xiao-nrf52840-bringup.md`, add:

```bash
tools/verify_host.sh --bare-build
tools/verify_host.sh --peripheral-build
```

State that the bare build is for BLE-only board validation and the peripheral
build compiles the WS2812, strip power, and NFC FD Adapter.

- [ ] **Step 3: Update the firmware README**

In `firmware/nrf52/README.md`, describe the startup order:

```text
domain initialization
-> Bluetooth initialization
-> settings_load()
-> initial advertising
```

Document that `--full-build` now builds both XIAO variants.

- [ ] **Step 4: Check documentation consistency**

Run:

```bash
rg -n "xiao_ble_nrf52840_sense.overlay.*加载|默认.*WS2812|app_ble_refresh_advertising" \
  README.md docs firmware/nrf52/README.md
```

Expected: no stale claim that the default bare overlay includes PartRack
peripherals, and no documentation reference to the removed refresh function.

- [ ] **Step 5: Run full verification**

Run:

```bash
tools/verify_host.sh --full-build
git diff --check
```

Expected: all tests and both builds pass; whitespace check is clean.

- [ ] **Step 6: Commit documentation**

```bash
git add docs/development-plan.md \
  docs/xiao-nrf52840-bringup.md \
  firmware/nrf52/README.md
git commit -m "docs: record ble lifecycle evidence"
```

## Task 6: Final Regression Review

**Files:**
- Review: `protocol/viberack_ble_lifecycle.[ch]`
- Review: `firmware/nrf52/app/src/app_ble.[ch]`
- Review: `firmware/nrf52/app/src/main.c`
- Review: `firmware/nrf52/app/src/light_control.c`
- Review: `firmware/nrf52/app/src/nfc_wake.c`
- Review: `tools/verify_host.sh`

- [ ] **Step 1: Verify protocol behavior did not change**

Run:

```bash
python3 tools/protocol_check.py
python3 tools/ble_gatt_smoke_test_test.py
```

Expected:

```text
protocol checks passed
ble gatt smoke test vectors passed
```

- [ ] **Step 2: Verify all host models**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected: exit code `0`.

- [ ] **Step 3: Verify both firmware variants from clean build directories**

Run:

```bash
VERIFY_TMP="$(mktemp -d)"
BARE_BUILD_DIR="${VERIFY_TMP}/bare" \
PERIPHERAL_BUILD_DIR="${VERIFY_TMP}/peripherals" \
tools/verify_host.sh --full-build
```

Expected: both builds reconfigure and compile successfully; generated DTS
assertions pass.

- [ ] **Step 4: Review ownership and removed leakage**

Run:

```bash
rg -n "bt_le_adv_|bt_gatt_notify|bt_conn_ref|bt_conn_unref" \
  firmware/nrf52/app/src
```

Expected: Zephyr BLE lifecycle calls appear only in `app_ble.c`.

Run:

```bash
rg -n "app_ble_" firmware/nrf52/app/src/light_control.c \
  firmware/nrf52/app/src/nfc_wake.c
```

Expected: only `app_ble_report_light_changed()` and
`app_ble_report_nfc_fd()` remain.

- [ ] **Step 5: Review git state**

Run:

```bash
git status --short
git log --oneline -6
```

Expected: no uncommitted implementation files and five focused implementation
commits after the design/plan commits.
