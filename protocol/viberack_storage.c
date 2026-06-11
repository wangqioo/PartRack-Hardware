#include "viberack_storage.h"

#include <errno.h>
#include <string.h>

static uint16_t snapshot_crc16(const vbrk_binding_snapshot_t *snapshot)
{
    return vbrk_crc16_ccitt_false((const uint8_t *)snapshot,
                                  offsetof(vbrk_binding_snapshot_t, crc16_le));
}

void vbrk_binding_snapshot_encode(vbrk_binding_snapshot_t *snapshot,
                                  const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                  uint32_t table_seq)
{
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->magic_le = VBRK_BINDING_SNAPSHOT_MAGIC;
    snapshot->version_le = VBRK_BINDING_SNAPSHOT_VERSION;
    snapshot->length_le = VBRK_BINDING_SNAPSHOT_RECORDS_SIZE;
    snapshot->table_seq_le = table_seq;
    memcpy(snapshot->records, records, VBRK_BINDING_SNAPSHOT_RECORDS_SIZE);
    snapshot->crc16_le = snapshot_crc16(snapshot);
}

int vbrk_binding_snapshot_decode(const vbrk_binding_snapshot_t *snapshot, size_t len,
                                 vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                 uint32_t *table_seq)
{
    if (snapshot == NULL || records == NULL || len != sizeof(*snapshot)) {
        return -EINVAL;
    }

    if (snapshot->magic_le != VBRK_BINDING_SNAPSHOT_MAGIC ||
        snapshot->version_le != VBRK_BINDING_SNAPSHOT_VERSION ||
        snapshot->length_le != VBRK_BINDING_SNAPSHOT_RECORDS_SIZE) {
        return -EINVAL;
    }

    if (snapshot->crc16_le != snapshot_crc16(snapshot)) {
        return -EILSEQ;
    }

    memcpy(records, snapshot->records, VBRK_BINDING_SNAPSHOT_RECORDS_SIZE);
    if (table_seq != NULL) {
        *table_seq = snapshot->table_seq_le;
    }

    return 0;
}
