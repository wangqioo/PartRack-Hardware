#include "app_ble.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include "binding_table.h"
#include "light_control.h"

LOG_MODULE_REGISTER(app_ble, LOG_LEVEL_INF);

#define VBRK_BT_UUID_BASE BT_UUID_128_ENCODE(0x7f4b0000, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_BINDING_SERVICE BT_UUID_128_ENCODE(0x7f4b0001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_BINDING_CP BT_UUID_128_ENCODE(0x7f4b1001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_TABLE_INFO BT_UUID_128_ENCODE(0x7f4b1002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_SERVICE BT_UUID_128_ENCODE(0x7f4b0002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_COMMAND BT_UUID_128_ENCODE(0x7f4b2001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_STATUS BT_UUID_128_ENCODE(0x7f4b2002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)

static struct bt_conn *current_conn;
static bool light_active;
static uint8_t battery_pct = 100;

static struct bt_uuid_128 binding_service_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_BINDING_SERVICE);
static struct bt_uuid_128 binding_cp_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_BINDING_CP);
static struct bt_uuid_128 table_info_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_TABLE_INFO);
static struct bt_uuid_128 light_service_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_SERVICE);
static struct bt_uuid_128 light_command_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_COMMAND);
static struct bt_uuid_128 light_status_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_STATUS);

static uint8_t adv_msd[11];

static void fill_adv_msd(void)
{
    uint8_t flags = 0;

    if (battery_pct <= 15) {
        flags |= VBRK_ADV_LOW_BATTERY;
    }
    if (binding_table_has_unbound_slot()) {
        flags |= VBRK_ADV_HAS_UNBOUND_SLOT;
    }
    if (light_active) {
        flags |= VBRK_ADV_LIGHT_ACTIVE;
    }

    sys_put_le16(VBRK_DEV_COMPANY_ID, &adv_msd[0]);
    adv_msd[2] = VBRK_PROTO_VER;
    sys_put_le16(1, &adv_msd[3]); /* TODO: assign real batch_id at factory bind. */
    adv_msd[5] = battery_pct;
    adv_msd[6] = flags;
    sys_put_le16((uint16_t)binding_table_seq(), &adv_msd[7]);
    adv_msd[9] = 0;
    adv_msd[10] = 0;
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_msd, sizeof(adv_msd)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static uint8_t status_from_errno(int err)
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

static ssize_t table_info_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    vbrk_table_info_t info;

    ARG_UNUSED(attr);
    binding_table_get_info(&info);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &info, sizeof(info));
}

static ssize_t light_status_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    uint8_t status[3];

    ARG_UNUSED(attr);
    status[0] = light_control_mode();
    sys_put_le16(light_control_remaining_s(), &status[1]);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, status, sizeof(status));
}

