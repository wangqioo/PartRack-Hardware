#ifndef VIBERACK_READ_ALL_PACER_H
#define VIBERACK_READ_ALL_PACER_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

#define VBRK_READ_ALL_NOTIFY_DELAY_MS 10
#define VBRK_READ_ALL_RETRY_DELAY_MS 30

typedef struct {
    int (*read_one)(uint8_t slot, vbrk_slot_record_t *record, void *user_data);
    int (*notify_binding)(uint8_t op, uint8_t status, const void *payload,
                          uint16_t len, void *user_data);
    void (*schedule_next)(uint16_t delay_ms, void *user_data);
    void *user_data;
} vbrk_read_all_pacer_adapter_t;

typedef struct {
    vbrk_read_all_pacer_adapter_t adapter;
    uint8_t next_slot;
    bool active;
} vbrk_read_all_pacer_t;

void vbrk_read_all_pacer_init(vbrk_read_all_pacer_t *pacer,
                              const vbrk_read_all_pacer_adapter_t *adapter);
void vbrk_read_all_pacer_start(vbrk_read_all_pacer_t *pacer);
void vbrk_read_all_pacer_cancel(vbrk_read_all_pacer_t *pacer);
void vbrk_read_all_pacer_process(vbrk_read_all_pacer_t *pacer);
bool vbrk_read_all_pacer_is_active(const vbrk_read_all_pacer_t *pacer);

#endif
