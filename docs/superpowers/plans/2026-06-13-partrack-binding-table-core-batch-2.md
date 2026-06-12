# PartRack Binding Table Core Batch 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract binding-table mutation logic into a host-testable protocol module and prove persistence atomicity without real hardware.

**Architecture:** Add `protocol/viberack_binding_table_model.[ch]` as a pure C model that owns table state, table sequence, record normalization, CRC validation, table mutations, and a caller-provided persistence callback. The firmware `binding_table.c` becomes a Zephyr settings/NVS adapter around the pure model. Host tests exercise the same pure model used by firmware.

**Tech Stack:** C11, existing protocol structs/CRC helpers, errno-style return codes, shell verification through `tools/verify_host.sh --host-only`, Zephyr firmware build through existing CMake source list.

---

## File Map

- `protocol/viberack_binding_table_model.h`: public pure-model API used by firmware and host tests.
- `protocol/viberack_binding_table_model.c`: pure binding table model implementation.
- `tools/binding_table_core_test.c`: host tests for write, clear, insert, remove, move, set quantity, factory reset, info, unbound-slot checks, and persistence atomicity.
- `tools/verify_host.sh`: compile and run the new host test.
- `firmware/nrf52/app/src/binding_table.c`: replace local mutation logic with calls into the pure model while preserving current public firmware API.
- `firmware/nrf52/app/CMakeLists.txt`: include `protocol/viberack_binding_table_model.c` in the Zephyr app target.
- `docs/firmware-quality-plan.md`: mark binding table core/fake persistence as implemented host/model verification.

## Task 1: Add Pure Binding Table Model

**Files:**
- Create: `protocol/viberack_binding_table_model.h`
- Create: `protocol/viberack_binding_table_model.c`
- Test: compile-only through Task 2 tests

- [ ] **Step 1: Create the public header**

Create `protocol/viberack_binding_table_model.h`:

```c
#ifndef VIBERACK_BINDING_TABLE_MODEL_H
#define VIBERACK_BINDING_TABLE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

typedef int (*vbrk_binding_table_save_cb)(const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                          uint32_t table_seq, void *user_data);

typedef struct {
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    uint32_t table_seq;
    vbrk_binding_table_save_cb save;
    void *save_user_data;
} vbrk_binding_table_model_t;

void vbrk_binding_table_model_init(vbrk_binding_table_model_t *model,
                                   vbrk_binding_table_save_cb save,
                                   void *user_data);
void vbrk_binding_table_model_load(vbrk_binding_table_model_t *model,
                                   const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                   uint32_t table_seq);
int vbrk_binding_table_model_read_one(const vbrk_binding_table_model_t *model,
                                      uint8_t slot, vbrk_slot_record_t *record);
int vbrk_binding_table_model_write_one(vbrk_binding_table_model_t *model,
                                       const vbrk_slot_record_t *record);
int vbrk_binding_table_model_clear_one(vbrk_binding_table_model_t *model, uint8_t slot);
int vbrk_binding_table_model_insert_at(vbrk_binding_table_model_t *model, uint8_t slot,
                                       const vbrk_slot_record_t *record);
int vbrk_binding_table_model_remove_at(vbrk_binding_table_model_t *model, uint8_t slot);
int vbrk_binding_table_model_move_block(vbrk_binding_table_model_t *model,
                                        uint8_t from, uint8_t to, uint8_t len);
int vbrk_binding_table_model_set_qty(vbrk_binding_table_model_t *model,
                                     uint8_t slot, uint16_t qty);
int vbrk_binding_table_model_factory_reset(vbrk_binding_table_model_t *model,
                                           uint32_t magic);
void vbrk_binding_table_model_get_info(const vbrk_binding_table_model_t *model,
                                       vbrk_table_info_t *info);
uint32_t vbrk_binding_table_model_seq(const vbrk_binding_table_model_t *model);
bool vbrk_binding_table_model_has_unbound_slot(const vbrk_binding_table_model_t *model);

#endif
```

