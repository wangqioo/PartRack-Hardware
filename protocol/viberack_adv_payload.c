#include "viberack_adv_payload.h"

#include <string.h>

static void put_le16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8);
}

void vbrk_adv_payload_build(uint8_t out[VBRK_ADV_MSD_SIZE],
                            const vbrk_adv_payload_input_t *input)
{
    uint8_t flags = 0;
    uint16_t company_id = VBRK_DEV_COMPANY_ID;
    uint16_t batch_id = 1;
    uint8_t battery_pct = 100;
    uint32_t table_seq = 0;

    if (out == NULL) {
        return;
    }

    memset(out, 0, VBRK_ADV_MSD_SIZE);
    if (input != NULL) {
        company_id = input->company_id;
        batch_id = input->batch_id;
        battery_pct = input->battery_pct > 100 ? 100 : input->battery_pct;
        table_seq = input->table_seq;
        if (input->has_unbound_slot) {
            flags |= VBRK_ADV_HAS_UNBOUND_SLOT;
        }
        if (input->light_active) {
            flags |= VBRK_ADV_LIGHT_ACTIVE;
        }
        if (input->fault) {
            flags |= VBRK_ADV_FAULT;
        }
    }

    if (battery_pct <= 15) {
        flags |= VBRK_ADV_LOW_BATTERY;
    }

    put_le16(&out[0], company_id);
    out[2] = VBRK_PROTO_VER;
    put_le16(&out[3], batch_id);
    out[5] = battery_pct;
    out[6] = flags;
    put_le16(&out[7], (uint16_t)table_seq);
}
