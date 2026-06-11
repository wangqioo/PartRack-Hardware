#ifndef BINDING_TABLE_H
#define BINDING_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

int binding_table_init(void);
int binding_table_read_one(uint8_t slot, vbrk_slot_record_t *record);
int binding_table_write_one(const vbrk_slot_record_t *record);
int binding_table_clear_one(uint8_t slot);
int binding_table_insert_at(uint8_t slot, const vbrk_slot_record_t *record);
int binding_table_remove_at(uint8_t slot);
int binding_table_move_block(uint8_t from, uint8_t to, uint8_t len);
int binding_table_set_qty(uint8_t slot, uint16_t qty);
int binding_table_factory_reset(uint32_t magic);
void binding_table_get_info(vbrk_table_info_t *info);
uint32_t binding_table_seq(void);
bool binding_table_has_unbound_slot(void);

#endif
