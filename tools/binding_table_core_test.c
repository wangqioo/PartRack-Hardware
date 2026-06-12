#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
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
    size_t part_len = strlen(part_id);

    assert(part_len <= sizeof(record.part_id));

    memset(&record, 0, sizeof(record));
    record.slot = slot;
    memcpy(record.part_id, part_id, part_len);
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
    if (memcmp(record->part_id, part_id, strlen(part_id)) != 0) {
        fprintf(stderr, "expected part_id %s, got %.10s in slot %u\n",
                part_id, record->part_id, (unsigned)record->slot);
    }
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
    vbrk_slot_record_t committed = make_record(1, "KEEP", 7);
    vbrk_slot_record_t rejected = make_record(2, "FAILSAVE", 9);
    vbrk_slot_record_t readback;

    init_model(&model, &store);

    assert(vbrk_binding_table_model_write_one(&model, &committed) == 0);
    assert(store.calls == 1);
    assert(store.saved_seq == 2);

    store.err = -EIO;

    assert(vbrk_binding_table_model_write_one(&model, &rejected) == -EIO);
    assert(store.calls == 2);
    assert(store.saved_seq == 2);
    assert(vbrk_binding_table_model_seq(&model) == 2);
    assert(vbrk_binding_table_model_read_one(&model, 1, &readback) == 0);
    assert_part_id(&readback, "KEEP");
    assert(readback.qty_le == 7);
    assert(vbrk_binding_table_model_read_one(&model, 2, &readback) == 0);
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

static void test_sparse_insert_shifts_only_to_next_empty_slot(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t a = make_record(1, "A", 1);
    vbrk_slot_record_t b = make_record(2, "B", 2);
    vbrk_slot_record_t c = make_record(3, "C", 3);
    vbrk_slot_record_t z = make_record(25, "Z", 25);
    vbrk_slot_record_t inserted = make_record(1, "INS", 9);
    vbrk_slot_record_t readback;

    init_model(&model, &store);
    assert(vbrk_binding_table_model_write_one(&model, &a) == 0);
    assert(vbrk_binding_table_model_write_one(&model, &b) == 0);
    assert(vbrk_binding_table_model_write_one(&model, &c) == 0);
    assert(vbrk_binding_table_model_write_one(&model, &z) == 0);

    assert(vbrk_binding_table_model_insert_at(&model, 2, &inserted) == 0);

    assert(vbrk_binding_table_model_read_one(&model, 2, &readback) == 0);
    assert_part_id(&readback, "INS");
    assert(readback.slot == 2);
    assert(vbrk_binding_table_model_read_one(&model, 3, &readback) == 0);
    assert_part_id(&readback, "B");
    assert(readback.slot == 3);
    assert(vbrk_binding_table_model_read_one(&model, 4, &readback) == 0);
    assert_part_id(&readback, "C");
    assert(readback.slot == 4);
    assert(vbrk_binding_table_model_read_one(&model, 5, &readback) == 0);
    assert(readback.slot == 0);
    assert(vbrk_binding_table_model_read_one(&model, 25, &readback) == 0);
    assert_part_id(&readback, "Z");
    assert(readback.slot == 25);
}

static void test_sequence_wrap_commits_seq_one(void)
{
    vbrk_binding_table_model_t model;
    fake_store_t store;
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    vbrk_slot_record_t record = make_record(1, "WRAP", 1);

    init_model(&model, &store);
    memset(records, 0, sizeof(records));
    vbrk_binding_table_model_load(&model, records, UINT32_MAX);

    assert(vbrk_binding_table_model_write_one(&model, &record) == 0);
    assert(vbrk_binding_table_model_seq(&model) == 1);
    assert(store.saved_seq == 1);
    assert(store.calls == 1);
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
    assert_part_id(&readback, "F");
    assert(vbrk_binding_table_model_read_one(&model, 5, &readback) == 0);
    assert_part_id(&readback, "B");
    assert(vbrk_binding_table_model_read_one(&model, 6, &readback) == 0);
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
    test_sparse_insert_shifts_only_to_next_empty_slot();
    test_sequence_wrap_commits_seq_one();
    test_move_block_matches_slot_order();
    test_set_qty_and_factory_reset();
    return 0;
}