static ssize_t binding_cp_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags)
{
    const uint8_t *frame = buf;
    uint8_t op;
    int err = 0;

    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (offset != 0 || len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    op = frame[0];

    switch (op) {
    case VBRK_OP_READ_ONE: {
        vbrk_slot_record_t record;

        if (len != 2) {
            err = -EINVAL;
            break;
        }

        err = binding_table_read_one(frame[1], &record);
        app_ble_notify_binding_result(op, status_from_errno(err), &record, err == 0 ? sizeof(record) : 0);
        return len;
    }
    case VBRK_OP_READ_ALL: {
        vbrk_slot_record_t record;
        uint8_t end_marker = VBRK_READ_ALL_END_MARKER;

        if (len != 1) {
            err = -EINVAL;
            break;
        }

        for (uint8_t slot = 1; slot <= VBRK_SLOT_COUNT; slot++) {
            err = binding_table_read_one(slot, &record);
            app_ble_notify_binding_result(op, status_from_errno(err), &record, err == 0 ? sizeof(record) : 0);
            if (err != 0) {
                return len;
            }
        }
        app_ble_notify_binding_result(op, VBRK_STATUS_OK, &end_marker, sizeof(end_marker));
        return len;
    }
    case VBRK_OP_WRITE_ONE:
        if (len != 1 + VBRK_SLOT_RECORD_SIZE) {
            err = -EINVAL;
            break;
        }
        err = binding_table_write_one((const vbrk_slot_record_t *)&frame[1]);
        break;
    case VBRK_OP_CLEAR_ONE:
        if (len != 2) {
            err = -EINVAL;
            break;
        }
        err = binding_table_clear_one(frame[1]);
        break;
    case VBRK_OP_INSERT_AT:
        if (len != 2 + VBRK_SLOT_RECORD_SIZE) {
            err = -EINVAL;
            break;
        }
        err = binding_table_insert_at(frame[1], (const vbrk_slot_record_t *)&frame[2]);
        break;
    case VBRK_OP_REMOVE_AT:
        if (len != 2) {
            err = -EINVAL;
            break;
        }
        err = binding_table_remove_at(frame[1]);
        break;
    case VBRK_OP_MOVE_BLOCK:
        if (len != 4) {
            err = -EINVAL;
            break;
        }
        err = binding_table_move_block(frame[1], frame[2], frame[3]);
        break;
    case VBRK_OP_SET_QTY:
        if (len != 4) {
            err = -EINVAL;
            break;
        }
        err = binding_table_set_qty(frame[1], sys_get_le16(&frame[2]));
        break;
    case VBRK_OP_FACTORY_RESET:
        if (len != 5) {
            err = -EINVAL;
            break;
        }
        err = binding_table_factory_reset(sys_get_le32(&frame[1]));
        break;
    default:
        err = -EINVAL;
        break;
    }

    app_ble_notify_binding_result(op, status_from_errno(err), NULL, 0);
    if (err == 0) {
        app_ble_notify_table_info();
        app_ble_refresh_advertising();
    }

    return len;
}

static ssize_t light_command_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset,
                                   uint8_t flags)
{
    int err;

    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (offset != 0 || len != VBRK_LIGHT_COMMAND_SIZE) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    err = light_control_apply((const vbrk_light_command_t *)buf);
    if (err != 0) {
        LOG_WRN("light command rejected: %d", err);
    }

    return len;
}

BT_GATT_SERVICE_DEFINE(binding_svc,
    BT_GATT_PRIMARY_SERVICE(&binding_service_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&binding_cp_uuid.uuid,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_WRITE_ENCRYPT,
                           NULL, binding_cp_write, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&table_info_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           table_info_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

BT_GATT_SERVICE_DEFINE(light_svc,
    BT_GATT_PRIMARY_SERVICE(&light_service_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&light_command_uuid.uuid,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           NULL, light_command_write, NULL),
    BT_GATT_CHARACTERISTIC(&light_status_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           light_status_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0) {
        LOG_WRN("connection failed: %u", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_INF("disconnected: %u", reason);
    if (current_conn != NULL) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int app_ble_notify_binding_result(uint8_t op, uint8_t status, const void *payload, uint16_t len)
{
    uint8_t frame[2 + VBRK_SLOT_RECORD_SIZE];

    if (current_conn == NULL || len > VBRK_SLOT_RECORD_SIZE) {
        return -ENOTCONN;
    }

    frame[0] = op;
    frame[1] = status;
    if (payload != NULL && len > 0) {
        memcpy(&frame[2], payload, len);
    }

    return bt_gatt_notify(current_conn, &binding_svc.attrs[2], frame, 2 + len);
}

int app_ble_notify_table_info(void)
{
    vbrk_table_info_t info;

    if (current_conn == NULL) {
        return -ENOTCONN;
    }

    binding_table_get_info(&info);
    return bt_gatt_notify(current_conn, &binding_svc.attrs[5], &info, sizeof(info));
}

int app_ble_notify_light_status(uint8_t mode, uint16_t remaining_s)
{
    uint8_t status[3];

    if (current_conn == NULL) {
        return -ENOTCONN;
    }

    status[0] = mode;
    sys_put_le16(remaining_s, &status[1]);
    return bt_gatt_notify(current_conn, &light_svc.attrs[4], status, sizeof(status));
}

void app_ble_set_light_active(bool active)
{
    light_active = active;
}

void app_ble_refresh_advertising(void)
{
    int err;

    fill_adv_msd();
    err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err != 0) {
        LOG_DBG("adv update skipped: %d", err);
    }
}

int app_ble_start(void)
{
    int err;

    err = bt_enable(NULL);
    if (err != 0) {
        return err;
    }

    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        err = settings_load();
        if (err != 0) {
            return err;
        }
    }

    fill_adv_msd();
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err != 0) {
        return err;
    }

    LOG_INF("advertising started");
    return 0;
}
