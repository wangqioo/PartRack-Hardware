#ifndef DEVICE_HEALTH_H
#define DEVICE_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_device_health.h"

int device_health_init(void);
uint8_t device_health_battery_pct(void);
uint16_t device_health_reset_reason(void);
bool device_health_watchdog_enabled(void);
bool device_health_fault(void);
void device_health_feed_watchdog(void);
void device_health_get(vbrk_device_health_t *health);

#endif