- [ ] **Step 2: Create the pure model implementation**

Create `protocol/viberack_binding_table_model.c`:

```c
#include "viberack_binding_table_model.h"

#include <errno.h>
#include <string.h>

static bool valid_slot(uint8_t slot)
{
    return slot >= 1 && slot <= VBRK_SLOT_COUNT;
}

static void normalize_slot(vbrk_slot_record_t *record, uint8_t slot)
{
    record->slot = slot;
    record->reserved = 0;
    record->crc8 = vbrk_crc8_maxim((const uint8_t *)record, VBRK_SLOT_RECORD_SIZE - 1);
}

static bool record_crc_ok(const vbrk_slot_record_t *record)
{
    return record->crc8 == vbrk_crc8_maxim((const uint8_t *)record,
                                           VBRK_SLOT_RECORD_SIZE - 1);
}

static int commit_table(vbrk_binding_table_model_t *model,
                        vbrk_slot_record_t next_records[VBRK_SLOT_COUNT])
{
    uint32_t next_seq;
    int err;

    if (model == NULL) {
        return -EINVAL;
    }

    next_seq = model->table_seq + 1;
    if (model->save != NULL) {
        err = model->save(next_records, next_seq, model->save_user_data);
        if (err != 0) {
            return err;
        }
    }

    memcpy(model->records, next_records, sizeof(model->records));
    model->table_seq = next_seq;
    return 0;
}

void vbrk_binding_table_model_init(vbrk_binding_table_model_t *model,
                                   vbrk_binding_table_save_cb save,
                                   void *user_data)
{
    if (model == NULL) {
        return;
    }

    memset(model->records, 0, sizeof(model->records));
    model->table_seq = 1;
    model->save = save;
    model->save_user_data = user_data;
}

void vbrk_binding_table_model_load(vbrk_binding_table_model_t *model,
                                   const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                   uint32_t table_seq)
{
    if (model == NULL || records == NULL) {
        return;
    }

    memcpy(model->records, records, sizeof(model->records));
    model->table_seq = table_seq == 0 ? 1 : table_seq;
}

int vbrk_binding_table_model_read_one(const vbrk_binding_table_model_t *model,
                                      uint8_t slot, vbrk_slot_record_t *record)
{
    if (model == NULL || !valid_slot(slot) || record == NULL) {
        return -EINVAL;
    }

    *record = model->records[slot - 1];
    return 0;
}

int vbrk_binding_table_model_write_one(vbrk_binding_table_model_t *model,
                                       const vbrk_slot_record_t *record)
{
    vbrk_slot_record_t copy;
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || record == NULL || !valid_slot(record->slot) ||
        !record_crc_ok(record)) {
        return -EINVAL;
    }

    memcpy(next_records, model->records, sizeof(next_records));
    copy = *record;
    normalize_slot(&copy, copy.slot);
    next_records[copy.slot - 1] = copy;
    return commit_table(model, next_records);
}

int vbrk_binding_table_model_clear_one(vbrk_binding_table_model_t *model, uint8_t slot)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || !valid_slot(slot)) {
        return -EINVAL;
    }

    memcpy(next_records, model->records, sizeof(next_records));
    memset(&next_records[slot - 1], 0, sizeof(next_records[slot - 1]));
    return commit_table(model, next_records);
}

int vbrk_binding_table_model_insert_at(vbrk_binding_table_model_t *model, uint8_t slot,
                                       const vbrk_slot_record_t *record)
{
    vbrk_slot_record_t copy;
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || !valid_slot(slot) || record == NULL || !record_crc_ok(record)) {
        return -EINVAL;
    }

    if (model->records[VBRK_SLOT_COUNT - 1].slot != 0) {
        return -ENOSPC;
    }

    memcpy(next_records, model->records, sizeof(next_records));
    for (int i = VBRK_SLOT_COUNT - 1; i >= slot; i--) {
        next_records[i] = next_records[i - 1];
        if (next_records[i].slot != 0) {
            normalize_slot(&next_records[i], (uint8_t)(i + 1));
        }
    }

    copy = *record;
    normalize_slot(&copy, slot);
    next_records[slot - 1] = copy;
    return commit_table(model, next_records);
}

int vbrk_binding_table_model_remove_at(vbrk_binding_table_model_t *model, uint8_t slot)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || !valid_slot(slot)) {
        return -EINVAL;
    }

    memcpy(next_records, model->records, sizeof(next_records));
    for (uint8_t i = slot - 1; i < VBRK_SLOT_COUNT - 1; i++) {
        next_records[i] = next_records[i + 1];
        if (next_records[i].slot != 0) {
            normalize_slot(&next_records[i], (uint8_t)(i + 1));
        }
    }

    memset(&next_records[VBRK_SLOT_COUNT - 1], 0, sizeof(next_records[VBRK_SLOT_COUNT - 1]));
    return commit_table(model, next_records);
}

int vbrk_binding_table_model_move_block(vbrk_binding_table_model_t *model,
                                        uint8_t from, uint8_t to, uint8_t len)
{
    vbrk_slot_record_t block[VBRK_SLOT_COUNT];
    vbrk_slot_record_t rest[VBRK_SLOT_COUNT];
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];
    uint8_t rest_count = 0;
    uint8_t insert_at;

    if (model == NULL || !valid_slot(from) || !valid_slot(to) || len == 0 ||
        from + len - 1 > VBRK_SLOT_COUNT || to + len - 1 > VBRK_SLOT_COUNT) {
        return -EINVAL;
    }

    if (from == to) {
        return 0;
    }

    memcpy(block, &model->records[from - 1], len * sizeof(model->records[0]));
    memset(rest, 0, sizeof(rest));
    memset(next_records, 0, sizeof(next_records));

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        uint8_t slot = i + 1;

        if (slot >= from && slot < from + len) {
            continue;
        }

        rest[rest_count++] = model->records[i];
    }

    insert_at = to - 1;
    if (insert_at > rest_count) {
        insert_at = rest_count;
    }

    memcpy(next_records, rest, insert_at * sizeof(next_records[0]));
    memcpy(&next_records[insert_at], block, len * sizeof(next_records[0]));
    memcpy(&next_records[insert_at + len], &rest[insert_at],
           (rest_count - insert_at) * sizeof(next_records[0]));

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        if (next_records[i].slot != 0) {
            normalize_slot(&next_records[i], (uint8_t)(i + 1));
        }
    }

    return commit_table(model, next_records);
}

int vbrk_binding_table_model_set_qty(vbrk_binding_table_model_t *model,
                                     uint8_t slot, uint16_t qty)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || !valid_slot(slot) || model->records[slot - 1].slot == 0) {
        return -EINVAL;
    }

    memcpy(next_records, model->records, sizeof(next_records));
    next_records[slot - 1].qty_le = qty;
    normalize_slot(&next_records[slot - 1], slot);
    return commit_table(model, next_records);
}

int vbrk_binding_table_model_factory_reset(vbrk_binding_table_model_t *model,
                                           uint32_t magic)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (model == NULL || magic != VBRK_FACTORY_RESET_MAGIC) {
        return -EINVAL;
    }

    memset(next_records, 0, sizeof(next_records));
    return commit_table(model, next_records);
}

void vbrk_binding_table_model_get_info(const vbrk_binding_table_model_t *model,
                                       vbrk_table_info_t *info)
{
    uint16_t crc;

    if (model == NULL || info == NULL) {
        return;
    }

    crc = vbrk_crc16_ccitt_false((const uint8_t *)model->records,
                                 sizeof(model->records));
    info->table_seq_le = model->table_seq;
    info->crc16_le = crc;
    info->slot_count = VBRK_SLOT_COUNT;
}

uint32_t vbrk_binding_table_model_seq(const vbrk_binding_table_model_t *model)
{
    return model == NULL ? 0 : model->table_seq;
}

bool vbrk_binding_table_model_has_unbound_slot(const vbrk_binding_table_model_t *model)
{
    if (model == NULL) {
        return false;
    }

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        if (model->records[i].slot == 0) {
            return true;
        }
    }

    return false;
}
```

