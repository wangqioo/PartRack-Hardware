#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "viberack_protocol.h"
#include "viberack_storage.h"

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

static void assert_record_equal(const vbrk_slot_record_t *a, const vbrk_slot_record_t *b)
{
    assert(memcmp(a, b, sizeof(*a)) == 0);
}

static void test_snapshot_round_trips_records_and_sequence(void)
{
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    vbrk_slot_record_t decoded[VBRK_SLOT_COUNT];
    vbrk_binding_snapshot_t snapshot;
    uint32_t decoded_seq = 0;

    memset(records, 0, sizeof(records));
    memset(decoded, 0, sizeof(decoded));
    records[0] = make_record(1, "C1234567", 12);
    records[6] = make_record(7, "R0402", 330);

    vbrk_binding_snapshot_encode(&snapshot, records, 42);

    assert(vbrk_binding_snapshot_decode(&snapshot, sizeof(snapshot), decoded, &decoded_seq) == 0);
    assert(decoded_seq == 42);
    assert_record_equal(&decoded[0], &records[0]);
    assert_record_equal(&decoded[6], &records[6]);
}

static void test_snapshot_rejects_bad_magic(void)
{
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    vbrk_binding_snapshot_t snapshot;

    memset(records, 0, sizeof(records));
    vbrk_binding_snapshot_encode(&snapshot, records, 1);
    snapshot.magic_le = 0;

    assert(vbrk_binding_snapshot_decode(&snapshot, sizeof(snapshot), records, NULL) == -EINVAL);
}

static void test_snapshot_rejects_bad_length(void)
{
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    vbrk_binding_snapshot_t snapshot;

    memset(records, 0, sizeof(records));
    vbrk_binding_snapshot_encode(&snapshot, records, 1);
    snapshot.length_le = 1;

    assert(vbrk_binding_snapshot_decode(&snapshot, sizeof(snapshot), records, NULL) == -EINVAL);
}

static void test_snapshot_rejects_bad_crc(void)
{
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    vbrk_binding_snapshot_t snapshot;

    memset(records, 0, sizeof(records));
    vbrk_binding_snapshot_encode(&snapshot, records, 1);
    snapshot.records[0].slot = 9;

    assert(vbrk_binding_snapshot_decode(&snapshot, sizeof(snapshot), records, NULL) == -EILSEQ);
}

int main(void)
{
    test_snapshot_round_trips_records_and_sequence();
    test_snapshot_rejects_bad_magic();
    test_snapshot_rejects_bad_length();
    test_snapshot_rejects_bad_crc();
    return 0;
}
