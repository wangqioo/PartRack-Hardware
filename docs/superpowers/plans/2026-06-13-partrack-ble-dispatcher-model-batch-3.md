# PartRack BLE Dispatcher Model Batch 3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract Binding Control Point command dispatch into a host-testable pure C model and wire firmware through it without changing APP-facing BLE behavior.

**Architecture:** Add `protocol/viberack_ble_dispatcher_model.[ch]` as the pure dispatcher. It validates request frames, calls table-operation callbacks, maps errno values to protocol status codes, and emits notify frames through callbacks. `firmware/nrf52/app/src/app_ble.c` remains the Zephyr adapter for GATT, real notifications, Table Info payloads, and advertising refresh.

**Tech Stack:** C11, existing `protocol/viberack_protocol.[ch]`, existing binding-table firmware API, shell verification through `tools/verify_host.sh --host-only`, Zephyr build through `tools/verify_host.sh --full-build`.

---

## File Map

- Create `protocol/viberack_ble_dispatcher_model.h`: pure dispatcher callback interface and dispatch function declaration.
- Create `protocol/viberack_ble_dispatcher_model.c`: Binding Control Point dispatch logic, status mapping, frame validation, notify sequencing.
- Create `tools/ble_dispatcher_model_test.c`: host tests for opcode lengths, status mapping, notify payloads, `READ_ALL`, Table Info notify behavior, and little-endian parsing.
- Modify `tools/verify_host.sh`: compile and run `tools/ble_dispatcher_model_test.c`.
- Modify `firmware/nrf52/app/src/app_ble.c`: replace inline Binding Control Point switch with calls into the pure dispatcher.
- Modify `firmware/nrf52/app/CMakeLists.txt`: compile `protocol/viberack_ble_dispatcher_model.c` into firmware.
- Modify `docs/firmware-quality-plan.md`: move BLE dispatcher model into existing host/model coverage.

## Task 1: Add Pure BLE Dispatcher Model

**Files:**
- Create: `protocol/viberack_ble_dispatcher_model.h`
- Create: `protocol/viberack_ble_dispatcher_model.c`

- [ ] **Step 1: Create the public header**

Create `protocol/viberack_ble_dispatcher_model.h`:

```c
#ifndef VIBERACK_BLE_DISPATCHER_MODEL_H
#define VIBERACK_BLE_DISPATCHER_MODEL_H

#include <stdint.h>

#include "viberack_protocol.h"

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

uint8_t vbrk_ble_status_from_errno(int err);
int vbrk_ble_dispatch_binding_cp(vbrk_ble_dispatcher_t *dispatcher,
                                 const uint8_t *frame, uint16_t len);

#endif
```

- [ ] **Step 2: Implement status mapping and endian helpers**

Create `protocol/viberack_ble_dispatcher_model.c` with this initial content:

```c
#include "viberack_ble_dispatcher_model.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

static uint16_t get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

uint8_t vbrk_ble_status_from_errno(int err)
{
    if (err == 0) {
        return VBRK_STATUS_OK;
    }
    if (err == -ENOSPC) {
        return VBRK_STATUS_ERR_FULL;
    }
    if (err == -EBUSY) {
        return VBRK_STATUS_ERR_FLASH_BUSY;
    }
    if (err == -EILSEQ) {
        return VBRK_STATUS_ERR_CRC;
    }

    return VBRK_STATUS_ERR_PARAM;
}
```

- [ ] **Step 3: Implement notify wrappers**

Append these helpers in `protocol/viberack_ble_dispatcher_model.c`:

```c
static void notify_binding(vbrk_ble_dispatcher_t *dispatcher, uint8_t op,
                           int err, const void *payload, uint16_t len)
{
    if (dispatcher == NULL || dispatcher->notify_binding == NULL) {
        return;
    }

    (void)dispatcher->notify_binding(op, vbrk_ble_status_from_errno(err),
                                     err == 0 ? payload : NULL,
                                     err == 0 ? len : 0,
                                     dispatcher->user_data);
}

static void notify_table_changed(vbrk_ble_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL || dispatcher->table_changed == NULL) {
        return;
    }

    dispatcher->table_changed(dispatcher->user_data);
}
```