- [ ] **Step 3: Compile the new module in isolation**

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol -c protocol/viberack_binding_table_model.c -o /tmp/viberack_binding_table_model.o
```

Expected:

- The command exits `0`.

- [ ] **Step 4: Commit Task 1**

Run:

```bash
git add protocol/viberack_binding_table_model.h protocol/viberack_binding_table_model.c
git commit -m "feat: add binding table model"
```

## Task 2: Add Binding Table Core Host Tests

**Files:**
- Create: `tools/binding_table_core_test.c`
- Modify: `tools/verify_host.sh`

- [ ] **Step 1: Create the host test**

Create `tools/binding_table_core_test.c`:

```c
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "viberack_binding_table_model.h"

typedef struct {
    int err;
    uint32_t saved_seq;
    vbrk_slot_record_t saved_records[VBRK_SLOT_COUNT];
    unsigned int calls;
} fake_store_t;

static vbrk_slot_record_t make_record(uint8_t slot, const char *part_id, uint16_t qty)
{
    vbrk_slot_record_t record;

    memset(&record, 0, sizeof(record));
    record.slot = slot;
    memcpy(record.part_id, part_id, strlen(part_id));
    record.qty_le = qty;
    record.flags = VBRK_SLOT_FLAG_LOW_STOCK;
    record.crc8 = vbrk_crc8_maxim((const uint8_t *)&record, VBRK_SLOT_RECORD_SIZE - 1);
    return record;
}

