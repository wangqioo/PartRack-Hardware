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
