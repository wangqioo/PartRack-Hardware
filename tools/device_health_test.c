#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "viberack_device_health.h"

static void test_battery_percent_clamps_to_valid_range(void)
{
    assert(vbrk_battery_percent_from_mv(3000, 3000, 4200) == 0);
    assert(vbrk_battery_percent_from_mv(3600, 3000, 4200) == 50);
    assert(vbrk_battery_percent_from_mv(4200, 3000, 4200) == 100);
    assert(vbrk_battery_percent_from_mv(4300, 3000, 4200) == 100);
    assert(vbrk_battery_percent_from_mv(2900, 3000, 4200) == 0);
}

static void test_invalid_battery_calibration_returns_zero(void)
{
    assert(vbrk_battery_percent_from_mv(3700, 4200, 3000) == 0);
    assert(vbrk_battery_percent_from_mv(3700, 3600, 3600) == 0);
}

static void test_reset_reason_payload_is_little_endian(void)
{
    uint8_t payload[VBRK_DEVICE_HEALTH_SIZE];
    vbrk_device_health_t health = {
        .battery_pct = 88,
        .reset_reason = VBRK_RESET_REASON_POWER_ON | VBRK_RESET_REASON_WATCHDOG,
        .watchdog_enabled = true,
        .fault = false,
    };

    vbrk_device_health_encode(payload, &health);

    assert(payload[0] == 88);
    assert(payload[1] == (VBRK_RESET_REASON_POWER_ON | VBRK_RESET_REASON_WATCHDOG));
    assert(payload[2] == 0);
    assert(payload[3] == VBRK_DEVICE_HEALTH_WATCHDOG_ENABLED);
}

static void test_health_payload_clamps_battery_and_fault_flag(void)
{
    uint8_t payload[VBRK_DEVICE_HEALTH_SIZE];
    vbrk_device_health_t health = {
        .battery_pct = 255,
        .reset_reason = VBRK_RESET_REASON_SOFTWARE,
        .watchdog_enabled = false,
        .fault = true,
    };

    vbrk_device_health_encode(payload, &health);

    assert(payload[0] == 100);
    assert(payload[1] == VBRK_RESET_REASON_SOFTWARE);
    assert(payload[2] == 0);
    assert(payload[3] == VBRK_DEVICE_HEALTH_FAULT);
}

static void test_null_inputs_are_safe(void)
{
    uint8_t payload[VBRK_DEVICE_HEALTH_SIZE] = {0xAA, 0xAA, 0xAA, 0xAA};
    vbrk_device_health_t health = {
        .battery_pct = 12,
        .reset_reason = VBRK_RESET_REASON_PIN,
    };

    vbrk_device_health_encode(NULL, &health);
    vbrk_device_health_encode(payload, NULL);

    assert(payload[0] == 0);
    assert(payload[1] == 0);
    assert(payload[2] == 0);
    assert(payload[3] == 0);
}

int main(void)
{
    test_battery_percent_clamps_to_valid_range();
    test_invalid_battery_calibration_returns_zero();
    test_reset_reason_payload_is_little_endian();
    test_health_payload_clamps_battery_and_fault_flag();
    test_null_inputs_are_safe();
    puts("device health checks passed");
    return 0;
}