static int fake_save(const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                     uint32_t table_seq, void *user_data)
{
    fake_store_t *store = user_data;

    store->calls++;
    if (store->err != 0) {
        return store->err;
    }

    memcpy(store->saved_records, records, sizeof(store->saved_records));
    store->saved_seq = table_seq;
    return 0;
}

static void init_model(vbrk_binding_table_model_t *model, fake_store_t *store)
{
    memset(store, 0, sizeof(*store));
    vbrk_binding_table_model_init(model, fake_save, store);
}

static void assert_part_id(const vbrk_slot_record_t *record, const char *part_id)
{
    assert(memcmp(record->part_id, part_id, strlen(part_id)) == 0);
}

static void test_write_read_info_and_unbound_slot(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t record = make_record(3, "C1234567", 12);
    vbrk_slot_record_t readback;
    vbrk_table_info_t info;

    init_model(&model, &store);

    assert(vbrk_binding_table_model_has_unbound_slot(&model));
    assert(vbrk_binding_table_model_seq(&model) == 1);
    assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    assert(store.calls == 1);
    assert(store.saved_seq == 2);
    assert(vbrk_binding_table_model_seq(&model) == 2);

    assert(vbrk_binding_table_model_read_one(&model, 3, &readback) == 0);
    assert(readback.slot == 3);
    assert(readback.qty_le == 12);
    assert_part_id(&readback, "C1234567");
    assert(readback.reserved == 0);
    assert(readback.crc8 == vbrk_crc8_maxim((const uint8_t *)&readback,
                                            VBRK_SLOT_RECORD_SIZE - 1));

    memset(&info, 0, sizeof(info));
    vbrk_binding_table_model_get_info(&model, &info);
    assert(info.table_seq_le == 2);
    assert(info.slot_count == VBRK_SLOT_COUNT);
}

static void test_rejects_invalid_crc_without_persisting(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t record = make_record(1, "BADCRC", 1);

    init_model(&model, &store);
    record.crc8 ^= 0x55;

    assert(vbrk_binding_table_model_write_one(&model, &record) == -EINVAL);
    assert(store.calls == 0);
    assert(vbrk_binding_table_model_seq(&model) == 1);
}

