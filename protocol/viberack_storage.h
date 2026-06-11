#ifndef VIBERACK_STORAGE_H
#define VIBERACK_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "viberack_protocol.h"

#define VBRK_BINDING_SNAPSHOT_MAGIC 0x4B425256u
#define VBRK_BINDING_SNAPSHOT_VERSION 1u
#define VBRK_BINDING_SNAPSHOT_RECORDS_SIZE (VBRK_SLOT_COUNT * VBRK_SLOT_RECORD_SIZE)

typedef struct __attribute__((packed)) {
    uint32_t magic_le;
    uint16_t version_le;
    uint16_t length_le;
    uint32_t table_seq_le;
    vbrk_slot_record_t records[VBRK_SLOT_COUNT];
    uint16_t crc16_le;
} vbrk_binding_snapshot_t;

void vbrk_binding_snapshot_encode(vbrk_binding_snapshot_t *snapshot,
                                  const vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                  uint32_t table_seq);
int vbrk_binding_snapshot_decode(const vbrk_binding_snapshot_t *snapshot, size_t len,
                                 vbrk_slot_record_t records[VBRK_SLOT_COUNT],
                                 uint32_t *table_seq);

#endif
