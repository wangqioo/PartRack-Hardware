#include "binding_table.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "viberack_binding_table_model.h"
#include "viberack_storage.h"

LOG_MODULE_REGISTER(binding_table, LOG_LEVEL_INF);

#define BINDING_TABLE_SETTINGS_TREE "vbrk"
#define BINDING_TABLE_SETTINGS_NAME "binding_table"
#define BINDING_TABLE_SETTINGS_KEY BINDING_TABLE_SETTINGS_TREE "/" BINDING_TABLE_SETTINGS_NAME

static vbrk_binding_table_model_t table_model;
static bool table_model_initialized;

static int persist_table(const vbrk_slot_record_t table[VBRK_SLOT_COUNT], uint32_t seq,
                         void *user_data);

static void ensure_table_model_initialized(void)
{
    if (!table_model_initialized) {
        vbrk_binding_table_model_init(&table_model, persist_table, NULL);
        table_model_initialized = true;
    }
}

static int persist_table(const vbrk_slot_record_t table[VBRK_SLOT_COUNT], uint32_t seq,
                         void *user_data)
{
    vbrk_binding_snapshot_t snapshot;

    ARG_UNUSED(user_data);
    vbrk_binding_snapshot_encode(&snapshot, table, seq);
    return settings_save_one(BINDING_TABLE_SETTINGS_KEY, &snapshot, sizeof(snapshot));
}

static int binding_table_settings_set(const char *key, size_t len, settings_read_cb read_cb,
                                      void *cb_arg)
{
    vbrk_binding_snapshot_t snapshot;
    vbrk_slot_record_t loaded_records[VBRK_SLOT_COUNT];
    uint32_t loaded_seq = 0;
    ssize_t read_len;
    int err;

    if (strcmp(key, BINDING_TABLE_SETTINGS_NAME) != 0) {
        return -ENOENT;
    }

    ensure_table_model_initialized();

    read_len = read_cb(cb_arg, &snapshot, sizeof(snapshot));
    if (read_len < 0) {
        LOG_WRN("binding table load failed: %d", (int)read_len);
        return (int)read_len;
    }

    err = vbrk_binding_snapshot_decode(&snapshot, (size_t)read_len, loaded_records,
                                       &loaded_seq);
    if (err != 0) {
        LOG_WRN("binding table snapshot invalid: %d", err);
        binding_table_init();
        return 0;
    }

    vbrk_binding_table_model_load(&table_model, loaded_records, loaded_seq);
    LOG_INF("binding table restored: seq=%u",
            vbrk_binding_table_model_seq(&table_model));
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(vbrk_binding_table, BINDING_TABLE_SETTINGS_TREE,
                               NULL, binding_table_settings_set, NULL, NULL);

int binding_table_init(void)
{
    vbrk_binding_table_model_init(&table_model, persist_table, NULL);
    table_model_initialized = true;
    return 0;
}

int binding_table_read_one(uint8_t slot, vbrk_slot_record_t *record)
{
    return vbrk_binding_table_model_read_one(&table_model, slot, record);
}

int binding_table_write_one(const vbrk_slot_record_t *record)
{
    return vbrk_binding_table_model_write_one(&table_model, record);
}

int binding_table_clear_one(uint8_t slot)
{
    return vbrk_binding_table_model_clear_one(&table_model, slot);
}

int binding_table_insert_at(uint8_t slot, const vbrk_slot_record_t *record)
{
    return vbrk_binding_table_model_insert_at(&table_model, slot, record);
}

int binding_table_remove_at(uint8_t slot)
{
    return vbrk_binding_table_model_remove_at(&table_model, slot);
}

int binding_table_move_block(uint8_t from, uint8_t to, uint8_t len)
{
    return vbrk_binding_table_model_move_block(&table_model, from, to, len);
}

int binding_table_set_qty(uint8_t slot, uint16_t qty)
{
    return vbrk_binding_table_model_set_qty(&table_model, slot, qty);
}

int binding_table_factory_reset(uint32_t magic)
{
    return vbrk_binding_table_model_factory_reset(&table_model, magic);
}

void binding_table_get_info(vbrk_table_info_t *info)
{
    vbrk_binding_table_model_get_info(&table_model, info);
}

uint32_t binding_table_seq(void)
{
    return vbrk_binding_table_model_seq(&table_model);
}

bool binding_table_has_unbound_slot(void)
{
    return vbrk_binding_table_model_has_unbound_slot(&table_model);
}