static void test_save_failure_is_atomic(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t record = make_record(1, "FAILSAVE", 9);
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    store.err = -EIO;

    assert(vbrk_binding_table_model_write_one(&model, &record) == -EIO);
    assert(store.calls == 1);
    assert(vbrk_binding_table_model_seq(&model) == 1);
    assert(vbrk_binding_table_model_read_one(&model, 1, &readback) == 0);
    assert(readback.slot == 0);
}

static void test_clear_one(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t record = make_record(1, "CLEAR", 5);
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    assert(vbrk_binding_table_model_clear_one(&model, 1) == 0);
    assert(vbrk_binding_table_model_read_one(&model, 1, &readback) == 0);
    assert(readback.slot == 0);
    assert(vbrk_binding_table_model_seq(&model) == 3);
}

static void test_insert_remove_and_renumber(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t a = make_record(1, "A", 1);
    vbrk_slot_record_t b = make_record(2, "B", 2);
    vbrk_slot_record_t c = make_record(1, "C", 3);
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    assert(vbrk_binding_table_model_write_one(&model, &a) == 0);
    assert(vbrk_binding_table_model_write_one(&model, &b) == 0);
    assert(vbrk_binding_table_model_insert_at(&model, 2, &c) == 0);

    assert(vbrk_binding_table_model_read_one(&model, 2, &readback) == 0);
    assert(readback.slot == 2);
    assert_part_id(&readback, "C");
    assert(vbrk_binding_table_model_read_one(&model, 3, &readback) == 0);
    assert(readback.slot == 3);
    assert_part_id(&readback, "B");

    assert(vbrk_binding_table_model_remove_at(&model, 2) == 0);
    assert(vbrk_binding_table_model_read_one(&model, 2, &readback) == 0);
    assert(readback.slot == 2);
    assert_part_id(&readback, "B");
}

static void test_insert_full_table_returns_enospc(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;

    init_model(&model, &store);
    for (uint8_t slot = 1; slot <= VBRK_SLOT_COUNT; slot++) {
        vbrk_slot_record_t record = make_record(slot, "FULL", slot);
        assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    }

    vbrk_slot_record_t extra = make_record(1, "EXTRA", 1);
    unsigned int calls_before = store.calls;
    assert(!vbrk_binding_table_model_has_unbound_slot(&model));
    assert(vbrk_binding_table_model_insert_at(&model, 1, &extra) == -ENOSPC);
    assert(store.calls == calls_before);
}

static void test_move_block_matches_slot_order(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    for (uint8_t slot = 1; slot <= 6; slot++) {
        char part_id[2] = { (char)('A' + slot - 1), '\0' };
        vbrk_slot_record_t record = make_record(slot, part_id, slot);
        assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    }

    assert(vbrk_binding_table_model_move_block(&model, 2, 5, 2) == 0);

    assert(vbrk_binding_table_model_read_one(&model, 1, &readback) == 0);
    assert_part_id(&readback, "A");
    assert(vbrk_binding_table_model_read_one(&model, 2, &readback) == 0);
    assert_part_id(&readback, "D");
    assert(vbrk_binding_table_model_read_one(&model, 3, &readback) == 0);
    assert_part_id(&readback, "E");
    assert(vbrk_binding_table_model_read_one(&model, 4, &readback) == 0);
    assert_part_id(&readback, "B");
    assert(vbrk_binding_table_model_read_one(&model, 5, &readback) == 0);
    assert_part_id(&readback, "C");
}

