#include "viberack_read_all_pacer.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "viberack_ble_dispatcher_model.h"

static void schedule_next(vbrk_read_all_pacer_t *pacer, uint16_t delay_ms)
{
    if (pacer->adapter.schedule_next != NULL) {
        pacer->adapter.schedule_next(delay_ms, pacer->adapter.user_data);
    }
}

static int notify_read_all(vbrk_read_all_pacer_t *pacer, uint8_t status,
                           const void *payload, uint16_t len)
{
    if (pacer->adapter.notify_binding == NULL) {
        return -ENOTCONN;
    }

    return pacer->adapter.notify_binding(VBRK_OP_READ_ALL, status, payload,
                                         len, pacer->adapter.user_data);
}

void vbrk_read_all_pacer_init(vbrk_read_all_pacer_t *pacer,
                              const vbrk_read_all_pacer_adapter_t *adapter)
{
    if (pacer == NULL) {
        return;
    }

    memset(pacer, 0, sizeof(*pacer));
    if (adapter != NULL) {
        pacer->adapter = *adapter;
    }
}

void vbrk_read_all_pacer_start(vbrk_read_all_pacer_t *pacer)
{
    if (pacer == NULL) {
        return;
    }

    pacer->active = true;
    pacer->next_slot = 1;
    schedule_next(pacer, 0);
}

void vbrk_read_all_pacer_cancel(vbrk_read_all_pacer_t *pacer)
{
    if (pacer != NULL) {
        pacer->active = false;
    }
}

bool vbrk_read_all_pacer_is_active(const vbrk_read_all_pacer_t *pacer)
{
    return pacer != NULL && pacer->active;
}

void vbrk_read_all_pacer_process(vbrk_read_all_pacer_t *pacer)
{
    vbrk_slot_record_t record;
    uint8_t end_marker = VBRK_READ_ALL_END_MARKER;
    uint8_t status;
    int err;

    if (pacer == NULL || !pacer->active) {
        return;
    }

    if (pacer->next_slot > VBRK_SLOT_COUNT) {
        err = notify_read_all(pacer, VBRK_STATUS_OK, &end_marker,
                              sizeof(end_marker));
        if (err == 0) {
            pacer->active = false;
            return;
        }
        schedule_next(pacer, VBRK_READ_ALL_RETRY_DELAY_MS);
        return;
    }

    if (pacer->adapter.read_one == NULL) {
        err = -EINVAL;
    } else {
        err = pacer->adapter.read_one(pacer->next_slot, &record,
                                      pacer->adapter.user_data);
    }

    status = vbrk_ble_status_from_errno(err);
    if (err != 0) {
        (void)notify_read_all(pacer, status, NULL, 0);
        pacer->active = false;
        return;
    }

    err = notify_read_all(pacer, status, &record, sizeof(record));
    if (err != 0) {
        schedule_next(pacer, VBRK_READ_ALL_RETRY_DELAY_MS);
        return;
    }

    pacer->next_slot++;
    schedule_next(pacer, VBRK_READ_ALL_NOTIFY_DELAY_MS);
}
