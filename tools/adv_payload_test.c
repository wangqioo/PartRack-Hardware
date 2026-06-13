#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "viberack_adv_payload.h"

static void test_default_payload(void)
{
    uint8_t payload[VBRK_ADV_MSD_SIZE];

    vbrk_adv_payload_build(payload, NULL);

    assert(payload[0] == 0xFF);
    assert(payload[1] == 0xFF);
    assert(payload[2] == VBRK_PROTO_VER);
    assert(payload[3] == 0x01);
    assert(payload[4] == 0x00);
    assert(payload[5] == 100);
    assert(payload[6] == 0);
    assert(payload[7] == 0);
    assert(payload[8] == 0);
    assert(payload[9] == 0);
    assert(payload[10] == 0);
}

static void test_flags_and_table_seq_are_encoded(void)
{
    uint8_t payload[VBRK_ADV_MSD_SIZE];
    vbrk_adv_payload_input_t input = {
        .company_id = VBRK_DEV_COMPANY_ID,
        .batch_id = 0x1234,
        .battery_pct = 15,
        .table_seq = 0x89ABCDEFu,
        .has_unbound_slot = true,
        .light_active = true,
        .fault = true,
    };

    vbrk_adv_payload_build(payload, &input);

    assert(payload[3] == 0x34);
    assert(payload[4] == 0x12);
    assert(payload[5] == 15);
    assert(payload[6] == (VBRK_ADV_LOW_BATTERY | VBRK_ADV_HAS_UNBOUND_SLOT |
                          VBRK_ADV_LIGHT_ACTIVE | VBRK_ADV_FAULT));
    assert(payload[7] == 0xEF);
    assert(payload[8] == 0xCD);
    assert(payload[9] == 0);
    assert(payload[10] == 0);
}

static void test_battery_is_clamped(void)
{
    uint8_t payload[VBRK_ADV_MSD_SIZE];
    vbrk_adv_payload_input_t input = {
        .company_id = VBRK_DEV_COMPANY_ID,
        .batch_id = 1,
        .battery_pct = 101,
    };

    vbrk_adv_payload_build(payload, &input);

    assert(payload[5] == 100);
    assert((payload[6] & VBRK_ADV_LOW_BATTERY) == 0);
}

int main(void)
{
    test_default_payload();
    test_flags_and_table_seq_are_encoded();
    test_battery_is_clamped();
    return 0;
}
