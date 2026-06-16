#include "viberack_device_health.h"

#include <stddef.h>
#include <string.h>

uint8_t vbrk_battery_percent_from_mv(uint16_t voltage_mv,
                                     uint16_t empty_mv,
                                     uint16_t full_mv)
{
    uint32_t numerator;
    uint32_t denominator;

    if (full_mv <= empty_mv) {
        return 0;
    }
    if (voltage_mv <= empty_mv) {
        return 0;
    }
    if (voltage_mv >= full_mv) {
        return 100;
    }

    numerator = (uint32_t)(voltage_mv - empty_mv) * 100u;
    denominator = (uint32_t)(full_mv - empty_mv);
    return (uint8_t)((numerator + denominator / 2u) / denominator);
}

void vbrk_device_health_encode(uint8_t out[VBRK_DEVICE_HEALTH_SIZE],
                               const vbrk_device_health_t *health)
{
    uint8_t flags = 0;
    uint8_t battery_pct = 0;
    uint16_t reset_reason = VBRK_RESET_REASON_UNKNOWN;

    if (out == NULL) {
        return;
    }

    if (health != NULL) {
        battery_pct = health->battery_pct > 100 ? 100 : health->battery_pct;
        reset_reason = health->reset_reason;
        if (health->watchdog_enabled) {
            flags |= VBRK_DEVICE_HEALTH_WATCHDOG_ENABLED;
        }
        if (health->fault) {
            flags |= VBRK_DEVICE_HEALTH_FAULT;
        }
    }

    memset(out, 0, VBRK_DEVICE_HEALTH_SIZE);
    out[0] = battery_pct;
    out[1] = (uint8_t)(reset_reason & 0xFFu);
    out[2] = (uint8_t)(reset_reason >> 8);
    out[3] = flags;
}
