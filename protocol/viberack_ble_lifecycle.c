#include "viberack_ble_lifecycle.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

static unsigned int lock_lifecycle(vbrk_ble_lifecycle_t *lifecycle)
{
    if (lifecycle->adapter.lock == NULL) {
        return 0;
    }

    return lifecycle->adapter.lock(lifecycle->adapter.user_data);
}

static void unlock_lifecycle(vbrk_ble_lifecycle_t *lifecycle, unsigned int key)
{
    if (lifecycle->adapter.unlock != NULL) {
        lifecycle->adapter.unlock(key, lifecycle->adapter.user_data);
    }
}

static void mark_dirty(vbrk_ble_lifecycle_t *lifecycle)
{
    lifecycle->advertising_generation++;
}

static bool schedule_locked(vbrk_ble_lifecycle_t *lifecycle)
{
    if (!lifecycle->ready || lifecycle->work_pending) {
        return false;
    }

    lifecycle->work_pending = true;
    return true;
}

static void schedule_if_needed(vbrk_ble_lifecycle_t *lifecycle)
{
    if (lifecycle->adapter.schedule_work != NULL) {
        lifecycle->adapter.schedule_work(lifecycle->adapter.user_data);
    }
}

void vbrk_ble_lifecycle_init(vbrk_ble_lifecycle_t *lifecycle,
                             const vbrk_ble_lifecycle_adapter_t *adapter)
{
    if (lifecycle == NULL) {
        return;
    }

    memset(lifecycle, 0, sizeof(*lifecycle));
    if (adapter != NULL) {
        lifecycle->adapter = *adapter;
    }
}

void vbrk_ble_lifecycle_bluetooth_initialized(vbrk_ble_lifecycle_t *lifecycle)
{
    unsigned int key;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    lifecycle->bluetooth_initialized = true;
    unlock_lifecycle(lifecycle, key);
}

int vbrk_ble_lifecycle_start(vbrk_ble_lifecycle_t *lifecycle)
{
    uint32_t target_generation;
    unsigned int key;
    int err;

    if (lifecycle == NULL || lifecycle->adapter.apply_advertising == NULL) {
        return -EINVAL;
    }

    key = lock_lifecycle(lifecycle);
    if (lifecycle->advertising_generation == lifecycle->applied_generation) {
        mark_dirty(lifecycle);
    }
    target_generation = lifecycle->advertising_generation;
    unlock_lifecycle(lifecycle, key);

    err = lifecycle->adapter.apply_advertising(true, lifecycle->adapter.user_data);

    key = lock_lifecycle(lifecycle);
    if (err == 0) {
        lifecycle->ready = true;
        lifecycle->advertising_running = true;
        lifecycle->applied_generation = target_generation;
    }
    unlock_lifecycle(lifecycle, key);

    return err;
}

void vbrk_ble_lifecycle_connected(vbrk_ble_lifecycle_t *lifecycle)
{
    unsigned int key;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    lifecycle->connected = true;
    lifecycle->advertising_running = false;
    unlock_lifecycle(lifecycle, key);
}

void vbrk_ble_lifecycle_disconnected(vbrk_ble_lifecycle_t *lifecycle)
{
    unsigned int key;
    bool schedule;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    lifecycle->connected = false;
    lifecycle->advertising_running = false;
    mark_dirty(lifecycle);
    schedule = schedule_locked(lifecycle);
    unlock_lifecycle(lifecycle, key);

    if (schedule) {
        schedule_if_needed(lifecycle);
    }
}

void vbrk_ble_lifecycle_report_nfc_fd(vbrk_ble_lifecycle_t *lifecycle)
{
    unsigned int key;
    bool schedule;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    mark_dirty(lifecycle);
    schedule = schedule_locked(lifecycle);
    unlock_lifecycle(lifecycle, key);

    if (schedule) {
        schedule_if_needed(lifecycle);
    }
}

