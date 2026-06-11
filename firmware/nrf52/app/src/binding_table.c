#include "binding_table.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "viberack_storage.h"

LOG_MODULE_REGISTER(binding_table, LOG_LEVEL_INF);

#define BINDING_TABLE_SETTINGS_TREE "vbrk"
#define BINDING_TABLE_SETTINGS_NAME "binding_table"
#define BINDING_TABLE_SETTINGS_KEY BINDING_TABLE_SETTINGS_TREE "/" BINDING_TABLE_SETTINGS_NAME

static vbrk_slot_record_t records[VBRK_SLOT_COUNT];
static uint32_t table_seq;

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
    return record->crc8 == vbrk_crc8_maxim((const uint8_t *)record, VBRK_SLOT_RECORD_SIZE - 1);
}

static int persist_table(const vbrk_slot_record_t table[VBRK_SLOT_COUNT], uint32_t seq)
{
    vbrk_binding_snapshot_t snapshot;

    vbrk_binding_snapshot_encode(&snapshot, table, seq);
    return settings_save_one(BINDING_TABLE_SETTINGS_KEY, &snapshot, sizeof(snapshot));
}

static int commit_table(vbrk_slot_record_t next_records[VBRK_SLOT_COUNT])
{
    uint32_t next_seq = table_seq + 1;
    int err;

    err = persist_table(next_records, next_seq);
    if (err != 0) {
        return err;
    }

    memcpy(records, next_records, sizeof(records));
    table_seq = next_seq;
    return 0;
}

static int binding_table_settings_set(const char *key, size_t len, settings_read_cb read_cb,
                                      void *cb_arg)
{
    vbrk_binding_snapshot_t snapshot;
    ssize_t read_len;
    int err;

    if (strcmp(key, BINDING_TABLE_SETTINGS_NAME) != 0) {
        return -ENOENT;
    }

    read_len = read_cb(cb_arg, &snapshot, sizeof(snapshot));
    if (read_len < 0) {
        LOG_WRN("binding table load failed: %d", (int)read_len);
        return (int)read_len;
    }

    err = vbrk_binding_snapshot_decode(&snapshot, (size_t)read_len, records, &table_seq);
    if (err != 0) {
        LOG_WRN("binding table snapshot invalid: %d", err);
        memset(records, 0, sizeof(records));
        table_seq = 1;
        return 0;
    }

    if (table_seq == 0) {
        table_seq = 1;
    }

    LOG_INF("binding table restored: seq=%u", table_seq);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(vbrk_binding_table, BINDING_TABLE_SETTINGS_TREE,
                               NULL, binding_table_settings_set, NULL, NULL);

int binding_table_init(void)
{
    memset(records, 0, sizeof(records));
    table_seq = 1;
    return 0;
}

int binding_table_read_one(uint8_t slot, vbrk_slot_record_t *record)
{
    if (!valid_slot(slot) || record == NULL) {
        return -EINVAL;
    }

    *record = records[slot - 1];
    return 0;
}

int binding_table_write_one(const vbrk_slot_record_t *record)
{
    vbrk_slot_record_t copy;
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (record == NULL || !valid_slot(record->slot) || !record_crc_ok(record)) {
        return -EINVAL;
    }

    memcpy(next_records, records, sizeof(next_records));
    copy = *record;
    normalize_slot(&copy, copy.slot);
    next_records[copy.slot - 1] = copy;
    return commit_table(next_records);
}

int binding_table_clear_one(uint8_t slot)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (!valid_slot(slot)) {
        return -EINVAL;
    }

    memcpy(next_records, records, sizeof(next_records));
    memset(&next_records[slot - 1], 0, sizeof(next_records[slot - 1]));
    return commit_table(next_records);
}

int binding_table_insert_at(uint8_t slot, const vbrk_slot_record_t *record)
{
    vbrk_slot_record_t copy;
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (!valid_slot(slot) || record == NULL || !record_crc_ok(record)) {
        return -EINVAL;
    }

    if (records[VBRK_SLOT_COUNT - 1].slot != 0) {
        return -ENOSPC;
    }

    memcpy(next_records, records, sizeof(next_records));
    for (int i = VBRK_SLOT_COUNT - 1; i >= slot; i--) {
        next_records[i] = next_records[i - 1];
        if (next_records[i].slot != 0) {
            normalize_slot(&next_records[i], (uint8_t)(i + 1));
        }
    }

    copy = *record;
    normalize_slot(&copy, slot);
    next_records[slot - 1] = copy;
    return commit_table(next_records);
}

int binding_table_remove_at(uint8_t slot)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (!valid_slot(slot)) {
        return -EINVAL;
    }

    memcpy(next_records, records, sizeof(next_records));
    for (uint8_t i = slot - 1; i < VBRK_SLOT_COUNT - 1; i++) {
        next_records[i] = next_records[i + 1];
        if (next_records[i].slot != 0) {
            normalize_slot(&next_records[i], (uint8_t)(i + 1));
        }
    }

    memset(&next_records[VBRK_SLOT_COUNT - 1], 0, sizeof(next_records[VBRK_SLOT_COUNT - 1]));
    return commit_table(next_records);
}

int binding_table_move_block(uint8_t from, uint8_t to, uint8_t len)
{
    vbrk_slot_record_t block[VBRK_SLOT_COUNT];
    vbrk_slot_record_t rest[VBRK_SLOT_COUNT];
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];
    uint8_t rest_count = 0;
    uint8_t insert_at;

    if (!valid_slot(from) || !valid_slot(to) || len == 0 ||
        from + len - 1 > VBRK_SLOT_COUNT || to + len - 1 > VBRK_SLOT_COUNT) {
        return -EINVAL;
    }

    if (from == to) {
        return 0;
    }

    memcpy(block, &records[from - 1], len * sizeof(records[0]));
    memset(rest, 0, sizeof(rest));
    memset(next_records, 0, sizeof(next_records));

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        uint8_t slot = i + 1;

        if (slot >= from && slot < from + len) {
            continue;
        }

        rest[rest_count++] = records[i];
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

    return commit_table(next_records);
}

int binding_table_set_qty(uint8_t slot, uint16_t qty)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (!valid_slot(slot) || records[slot - 1].slot == 0) {
        return -EINVAL;
    }

    memcpy(next_records, records, sizeof(next_records));
    next_records[slot - 1].qty_le = qty;
    normalize_slot(&next_records[slot - 1], slot);
    return commit_table(next_records);
}

int binding_table_factory_reset(uint32_t magic)
{
    vbrk_slot_record_t next_records[VBRK_SLOT_COUNT];

    if (magic != VBRK_FACTORY_RESET_MAGIC) {
        return -EINVAL;
    }

    memset(next_records, 0, sizeof(next_records));
    return commit_table(next_records);
}

void binding_table_get_info(vbrk_table_info_t *info)
{
    uint16_t crc;

    if (info == NULL) {
        return;
    }

    crc = vbrk_crc16_ccitt_false((const uint8_t *)records, sizeof(records));
    info->table_seq_le = table_seq;
    info->crc16_le = crc;
    info->slot_count = VBRK_SLOT_COUNT;
}

uint32_t binding_table_seq(void)
{
    return table_seq;
}

bool binding_table_has_unbound_slot(void)
{
    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        if (records[i].slot == 0) {
            return true;
        }
    }

    return false;
}