static void test_set_qty_and_factory_reset(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t record = make_record(4, "QTY", 1);
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    assert(vbrk_binding_table_model_set_qty(&model, 4, 99) == -EINVAL);

    assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    assert(vbrk_binding_table_model_set_qty(&model, 4, 99) == 0);
    assert(vbrk_binding_table_model_read_one(&model, 4, &readback) == 0);
    assert(readback.qty_le == 99);
    assert(readback.crc8 == vbrk_crc8_maxim((const uint8_t *)&readback,
                                            VBRK_SLOT_RECORD_SIZE - 1));

    assert(vbrk_binding_table_model_factory_reset(&model, 0) == -EINVAL);
    assert(vbrk_binding_table_model_factory_reset(&model, VBRK_FACTORY_RESET_MAGIC) == 0);
    assert(vbrk_binding_table_model_read_one(&model, 4, &readback) == 0);
    assert(readback.slot == 0);
}

int main(void)
{
    test_write_read_info_and_unbound_slot();
    test_rejects_invalid_crc_without_persisting();
    test_save_failure_is_atomic();
    test_clear_one();
    test_insert_remove_and_renumber();
    test_insert_full_table_returns_enospc();
    test_move_block_matches_slot_order();
    test_set_qty_and_factory_reset();
    return 0;
}
```

- [ ] **Step 2: Add the host test to verification**

Modify `tools/verify_host.sh`. After the `storage_snapshot_test` compile/run block and before Python checks, add:

```bash
  cc -std=c11 -Wall -Wextra -Iprotocol \
    tools/binding_table_core_test.c protocol/viberack_protocol.c protocol/viberack_binding_table_model.c \
    -o /tmp/binding_table_core_test
  /tmp/binding_table_core_test
```

- [ ] **Step 3: Run the new host test**

Run:

```bash
cc -std=c11 -Wall -Wextra -Iprotocol \
  tools/binding_table_core_test.c protocol/viberack_protocol.c protocol/viberack_binding_table_model.c \
  -o /tmp/binding_table_core_test
/tmp/binding_table_core_test
```

Expected:

- The command exits `0`.

- [ ] **Step 4: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected:

- The command exits `0`.
- Existing host tests still pass.

- [ ] **Step 5: Commit Task 2**

Run:

```bash
git add tools/binding_table_core_test.c tools/verify_host.sh
git commit -m "test: cover binding table core model"
```

## Task 3: Route Firmware Binding Table Through Model

**Files:**
- Modify: `firmware/nrf52/app/src/binding_table.c`
- Modify: `firmware/nrf52/app/CMakeLists.txt`

- [ ] **Step 1: Replace firmware local state with model adapter**

Modify `firmware/nrf52/app/src/binding_table.c` so it keeps Zephyr settings code but delegates table operations to `vbrk_binding_table_model_t`.

The final file should have these key changes:

1. Include the model header:

```c
#include "viberack_binding_table_model.h"
```

2. Replace:

```c
static vbrk_slot_record_t records[VBRK_SLOT_COUNT];
static uint32_t table_seq;
```

with:

```c
static vbrk_binding_table_model_t table_model;
```

3. Remove local `valid_slot`, `normalize_slot`, `record_crc_ok`, and `commit_table` helpers.

4. Replace `persist_table` with this save callback:

```c
static int persist_table(const vbrk_slot_record_t table[VBRK_SLOT_COUNT], uint32_t seq,
                         void *user_data)
{
    vbrk_binding_snapshot_t snapshot;

    ARG_UNUSED(user_data);
    vbrk_binding_snapshot_encode(&snapshot, table, seq);
    return settings_save_one(BINDING_TABLE_SETTINGS_KEY, &snapshot, sizeof(snapshot));
}
```

5. In `binding_table_settings_set`, decode into a local table and then load the model:

```c
vbrk_slot_record_t loaded_records[VBRK_SLOT_COUNT];
uint32_t loaded_seq = 0;
```

Use:

```c
err = vbrk_binding_snapshot_decode(&snapshot, (size_t)read_len, loaded_records, &loaded_seq);
if (err != 0) {
    LOG_WRN("binding table snapshot invalid: %d", err);
    binding_table_init();
    return 0;
}

