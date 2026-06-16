#ifndef VIBERACK_DEVICE_HEALTH_H
#define VIBERACK_DEVICE_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#define VBRK_DEVICE_HEALTH_SIZE 4

typedef enum {
    VBRK_RESET_REASON_UNKNOWN = 0,
    VBRK_RESET_REASON_POWER_ON = 1 << 0,
    VBRK_RESET_REASON_PIN = 1 << 1,
    VBRK_RESET_REASON_SOFTWARE = 1 << 2,
    VBRK_RESET_REASON_WATCHDOG = 1 << 3,
    VBRK_RESET_REASON_LOCKUP = 1 << 4,
    VBRK_RESET_REASON_OFF = 1 << 5,
    VBRK_RESET_REASON_OTHER = 1 << 15,
} vbrk_reset_reason_t;

typedef enum {
    VBRK_DEVICE_HEALTH_WATCHDOG_ENABLED = 1 << 0,
    VBRK_DEVICE_HEALTH_FAULT = 1 << 1,
} vbrk_device_health_flags_t;

typedef struct {
    uint8_t battery_pct;
    uint16_t reset_reason;
    bool watchdog_enabled;
    bool fault;
} vbrk_device_health_t;

uint8_t vbrk_battery_percent_from_mv(uint16_t voltage_mv,
                                     uint16_t empty_mv,
                                     uint16_t full_mv);
void vbrk_device_health_encode(uint8_t out[VBRK_DEVICE_HEALTH_SIZE],
                               const vbrk_device_health_t *health);

#endif
