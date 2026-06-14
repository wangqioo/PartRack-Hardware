#ifndef VIBERACK_BLE_LIFECYCLE_H
#define VIBERACK_BLE_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    unsigned int (*lock)(void *user_data);
    void (*unlock)(unsigned int key, void *user_data);
    void (*schedule_work)(void *user_data);
    int (*apply_advertising)(bool restart, void *user_data);
    int (*notify_binding)(uint8_t op, uint8_t status,
                          const void *payload, uint16_t len,
                          void *user_data);
    int (*notify_table_info)(void *user_data);
    int (*notify_light_status)(uint8_t mode, uint16_t remaining_s,
                               void *user_data);
    void *user_data;
} vbrk_ble_lifecycle_adapter_t;

typedef struct {
    vbrk_ble_lifecycle_adapter_t adapter;
    bool bluetooth_initialized;
    bool ready;
    bool connected;
    bool advertising_running;
    bool work_pending;
    uint32_t advertising_generation;
    uint32_t applied_generation;
} vbrk_ble_lifecycle_t;

void vbrk_ble_lifecycle_init(vbrk_ble_lifecycle_t *lifecycle,
                             const vbrk_ble_lifecycle_adapter_t *adapter);
void vbrk_ble_lifecycle_bluetooth_initialized(vbrk_ble_lifecycle_t *lifecycle);
int vbrk_ble_lifecycle_start(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_connected(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_disconnected(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_nfc_fd(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_binding_changed(vbrk_ble_lifecycle_t *lifecycle);
void vbrk_ble_lifecycle_report_light_changed(vbrk_ble_lifecycle_t *lifecycle,
                                             uint8_t mode,
                                             uint16_t remaining_s);
int vbrk_ble_lifecycle_notify_binding(vbrk_ble_lifecycle_t *lifecycle,
                                      uint8_t op, uint8_t status,
                                      const void *payload, uint16_t len);
void vbrk_ble_lifecycle_process(vbrk_ble_lifecycle_t *lifecycle);
bool vbrk_ble_lifecycle_is_advertising_dirty(
    const vbrk_ble_lifecycle_t *lifecycle);

#endif
