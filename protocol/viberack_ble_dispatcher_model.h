#ifndef VIBERACK_BLE_DISPATCHER_MODEL_H
#define VIBERACK_BLE_DISPATCHER_MODEL_H

#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    int (*read_one)(uint8_t slot, vbrk_slot_record_t *record, void *user_data);
    int (*write_one)(const vbrk_slot_record_t *record, void *user_data);
    int (*clear_one)(uint8_t slot, void *user_data);
    int (*insert_at)(uint8_t slot, const vbrk_slot_record_t *record, void *user_data);
    int (*remove_at)(uint8_t slot, void *user_data);
    int (*move_block)(uint8_t from, uint8_t to, uint8_t len, void *user_data);
    int (*set_qty)(uint8_t slot, uint16_t qty, void *user_data);
    int (*factory_reset)(uint32_t magic, void *user_data);
    int (*notify_binding)(uint8_t op, uint8_t status, const void *payload,
                          uint16_t len, void *user_data);
    void (*table_changed)(void *user_data);
    void *user_data;
} vbrk_ble_dispatcher_t;

uint8_t vbrk_ble_status_from_errno(int err);
int vbrk_ble_dispatch_binding_cp(vbrk_ble_dispatcher_t *dispatcher,
                                 const uint8_t *frame, uint16_t len);

#endif