vbrk_binding_table_model_load(&table_model, loaded_records, loaded_seq);
LOG_INF("binding table restored: seq=%u", vbrk_binding_table_model_seq(&table_model));
return 0;
```

6. `binding_table_init` should initialize the model with the save callback:

```c
int binding_table_init(void)
{
    vbrk_binding_table_model_init(&table_model, persist_table, NULL);
    return 0;
}
```

7. Each public operation should delegate:

```c
return vbrk_binding_table_model_read_one(&table_model, slot, record);
return vbrk_binding_table_model_write_one(&table_model, record);
return vbrk_binding_table_model_clear_one(&table_model, slot);
return vbrk_binding_table_model_insert_at(&table_model, slot, record);
return vbrk_binding_table_model_remove_at(&table_model, slot);
return vbrk_binding_table_model_move_block(&table_model, from, to, len);
return vbrk_binding_table_model_set_qty(&table_model, slot, qty);
return vbrk_binding_table_model_factory_reset(&table_model, magic);
vbrk_binding_table_model_get_info(&table_model, info);
return vbrk_binding_table_model_seq(&table_model);
return vbrk_binding_table_model_has_unbound_slot(&table_model);
```

- [ ] **Step 2: Add model source to Zephyr build**

Modify `firmware/nrf52/app/CMakeLists.txt`. Add this source near the other protocol sources:

```cmake
  ../../../protocol/viberack_binding_table_model.c
```

- [ ] **Step 3: Run host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected:

- The command exits `0`.

- [ ] **Step 4: Run a firmware compile check if the local NCS environment is available**

Run:

```bash
tools/verify_host.sh --full-build
```

Expected:

- If the local NCS environment is available, the command exits `0`.
- If it fails because of local Zephyr/NCS environment or toolchain setup, capture the exact error and report it; do not hide it.

- [ ] **Step 5: Commit Task 3**

If `--full-build` passes or fails only due to local environment setup, commit:

```bash
git add firmware/nrf52/app/src/binding_table.c firmware/nrf52/app/CMakeLists.txt
git commit -m "refactor: use binding table model in firmware"
```

## Task 4: Update Quality Plan And Final Verification

**Files:**
- Modify: `docs/firmware-quality-plan.md`

- [ ] **Step 1: Update implemented evidence**

In `docs/firmware-quality-plan.md`, under `## 已有 host 验证`, add:

```markdown
| `tools/binding_table_core_test.c` | 绑定表 write/read/clear/insert/remove/move/set qty/factory reset、`table_seq`、CRC、保存失败原子性 |
```

In `## 下一批 host/model 验证`, remove:

```markdown
- Binding table core：所有表操作、CRC 拒绝、满表、空槽、重排、`table_seq`。
- Fake persistence：保存失败时 RAM 和 `table_seq` 不变。
```

Keep the BLE dispatcher, advertising payload, and light state machine bullets for future batches.

- [ ] **Step 2: Run final host verification**

Run:

```bash
tools/verify_host.sh --host-only
```

Expected:

- The command exits `0`.

- [ ] **Step 3: Check worktree status**

Run:

```bash
git status --short --branch
```

Expected:

- Only `docs/firmware-quality-plan.md` is modified before commit.

- [ ] **Step 4: Commit Task 4**

Run:

```bash
git add docs/firmware-quality-plan.md
git commit -m "docs: record binding table host coverage"
```

## Final Review

- Run:

```bash
tools/verify_host.sh --host-only
git log --oneline -8
git status --short --branch
```

- Expected:
  - Host verification exits `0`.
  - Branch is clean.
  - Recent commits show the model, test, firmware routing, and docs update.

## Self-Review Notes

- Spec coverage: This plan implements only the binding-table core portion of the Batch 2 quality work. BLE dispatcher, advertising payload, and light state machine tests remain future batches.
- Hardware boundary: No task claims real BLE, flash restart, WS2812, NFC, ADC, or low-power verification.
- Risk: Task 3 changes firmware code path. It must be reviewed carefully and should run `--full-build` when the local NCS environment is usable.