- [ ] **Step 4: Implement callback validation**

Append:

```c
static int validate_dispatcher(const vbrk_ble_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL || dispatcher->read_one == NULL ||
        dispatcher->write_one == NULL || dispatcher->clear_one == NULL ||
        dispatcher->insert_at == NULL || dispatcher->remove_at == NULL ||
        dispatcher->move_block == NULL || dispatcher->set_qty == NULL ||
        dispatcher->factory_reset == NULL || dispatcher->notify_binding == NULL ||
        dispatcher->table_changed == NULL) {
        return -EINVAL;
    }

    return 0;
}
```

- [ ] **Step 5: Implement read operations**

Append:

```c
static int dispatch_read_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                             uint16_t len)
{
    vbrk_slot_record_t record;
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_READ_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->read_one(frame[1], &record, dispatcher->user_data);
    notify_binding(dispatcher, VBRK_OP_READ_ONE, err, &record, sizeof(record));
    return 0;
}

static int dispatch_read_all(vbrk_ble_dispatcher_t *dispatcher, uint16_t len)
{
    vbrk_slot_record_t record;
    uint8_t end_marker = VBRK_READ_ALL_END_MARKER;
    int err;

    if (len != 1) {
        notify_binding(dispatcher, VBRK_OP_READ_ALL, -EINVAL, NULL, 0);
        return 0;
    }

    for (uint8_t slot = 1; slot <= VBRK_SLOT_COUNT; slot++) {
        err = dispatcher->read_one(slot, &record, dispatcher->user_data);
        notify_binding(dispatcher, VBRK_OP_READ_ALL, err, &record, sizeof(record));
        if (err != 0) {
            return 0;
        }
    }

    notify_binding(dispatcher, VBRK_OP_READ_ALL, 0, &end_marker, sizeof(end_marker));
    return 0;
}
```

- [ ] **Step 6: Implement mutating operations**

Append:

```c
static int dispatch_mutating_status(vbrk_ble_dispatcher_t *dispatcher,
                                    uint8_t op, int err)
{
    notify_binding(dispatcher, op, err, NULL, 0);

    if (err == 0) {
        notify_table_changed(dispatcher);
    }

    return 0;
}

static int dispatch_write_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 1 + VBRK_SLOT_RECORD_SIZE) {
        notify_binding(dispatcher, VBRK_OP_WRITE_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->write_one((const vbrk_slot_record_t *)&frame[1],
                                dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_WRITE_ONE, err);
}

static int dispatch_clear_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_CLEAR_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->clear_one(frame[1], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_CLEAR_ONE, err);
}

static int dispatch_insert_at(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 2 + VBRK_SLOT_RECORD_SIZE) {
        notify_binding(dispatcher, VBRK_OP_INSERT_AT, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->insert_at(frame[1], (const vbrk_slot_record_t *)&frame[2],
                                dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_INSERT_AT, err);
}

static int dispatch_remove_at(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_REMOVE_AT, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->remove_at(frame[1], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_REMOVE_AT, err);
}

static int dispatch_move_block(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                               uint16_t len)
{
    int err;

    if (len != 4) {
        notify_binding(dispatcher, VBRK_OP_MOVE_BLOCK, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->move_block(frame[1], frame[2], frame[3], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_MOVE_BLOCK, err);
}

static int dispatch_set_qty(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                            uint16_t len)
{
    int err;

    if (len != 4) {
        notify_binding(dispatcher, VBRK_OP_SET_QTY, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->set_qty(frame[1], get_le16(&frame[2]), dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_SET_QTY, err);
}

static int dispatch_factory_reset(vbrk_ble_dispatcher_t *dispatcher,
                                  const uint8_t *frame, uint16_t len)
{
    int err;

    if (len != 5) {
        notify_binding(dispatcher, VBRK_OP_FACTORY_RESET, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->factory_reset(get_le32(&frame[1]), dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_FACTORY_RESET, err);
}
```