void vbrk_ble_lifecycle_report_binding_changed(vbrk_ble_lifecycle_t *lifecycle)
{
    int (*notify_table_info)(void *user_data);
    void *user_data;
    unsigned int key;
    bool connected;
    bool schedule;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    mark_dirty(lifecycle);
    connected = lifecycle->connected;
    notify_table_info = lifecycle->adapter.notify_table_info;
    user_data = lifecycle->adapter.user_data;
    schedule = !connected && schedule_locked(lifecycle);
    unlock_lifecycle(lifecycle, key);

    if (connected && notify_table_info != NULL) {
        (void)notify_table_info(user_data);
    }
    if (schedule) {
        schedule_if_needed(lifecycle);
    }
}

void vbrk_ble_lifecycle_report_light_changed(vbrk_ble_lifecycle_t *lifecycle,
                                             uint8_t mode,
                                             uint16_t remaining_s)
{
    int (*notify_light_status)(uint8_t mode, uint16_t remaining_s,
                               void *user_data);
    void *user_data;
    unsigned int key;
    bool connected;
    bool schedule;

    if (lifecycle == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    mark_dirty(lifecycle);
    connected = lifecycle->connected;
    notify_light_status = lifecycle->adapter.notify_light_status;
    user_data = lifecycle->adapter.user_data;
    schedule = !connected && schedule_locked(lifecycle);
    unlock_lifecycle(lifecycle, key);

    if (connected && notify_light_status != NULL) {
        (void)notify_light_status(mode, remaining_s, user_data);
    }
    if (schedule) {
        schedule_if_needed(lifecycle);
    }
}

int vbrk_ble_lifecycle_notify_binding(vbrk_ble_lifecycle_t *lifecycle,
                                      uint8_t op, uint8_t status,
                                      const void *payload, uint16_t len)
{
    int (*notify_binding)(uint8_t op, uint8_t status, const void *payload,
                          uint16_t len, void *user_data);
    void *user_data;
    unsigned int key;

    if (lifecycle == NULL) {
        return -EINVAL;
    }

    key = lock_lifecycle(lifecycle);
    if (!lifecycle->connected) {
        unlock_lifecycle(lifecycle, key);
        return -ENOTCONN;
    }
    notify_binding = lifecycle->adapter.notify_binding;
    user_data = lifecycle->adapter.user_data;
    unlock_lifecycle(lifecycle, key);

    if (notify_binding == NULL) {
        return -ENOTCONN;
    }

    return notify_binding(op, status, payload, len, user_data);
}

void vbrk_ble_lifecycle_process(vbrk_ble_lifecycle_t *lifecycle)
{
    uint32_t target_generation;
    unsigned int key;
    bool restart;
    bool apply;
    int err;

    if (lifecycle == NULL || lifecycle->adapter.apply_advertising == NULL) {
        return;
    }

    key = lock_lifecycle(lifecycle);
    lifecycle->work_pending = false;
    apply = lifecycle->ready && !lifecycle->connected &&
            lifecycle->advertising_generation != lifecycle->applied_generation;
    restart = !lifecycle->advertising_running;
    target_generation = lifecycle->advertising_generation;
    unlock_lifecycle(lifecycle, key);

    if (!apply) {
        return;
    }

    err = lifecycle->adapter.apply_advertising(restart,
                                               lifecycle->adapter.user_data);

    key = lock_lifecycle(lifecycle);
    if (err == 0) {
        lifecycle->advertising_running = true;
        lifecycle->applied_generation = target_generation;
        if (lifecycle->ready && !lifecycle->connected &&
            lifecycle->advertising_generation != lifecycle->applied_generation &&
            schedule_locked(lifecycle)) {
            unlock_lifecycle(lifecycle, key);
            schedule_if_needed(lifecycle);
            return;
        }
    }
    unlock_lifecycle(lifecycle, key);
}

bool vbrk_ble_lifecycle_is_advertising_dirty(
    const vbrk_ble_lifecycle_t *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    return lifecycle->advertising_generation != lifecycle->applied_generation;
}
