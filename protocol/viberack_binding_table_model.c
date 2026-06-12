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