- [ ] **Step 7: Implement the public dispatcher**

Append:

```c
int vbrk_ble_dispatch_binding_cp(vbrk_ble_dispatcher_t *dispatcher,
                                 const uint8_t *frame, uint16_t len)
{
    uint8_t op;

    if (validate_dispatcher(dispatcher) != 0 || frame == NULL || len < 1) {
        return -EINVAL;
    }

    op = frame[0];
    switch (op) {
    case VBRK_OP_READ_ONE:
        return dispatch_read_one(dispatcher, frame, len);
    case VBRK_OP_READ_ALL:
        return dispatch_read_all(dispatcher, len);
    case VBRK_OP_WRITE_ONE:
        return dispatch_write_one(dispatcher, frame, len);
    case VBRK_OP_CLEAR_ONE:
        return dispatch_clear_one(dispatcher, frame, len);
    case VBRK_OP_INSERT_AT:
        return dispatch_insert_at(dispatcher, frame, len);
    case VBRK_OP_REMOVE_AT:
        return dispatch_remove_at(dispatcher, frame, len);
    case VBRK_OP_MOVE_BLOCK:
        return dispatch_move_block(dispatcher, frame, len);
    case VBRK_OP_SET_QTY:
        return dispatch_set_qty(dispatcher, frame, len);
    case VBRK_OP_FACTORY_RESET:
        return dispatch_factory_reset(dispatcher, frame, len);
    default:
        notify_binding(dispatcher, op, -EINVAL, NULL, 0);
        return 0;
    }
}
```

- [ ] **Step 8: Compile-check the model**

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol -c protocol/viberack_ble_dispatcher_model.c -o /tmp/viberack_ble_dispatcher_model.o
```

Expected: command exits 0 with no warnings.

- [ ] **Step 9: Commit**

Run:

```bash
git add protocol/viberack_ble_dispatcher_model.h protocol/viberack_ble_dispatcher_model.c
git commit -m "feat: add ble dispatcher model"
```

## Task 2: Add Host Tests for Dispatcher Behavior

**Files:**
- Create: `tools/ble_dispatcher_model_test.c`
- Modify: `tools/verify_host.sh`

- [ ] **Step 1: Create the test scaffold**

Create `tools/ble_dispatcher_model_test.c`:

```c
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "viberack_ble_dispatcher_model.h"

#define MAX_NOTIFIES 32
#define MAX_NOTIFY_LEN (2 + VBRK_SLOT_RECORD_SIZE)

typedef struct {
    uint8_t frames[MAX_NOTIFIES][MAX_NOTIFY_LEN];
    uint16_t lens[MAX_NOTIFIES];
    uint8_t count;
    uint8_t table_info_count;
    uint8_t read_fail_slot;
    int notify_result;
    int write_result;
    int insert_result;
    uint8_t last_set_qty_slot;
    uint16_t last_set_qty;
    uint32_t last_factory_magic;
} fake_ble_t;

static void reset_fake(fake_ble_t *fake)
{
    memset(fake, 0, sizeof(*fake));
}

static vbrk_slot_record_t make_record(uint8_t slot, const char *part_id, uint16_t qty)
{
    vbrk_slot_record_t record;
    size_t part_len = strlen(part_id);

    assert(part_len <= sizeof(record.part_id));
    memset(&record, 0, sizeof(record));
    record.slot = slot;
    memcpy(record.part_id, part_id, part_len);
    record.qty_le = qty;
    record.crc8 = vbrk_crc8_maxim((const uint8_t *)&record, VBRK_SLOT_RECORD_SIZE - 1);
    return record;
}
```

- [ ] **Step 2: Add fake callbacks**

Append:

```c
static int fake_read_one(uint8_t slot, vbrk_slot_record_t *record, void *user_data)
{
    fake_ble_t *fake = user_data;
    char part_id[4] = {'S', (char)('0' + (slot / 10)), (char)('0' + (slot % 10)), 0};

    if (slot == fake->read_fail_slot) {
        return -EINVAL;
    }

    *record = make_record(slot, part_id, (uint16_t)(slot * 10));
    return 0;
}

