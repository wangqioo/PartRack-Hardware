#include "binding_table.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(binding_table, LOG_LEVEL_INF);

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

static void clear_record(uint8_t slot)
{
    memset(&records[slot - 1], 0, sizeof(records[slot - 1]));
}

static void commit_table(void)
{
    table_seq++;
    /* TODO: persist records and table_seq with Zephyr settings/NVS. */
}

int binding_table_init(void)
{
    memset(records, 0, sizeof(records));
    table_seq = 1;
    /* TODO: load records and table_seq from Zephyr settings/NVS. */
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

    if (record == NULL || !valid_slot(record->slot) || !record_crc_ok(record)) {
        return -EINVAL;
    }

    copy = *record;
    normalize_slot(&copy, copy.slot);
    records[copy.slot - 1] = copy;
    commit_table();

    return 0;
}

int binding_table_clear_one(uint8_t slot)
{
    if (!valid_slot(slot)) {
        return -EINVAL;
    }

    clear_record(slot);
    commit_table();
    return 0;
}

int binding_table_insert_at(uint8_t slot, const vbrk_slot_record_t *record)
{
    vbrk_slot_record_t copy;

    if (!valid_slot(slot) || record == NULL || !record_crc_ok(record)) {
        return -EINVAL;
    }

    if (records[VBRK_SLOT_COUNT - 1].slot != 0) {
        return -ENOSPC;
    }

    for (int i = VBRK_SLOT_COUNT - 1; i >= slot; i--) {
        records[i] = records[i - 1];
        if (records[i].slot != 0) {
            normalize_slot(&records[i], (uint8_t)(i + 1));
        }
    }

    copy = *record;
    normalize_slot(&copy, slot);
    records[slot - 1] = copy;
    commit_table();

    return 0;
}

int binding_table_remove_at(uint8_t slot)
{
    if (!valid_slot(slot)) {
        return -EINVAL;
    }

    for (uint8_t i = slot - 1; i < VBRK_SLOT_COUNT - 1; i++) {
        records[i] = records[i + 1];
        if (records[i].slot != 0) {
            normalize_slot(&records[i], (uint8_t)(i + 1));
        }
    }

    clear_record(VBRK_SLOT_COUNT);
    commit_table();

    return 0;
}

int binding_table_move_block(uint8_t from, uint8_t to, uint8_t len)
{
    vbrk_slot_record_t block[VBRK_SLOT_COUNT];
    vbrk_slot_record_t rest[VBRK_SLOT_COUNT];
    vbrk_slot_record_t temp[VBRK_SLOT_COUNT];
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
    memset(temp, 0, sizeof(temp));

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

    memcpy(temp, rest, insert_at * sizeof(temp[0]));
    memcpy(&temp[insert_at], block, len * sizeof(temp[0]));
    memcpy(&temp[insert_at + len], &rest[insert_at],
           (rest_count - insert_at) * sizeof(temp[0]));

    memcpy(records, temp, sizeof(records));
    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        if (records[i].slot != 0) {
            normalize_slot(&records[i], (uint8_t)(i + 1));
        }
    }

    commit_table();
    return 0;
}

int binding_table_set_qty(uint8_t slot, uint16_t qty)
{
    if (!valid_slot(slot) || records[slot - 1].slot == 0) {
        return -EINVAL;
    }

    records[slot - 1].qty_le = qty;
    normalize_slot(&records[slot - 1], slot);
    commit_table();

    return 0;
}

int binding_table_factory_reset(uint32_t magic)
{
    if (magic != VBRK_FACTORY_RESET_MAGIC) {
        return -EINVAL;
    }

    memset(records, 0, sizeof(records));
    commit_table();
    return 0;
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
