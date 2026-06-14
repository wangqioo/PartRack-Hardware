#ifndef APP_BLE_H
#define APP_BLE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

int app_ble_init(void);
int app_ble_start(void);
int app_ble_notify_binding_result(uint8_t op, uint8_t status, const void *payload, uint16_t len);
void app_ble_report_binding_changed(void);
void app_ble_report_light_changed(void);
void app_ble_report_nfc_fd(void);

#endif