static int fake_write_one(const vbrk_slot_record_t *record, void *user_data)
{
    fake_ble_t *fake = user_data;

    if (fake->write_result != 0) {
        return fake->write_result;
    }
    if (record->crc8 != vbrk_crc8_maxim((const uint8_t *)record,
                                        VBRK_SLOT_RECORD_SIZE - 1)) {
        return -EILSEQ;
    }
    return 0;
}

static int fake_clear_one(uint8_t slot, void *user_data)
{
    (void)slot;
    (void)user_data;
    return 0;
}

static int fake_insert_at(uint8_t slot, const vbrk_slot_record_t *record, void *user_data)
{
    fake_ble_t *fake = user_data;

    (void)slot;
    (void)record;
    return fake->insert_result;
}

static int fake_remove_at(uint8_t slot, void *user_data)
{
    (void)slot;
    (void)user_data;
    return 0;
}

static int fake_move_block(uint8_t from, uint8_t to, uint8_t len, void *user_data)
{
    (void)from;
    (void)to;
    (void)len;
    (void)user_data;
    return 0;
}

static int fake_set_qty(uint8_t slot, uint16_t qty, void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->last_set_qty_slot = slot;
    fake->last_set_qty = qty;
    return 0;
}

static int fake_factory_reset(uint32_t magic, void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->last_factory_magic = magic;
    return magic == VBRK_FACTORY_RESET_MAGIC ? 0 : -EINVAL;
}
```

- [ ] **Step 3: Add fake notify callbacks and dispatcher initializer**

Append:

```c
static int fake_notify_binding(uint8_t op, uint8_t status, const void *payload,
                               uint16_t len, void *user_data)
{
    fake_ble_t *fake = user_data;

    assert(fake->count < MAX_NOTIFIES);
    assert(len <= VBRK_SLOT_RECORD_SIZE);
    fake->frames[fake->count][0] = op;
    fake->frames[fake->count][1] = status;
    if (payload != NULL && len > 0) {
        memcpy(&fake->frames[fake->count][2], payload, len);
    }
    fake->lens[fake->count] = (uint16_t)(2 + len);
    fake->count++;
    return fake->notify_result;
}

static void fake_table_changed(void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->table_info_count++;
}

static vbrk_ble_dispatcher_t make_dispatcher(fake_ble_t *fake)
{
    vbrk_ble_dispatcher_t dispatcher = {
        .read_one = fake_read_one,
        .write_one = fake_write_one,
        .clear_one = fake_clear_one,
        .insert_at = fake_insert_at,
        .remove_at = fake_remove_at,
        .move_block = fake_move_block,
        .set_qty = fake_set_qty,
        .factory_reset = fake_factory_reset,
        .notify_binding = fake_notify_binding,
        .table_changed = fake_table_changed,
        .user_data = fake,
    };

    return dispatcher;
}

static void assert_notify(const fake_ble_t *fake, uint8_t index, uint8_t op,
                          uint8_t status, uint16_t len)
{
    assert(index < fake->count);
    assert(fake->frames[index][0] == op);
    assert(fake->frames[index][1] == status);
    assert(fake->lens[index] == len);
}
```

- [ ] **Step 4: Add tests for malformed and read commands**

Append:

```c
static void test_empty_frame_emits_nothing(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, NULL, 0) == -EINVAL);
    assert(fake.count == 0);
    assert(fake.table_info_count == 0);
}

static void test_unknown_opcode_and_bad_lengths(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t unknown[] = {0x99};
    uint8_t read_one_bad[] = {VBRK_OP_READ_ONE};
    uint8_t set_qty_bad[] = {VBRK_OP_SET_QTY, 1, 2};

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, unknown, sizeof(unknown)) == 0);
    assert_notify(&fake, 0, 0x99, VBRK_STATUS_ERR_PARAM, 2);

    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, read_one_bad, sizeof(read_one_bad)) == 0);
    assert_notify(&fake, 1, VBRK_OP_READ_ONE, VBRK_STATUS_ERR_PARAM, 2);

    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, set_qty_bad, sizeof(set_qty_bad)) == 0);
    assert_notify(&fake, 2, VBRK_OP_SET_QTY, VBRK_STATUS_ERR_PARAM, 2);
    assert(fake.table_info_count == 0);
}

