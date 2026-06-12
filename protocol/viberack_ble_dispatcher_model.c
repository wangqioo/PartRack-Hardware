#include "viberack_ble_dispatcher_model.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

static uint16_t get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

uint8_t vbrk_ble_status_from_errno(int err)
{
    if (err == 0) {
        return VBRK_STATUS_OK;
    }
    if (err == -ENOSPC) {
        return VBRK_STATUS_ERR_FULL;
    }
    if (err == -EBUSY) {
        return VBRK_STATUS_ERR_FLASH_BUSY;
    }
    if (err == -EILSEQ) {
        return VBRK_STATUS_ERR_CRC;
    }

    return VBRK_STATUS_ERR_PARAM;
}

static void notify_binding(vbrk_ble_dispatcher_t *dispatcher, uint8_t op,
                           int err, const void *payload, uint16_t len)
{
    if (dispatcher == NULL || dispatcher->notify_binding == NULL) {
        return;
    }

    (void)dispatcher->notify_binding(op, vbrk_ble_status_from_errno(err),
                                     err == 0 ? payload : NULL,
                                     err == 0 ? len : 0,
                                     dispatcher->user_data);
}

static void notify_table_changed(vbrk_ble_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL || dispatcher->table_changed == NULL) {
        return;
    }

    dispatcher->table_changed(dispatcher->user_data);
}

static int validate_dispatcher(const vbrk_ble_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL || dispatcher->read_one == NULL ||
        dispatcher->write_one == NULL || dispatcher->clear_one == NULL ||
        dispatcher->insert_at == NULL || dispatcher->remove_at == NULL ||
        dispatcher->move_block == NULL || dispatcher->set_qty == NULL ||
        dispatcher->factory_reset == NULL || dispatcher->notify_binding == NULL ||
        dispatcher->table_changed == NULL) {
        return -EINVAL;
    }

    return 0;
}

static int dispatch_read_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                             uint16_t len)
{
    vbrk_slot_record_t record;
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_READ_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->read_one(frame[1], &record, dispatcher->user_data);
    notify_binding(dispatcher, VBRK_OP_READ_ONE, err, &record, sizeof(record));
    return 0;
}

static int dispatch_read_all(vbrk_ble_dispatcher_t *dispatcher, uint16_t len)
{
    vbrk_slot_record_t record;
    uint8_t end_marker = VBRK_READ_ALL_END_MARKER;
    int err;

    if (len != 1) {
        notify_binding(dispatcher, VBRK_OP_READ_ALL, -EINVAL, NULL, 0);
        return 0;
    }

    for (uint8_t slot = 1; slot <= VBRK_SLOT_COUNT; slot++) {
        err = dispatcher->read_one(slot, &record, dispatcher->user_data);
        notify_binding(dispatcher, VBRK_OP_READ_ALL, err, &record, sizeof(record));
        if (err != 0) {
            return 0;
        }
    }

    notify_binding(dispatcher, VBRK_OP_READ_ALL, 0, &end_marker, sizeof(end_marker));
    return 0;
}

static int dispatch_mutating_status(vbrk_ble_dispatcher_t *dispatcher,
                                    uint8_t op, int err)
{
    notify_binding(dispatcher, op, err, NULL, 0);

    if (err == 0) {
        notify_table_changed(dispatcher);
    }

    return 0;
}

static int dispatch_write_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    vbrk_slot_record_t record;
    int err;

    if (len != 1 + VBRK_SLOT_RECORD_SIZE) {
        notify_binding(dispatcher, VBRK_OP_WRITE_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    memcpy(&record, &frame[1], sizeof(record));
    err = dispatcher->write_one(&record, dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_WRITE_ONE, err);
}

static int dispatch_clear_one(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_CLEAR_ONE, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->clear_one(frame[1], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_CLEAR_ONE, err);
}

static int dispatch_insert_at(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    vbrk_slot_record_t record;
    int err;

    if (len != 2 + VBRK_SLOT_RECORD_SIZE) {
        notify_binding(dispatcher, VBRK_OP_INSERT_AT, -EINVAL, NULL, 0);
        return 0;
    }

    memcpy(&record, &frame[2], sizeof(record));
    err = dispatcher->insert_at(frame[1], &record, dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_INSERT_AT, err);
}

static int dispatch_remove_at(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                              uint16_t len)
{
    int err;

    if (len != 2) {
        notify_binding(dispatcher, VBRK_OP_REMOVE_AT, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->remove_at(frame[1], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_REMOVE_AT, err);
}

static int dispatch_move_block(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                               uint16_t len)
{
    int err;

    if (len != 4) {
        notify_binding(dispatcher, VBRK_OP_MOVE_BLOCK, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->move_block(frame[1], frame[2], frame[3], dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_MOVE_BLOCK, err);
}

static int dispatch_set_qty(vbrk_ble_dispatcher_t *dispatcher, const uint8_t *frame,
                            uint16_t len)
{
    int err;

    if (len != 4) {
        notify_binding(dispatcher, VBRK_OP_SET_QTY, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->set_qty(frame[1], get_le16(&frame[2]), dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_SET_QTY, err);
}

static int dispatch_factory_reset(vbrk_ble_dispatcher_t *dispatcher,
                                  const uint8_t *frame, uint16_t len)
{
    int err;

    if (len != 5) {
        notify_binding(dispatcher, VBRK_OP_FACTORY_RESET, -EINVAL, NULL, 0);
        return 0;
    }

    err = dispatcher->factory_reset(get_le32(&frame[1]), dispatcher->user_data);
    return dispatch_mutating_status(dispatcher, VBRK_OP_FACTORY_RESET, err);
}

int vbrk_ble_dispatch_binding_cp(vbrk_ble_dispatcher_t *dispatcher,
                                 const uint8_t *frame, uint16_t len)
{
    uint8_t op;

    if (validate_dispatcher(dispatcher) != 0 || frame == NULL || len < 1) {
        return -EINVAL;
    }

    op = frame[0];
    switch (op) {
    case VBRK_OP_READ_ONE:
        return dispatch_read_one(dispatcher, frame, len);
    case VBRK_OP_READ_ALL:
        return dispatch_read_all(dispatcher, len);
    case VBRK_OP_WRITE_ONE:
        return dispatch_write_one(dispatcher, frame, len);
    case VBRK_OP_CLEAR_ONE:
        return dispatch_clear_one(dispatcher, frame, len);
    case VBRK_OP_INSERT_AT:
        return dispatch_insert_at(dispatcher, frame, len);
    case VBRK_OP_REMOVE_AT:
        return dispatch_remove_at(dispatcher, frame, len);
    case VBRK_OP_MOVE_BLOCK:
        return dispatch_move_block(dispatcher, frame, len);
    case VBRK_OP_SET_QTY:
        return dispatch_set_qty(dispatcher, frame, len);
    case VBRK_OP_FACTORY_RESET:
        return dispatch_factory_reset(dispatcher, frame, len);
    default:
        notify_binding(dispatcher, op, -EINVAL, NULL, 0);
        return 0;
    }
}
