#ifndef APP_BLE_H
#define APP_BLE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

int app_ble_start(void);
void app_ble_refresh_advertising(void);
int app_ble_notify_binding_result(uint8_t op, uint8_t status, const void *payload, uint16_t len);
int app_ble_notify_table_info(void);
int app_ble_notify_light_status(uint8_t mode, uint16_t remaining_s);
void app_ble_set_light_active(bool active);

#endif