static void test_read_one_success_payload(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t frame[] = {VBRK_OP_READ_ONE, 7};
    const vbrk_slot_record_t *record;

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert_notify(&fake, 0, VBRK_OP_READ_ONE, VBRK_STATUS_OK,
                  2 + VBRK_SLOT_RECORD_SIZE);
    record = (const vbrk_slot_record_t *)&fake.frames[0][2];
    assert(record->slot == 7);
    assert(fake.table_info_count == 0);
}

static void test_read_all_success_and_failure(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t frame[] = {VBRK_OP_READ_ALL};

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert(fake.count == VBRK_SLOT_COUNT + 1);
    assert_notify(&fake, 0, VBRK_OP_READ_ALL, VBRK_STATUS_OK,
                  2 + VBRK_SLOT_RECORD_SIZE);
    assert_notify(&fake, VBRK_SLOT_COUNT, VBRK_OP_READ_ALL, VBRK_STATUS_OK, 3);
    assert(fake.frames[VBRK_SLOT_COUNT][2] == VBRK_READ_ALL_END_MARKER);
    assert(fake.table_info_count == 0);

    reset_fake(&fake);
    fake.read_fail_slot = 5;
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert(fake.count == 5);
    assert_notify(&fake, 4, VBRK_OP_READ_ALL, VBRK_STATUS_ERR_PARAM, 2);
}
```

- [ ] **Step 5: Add tests for write/status behavior**

Append:

```c
static void test_write_success_and_crc_failure(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t frame[1 + VBRK_SLOT_RECORD_SIZE];
    vbrk_slot_record_t record = make_record(3, "ABC", 12);

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    frame[0] = VBRK_OP_WRITE_ONE;
    memcpy(&frame[1], &record, sizeof(record));
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert_notify(&fake, 0, VBRK_OP_WRITE_ONE, VBRK_STATUS_OK, 2);
    assert(fake.table_info_count == 1);

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    record.crc8 ^= 0x01;
    memcpy(&frame[1], &record, sizeof(record));
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert_notify(&fake, 0, VBRK_OP_WRITE_ONE, VBRK_STATUS_ERR_CRC, 2);
    assert(fake.table_info_count == 0);
}

