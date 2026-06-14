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
    uint8_t callback_order[MAX_NOTIFIES];
    uint8_t callback_order_count;
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
    assert(fake->callback_order_count < sizeof(fake->callback_order));
    fake->callback_order[fake->callback_order_count++] = 1;
    return fake->notify_result;
}

static void fake_table_changed(void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->table_info_count++;
    assert(fake->callback_order_count < sizeof(fake->callback_order));
    fake->callback_order[fake->callback_order_count++] = 2;
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
    assert(fake.callback_order_count == 2);
    assert(fake.callback_order[0] == 1);
    assert(fake.callback_order[1] == 2);

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
