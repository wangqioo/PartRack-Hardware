#ifndef VIBERACK_ADV_PAYLOAD_H
#define VIBERACK_ADV_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    uint16_t company_id;
    uint16_t batch_id;
    uint8_t battery_pct;
    uint32_t table_seq;
    bool has_unbound_slot;
    bool light_active;
    bool fault;
} vbrk_adv_payload_input_t;

void vbrk_adv_payload_build(uint8_t out[VBRK_ADV_MSD_SIZE],
                            const vbrk_adv_payload_input_t *input);

#endif