static void test_insert_full_maps_to_full_status(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t frame[2 + VBRK_SLOT_RECORD_SIZE];
    vbrk_slot_record_t record = make_record(1, "FULL", 1);

    reset_fake(&fake);
    fake.insert_result = -ENOSPC;
    dispatcher = make_dispatcher(&fake);
    frame[0] = VBRK_OP_INSERT_AT;
    frame[1] = 1;
    memcpy(&frame[2], &record, sizeof(record));
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert_notify(&fake, 0, VBRK_OP_INSERT_AT, VBRK_STATUS_ERR_FULL, 2);
    assert(fake.table_info_count == 0);
}
```

- [ ] **Step 6: Add tests for little-endian parsed commands**

Append:

```c
static void test_set_qty_and_factory_reset_parse_little_endian(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t set_qty[] = {VBRK_OP_SET_QTY, 4, 0x34, 0x12};
    uint8_t factory_reset[] = {VBRK_OP_FACTORY_RESET, 0xA5, 0xA5, 0x5A, 0x5A};

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, set_qty, sizeof(set_qty)) == 0);
    assert(fake.last_set_qty_slot == 4);
    assert(fake.last_set_qty == 0x1234);
    assert_notify(&fake, 0, VBRK_OP_SET_QTY, VBRK_STATUS_OK, 2);
    assert(fake.table_info_count == 1);

    reset_fake(&fake);
    dispatcher = make_dispatcher(&fake);
    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, factory_reset,
                                        sizeof(factory_reset)) == 0);
    assert(fake.last_factory_magic == VBRK_FACTORY_RESET_MAGIC);
    assert_notify(&fake, 0, VBRK_OP_FACTORY_RESET, VBRK_STATUS_OK, 2);
    assert(fake.table_info_count == 1);
}
```

- [ ] **Step 7: Add notify failure compatibility test**

Append:

```c
static void test_notify_failure_does_not_change_dispatch_result(void)
{
    fake_ble_t fake;
    vbrk_ble_dispatcher_t dispatcher;
    uint8_t frame[1 + VBRK_SLOT_RECORD_SIZE];
    vbrk_slot_record_t record = make_record(8, "NFAIL", 1);

    reset_fake(&fake);
    fake.notify_result = -ENOTCONN;
    dispatcher = make_dispatcher(&fake);
    frame[0] = VBRK_OP_WRITE_ONE;
    memcpy(&frame[1], &record, sizeof(record));

    assert(vbrk_ble_dispatch_binding_cp(&dispatcher, frame, sizeof(frame)) == 0);
    assert_notify(&fake, 0, VBRK_OP_WRITE_ONE, VBRK_STATUS_OK, 2);
    assert(fake.table_info_count == 1);
}
```

- [ ] **Step 8: Add main**

Append:

```c
int main(void)
{
    test_empty_frame_emits_nothing();
    test_unknown_opcode_and_bad_lengths();
    test_read_one_success_payload();
    test_read_all_success_and_failure();
    test_write_success_and_crc_failure();
    test_insert_full_maps_to_full_status();
    test_set_qty_and_factory_reset_parse_little_endian();
    test_notify_failure_does_not_change_dispatch_result();
    return 0;
}
```

- [ ] **Step 9: Wire into host verification**

Modify `tools/verify_host.sh` after the binding table core test block:

```bash
  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/ble_dispatcher_model_test.c protocol/viberack_protocol.c protocol/viberack_ble_dispatcher_model.c \
    -o /tmp/ble_dispatcher_model_test
  /tmp/ble_dispatcher_model_test
```

- [ ] **Step 10: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected output includes:

```text
protocol checks passed
binding table model checks passed
ble gatt smoke test vectors passed
```

The new C test is silent on success.

- [ ] **Step 11: Commit**

Run:

```bash
git add tools/ble_dispatcher_model_test.c tools/verify_host.sh
git commit -m "test: cover ble dispatcher model"
```

## Task 3: Route Firmware Through Dispatcher Model

**Files:**
- Modify: `firmware/nrf52/app/src/app_ble.c`
- Modify: `firmware/nrf52/app/CMakeLists.txt`

- [ ] **Step 1: Include the dispatcher header**

In `firmware/nrf52/app/src/app_ble.c`, add:

```c
#include "viberack_ble_dispatcher_model.h"
```

Keep existing includes needed for Zephyr BLE and `binding_table.h`.

- [ ] **Step 2: Remove local status mapping**

Delete the local `static uint8_t status_from_errno(int err)` function from
`app_ble.c`. Status mapping now lives in `vbrk_ble_status_from_errno`.

- [ ] **Step 3: Add adapter callbacks above `binding_cp_write`**

Add these static functions in `app_ble.c` before `binding_cp_write`:

```c
static int dispatcher_read_one(uint8_t slot, vbrk_slot_record_t *record, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_read_one(slot, record);
}

static int dispatcher_write_one(const vbrk_slot_record_t *record, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_write_one(record);
}

static int dispatcher_clear_one(uint8_t slot, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_clear_one(slot);
}

static int dispatcher_insert_at(uint8_t slot, const vbrk_slot_record_t *record,
                                void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_insert_at(slot, record);
}

static int dispatcher_remove_at(uint8_t slot, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_remove_at(slot);
}

static int dispatcher_move_block(uint8_t from, uint8_t to, uint8_t len, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_move_block(from, to, len);
}

static int dispatcher_set_qty(uint8_t slot, uint16_t qty, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_set_qty(slot, qty);
}

static int dispatcher_factory_reset(uint32_t magic, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_factory_reset(magic);
}

static int dispatcher_notify_binding(uint8_t op, uint8_t status, const void *payload,
                                     uint16_t len, void *user_data)
{
    ARG_UNUSED(user_data);
    return app_ble_notify_binding_result(op, status, payload, len);
}

static void dispatcher_table_changed(void *user_data)
{
    ARG_UNUSED(user_data);
    (void)app_ble_notify_table_info();
    app_ble_refresh_advertising();
}
```

- [ ] **Step 4: Replace the Binding CP switch with dispatcher call**

Replace the body of `binding_cp_write` after the offset/length guard with:

```c
    vbrk_ble_dispatcher_t dispatcher = {
        .read_one = dispatcher_read_one,
        .write_one = dispatcher_write_one,
        .clear_one = dispatcher_clear_one,
        .insert_at = dispatcher_insert_at,
        .remove_at = dispatcher_remove_at,
        .move_block = dispatcher_move_block,
        .set_qty = dispatcher_set_qty,
        .factory_reset = dispatcher_factory_reset,
        .notify_binding = dispatcher_notify_binding,
        .table_changed = dispatcher_table_changed,
        .user_data = NULL,
    };

    (void)vbrk_ble_dispatch_binding_cp(&dispatcher, frame, len);

    return len;
```

Keep the existing top guard:

```c
    if (offset != 0 || len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
```

This preserves current behavior: malformed empty writes use GATT invalid length;
opcode-level malformed frames emit Binding Control Point status notifies.

- [ ] **Step 5: Add model source to firmware CMake**

In `firmware/nrf52/app/CMakeLists.txt`, add:

```cmake
  ../../../protocol/viberack_ble_dispatcher_model.c
```

next to the other protocol sources.

- [ ] **Step 6: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected: all host checks pass.

- [ ] **Step 7: Run full build**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected: Zephyr build exits 0 and prints a UF2 path.

- [ ] **Step 8: Commit**

Run:

```bash
git add firmware/nrf52/app/src/app_ble.c firmware/nrf52/app/CMakeLists.txt
git commit -m "refactor: route binding control dispatch through model"
```

## Task 4: Update Quality Plan and Final Verification

**Files:**
- Modify: `docs/firmware-quality-plan.md`

- [ ] **Step 1: Update existing host/model coverage**

In `docs/firmware-quality-plan.md`, add this row under `## 已有 host 验证`:

```markdown
| `tools/ble_dispatcher_model_test.c` | Binding Control Point opcode 长度、状态码、notify payload、`READ_ALL` end marker、Table Info notify |
```

Remove this bullet from `## 下一批 host/model 验证`:

```markdown
- BLE dispatcher model：opcode 长度、状态码、notify payload、`READ_ALL` end marker、Table Info notify。
```

- [ ] **Step 2: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected: all host checks pass.

- [ ] **Step 3: Run full build**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected: Zephyr build exits 0 and prints a UF2 path.

- [ ] **Step 4: Check whitespace and status**

Run:

```bash
git diff --check
git status --short --branch
```

Expected: no whitespace errors; only `docs/firmware-quality-plan.md` is modified before commit.

- [ ] **Step 5: Commit**

Run:

```bash
git add docs/firmware-quality-plan.md
git commit -m "docs: record ble dispatcher host coverage"
```

- [ ] **Step 6: Final branch verification**

Run:

```bash
tools/verify_host.sh --host-only
tools/verify_host.sh --full-build
git status --short --branch
git log --oneline -8
```

Expected:

- host-only exits 0
- full-build exits 0 and prints UF2 path
- status is clean
- log shows Task 1-4 commits above the Batch 3 plan commit

## Self-Review Notes

- Spec coverage: The plan implements only the BLE dispatcher model batch. Advertising payload and light state machine remain future batches.
- Compatibility: The firmware still returns `BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN)` for empty/offset writes and still refreshes advertising after successful non-read Binding Control Point commands.
- Host evidence boundary: The new host test proves command-level dispatch behavior, not real BLE delivery, encryption, connection state, or notify timing.
