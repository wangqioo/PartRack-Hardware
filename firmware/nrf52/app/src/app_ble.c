#include "app_ble.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "binding_table.h"
#include "device_health.h"
#include "light_control.h"
#include "viberack_adv_payload.h"
#include "viberack_ble_dispatcher_model.h"
#include "viberack_ble_lifecycle.h"
#include "viberack_device_health.h"
#include "viberack_read_all_pacer.h"

LOG_MODULE_REGISTER(app_ble, LOG_LEVEL_INF);

void app_status_set_ble_connected(bool connected);

#define VBRK_BT_UUID_BASE BT_UUID_128_ENCODE(0x7f4b0000, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_BINDING_SERVICE BT_UUID_128_ENCODE(0x7f4b0001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_BINDING_CP BT_UUID_128_ENCODE(0x7f4b1001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_TABLE_INFO BT_UUID_128_ENCODE(0x7f4b1002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_SERVICE BT_UUID_128_ENCODE(0x7f4b0002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_COMMAND BT_UUID_128_ENCODE(0x7f4b2001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_LIGHT_STATUS BT_UUID_128_ENCODE(0x7f4b2002, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_HEALTH_SERVICE BT_UUID_128_ENCODE(0x7f4b0003, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)
#define VBRK_BT_UUID_DEVICE_HEALTH BT_UUID_128_ENCODE(0x7f4b3001, 0x8d1a, 0x4d45, 0x9a4e, 0x2b4a7c000000)

static struct bt_conn *current_conn;
static vbrk_ble_lifecycle_t ble_lifecycle;
static struct k_work ble_lifecycle_work;
static vbrk_read_all_pacer_t read_all_pacer;
static struct k_work_delayable read_all_work;

static struct bt_uuid_128 binding_service_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_BINDING_SERVICE);
static struct bt_uuid_128 binding_cp_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_BINDING_CP);
static struct bt_uuid_128 table_info_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_TABLE_INFO);
static struct bt_uuid_128 light_service_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_SERVICE);
static struct bt_uuid_128 light_command_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_COMMAND);
static struct bt_uuid_128 light_status_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_LIGHT_STATUS);
static struct bt_uuid_128 health_service_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_HEALTH_SERVICE);
static struct bt_uuid_128 device_health_uuid = BT_UUID_INIT_128(VBRK_BT_UUID_DEVICE_HEALTH);

static uint8_t adv_msd[VBRK_ADV_MSD_SIZE];

static void ble_lifecycle_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    vbrk_ble_lifecycle_process(&ble_lifecycle);
}

static void read_all_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    vbrk_read_all_pacer_process(&read_all_pacer);
}

static unsigned int lifecycle_lock(void *user_data)
{
    ARG_UNUSED(user_data);
    return irq_lock();
}

static void lifecycle_unlock(unsigned int key, void *user_data)
{
    ARG_UNUSED(user_data);
    irq_unlock(key);
}

static void lifecycle_schedule_work(void *user_data)
{
    ARG_UNUSED(user_data);
    (void)k_work_submit(&ble_lifecycle_work);
}

static void fill_adv_msd(void)
{
    vbrk_adv_payload_input_t input = {
        .company_id = VBRK_DEV_COMPANY_ID,
        .batch_id = 1,
        .battery_pct = device_health_battery_pct(),
        .table_seq = binding_table_seq(),
        .has_unbound_slot = binding_table_has_unbound_slot(),
        .light_active = light_control_mode() != VBRK_LIGHT_OFF,
        .fault = device_health_fault(),
    };

    vbrk_adv_payload_build(adv_msd, &input);
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_msd, sizeof(adv_msd)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static int lifecycle_apply_advertising(bool restart, void *user_data)
{
    ARG_UNUSED(user_data);

    fill_adv_msd();

    if (restart) {
        return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
                               sd, ARRAY_SIZE(sd));
    }

    return bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
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

static ssize_t device_health_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    uint8_t payload[VBRK_DEVICE_HEALTH_SIZE];
    vbrk_device_health_t health;

    ARG_UNUSED(attr);

    device_health_get(&health);
    vbrk_device_health_encode(payload, &health);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, payload, sizeof(payload));
}

static int dispatcher_read_one(uint8_t slot, vbrk_slot_record_t *record, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_read_one(slot, record);
}

static void read_all_schedule_next(uint16_t delay_ms, void *user_data)
{
    ARG_UNUSED(user_data);
    (void)k_work_schedule(&read_all_work, K_MSEC(delay_ms));
}

static int dispatcher_write_one(const vbrk_slot_record_t *record, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_write_one(record);
}

static int dispatcher_clear_one(uint8_t slot, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_clear_one(slot);
}

static int dispatcher_insert_at(uint8_t slot, const vbrk_slot_record_t *record,
                                void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_insert_at(slot, record);
}

static int dispatcher_remove_at(uint8_t slot, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_remove_at(slot);
}

static int dispatcher_move_block(uint8_t from, uint8_t to, uint8_t len, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_move_block(from, to, len);
}

static int dispatcher_set_qty(uint8_t slot, uint16_t qty, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_set_qty(slot, qty);
}

static int dispatcher_factory_reset(uint32_t magic, void *user_data)
{
    ARG_UNUSED(user_data);
    return binding_table_factory_reset(magic);
}

static int dispatcher_notify_binding(uint8_t op, uint8_t status, const void *payload,
                                     uint16_t len, void *user_data)
{
    ARG_UNUSED(user_data);
    return app_ble_notify_binding_result(op, status, payload, len);
}

static void dispatcher_table_changed(void *user_data)
{
    ARG_UNUSED(user_data);
    app_ble_report_binding_changed();
}

static void notify_binding_status(uint8_t op, int err)
{
    (void)app_ble_notify_binding_result(op, vbrk_ble_status_from_errno(err),
                                        NULL, 0);
}

static bool handle_read_all_async(const uint8_t *frame, uint16_t len)
{
    if (frame[0] != VBRK_OP_READ_ALL) {
        return false;
    }

    if (len != 1) {
        notify_binding_status(VBRK_OP_READ_ALL, -EINVAL);
        return true;
    }

    vbrk_read_all_pacer_start(&read_all_pacer);
    return true;
}

static ssize_t binding_cp_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags)
{
    const uint8_t *frame = buf;
    vbrk_ble_dispatcher_t dispatcher = {
        .read_one = dispatcher_read_one,
        .write_one = dispatcher_write_one,
        .clear_one = dispatcher_clear_one,
        .insert_at = dispatcher_insert_at,
        .remove_at = dispatcher_remove_at,
        .move_block = dispatcher_move_block,
        .set_qty = dispatcher_set_qty,
        .factory_reset = dispatcher_factory_reset,
        .notify_binding = dispatcher_notify_binding,
        .table_changed = dispatcher_table_changed,
        .user_data = NULL,
    };

    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (offset != 0 || len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (handle_read_all_async(frame, len)) {
        return len;
    }

    (void)vbrk_ble_dispatch_binding_cp(&dispatcher, frame, len);
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

BT_GATT_SERVICE_DEFINE(health_svc,
    BT_GATT_PRIMARY_SERVICE(&health_service_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&device_health_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           device_health_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static void connected(struct bt_conn *conn, uint8_t err)
{
    int sec_err;

    if (err != 0) {
        LOG_WRN("connection failed: %u", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    vbrk_ble_lifecycle_connected(&ble_lifecycle);
    app_status_set_ble_connected(true);
    LOG_INF("connected");

    sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (sec_err != 0) {
        LOG_WRN("failed to request encrypted link: %d", sec_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_INF("disconnected: %u", reason);
    if (current_conn != NULL) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    vbrk_read_all_pacer_cancel(&read_all_pacer);
    (void)k_work_cancel_delayable(&read_all_work);
    vbrk_ble_lifecycle_disconnected(&ble_lifecycle);
    app_status_set_ble_connected(false);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    ARG_UNUSED(conn);

    if (err == BT_SECURITY_ERR_SUCCESS) {
        LOG_INF("security changed: level %u", level);
        return;
    }

    LOG_WRN("security failed: level %u err %u", level, err);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static int lifecycle_notify_binding(uint8_t op, uint8_t status, const void *payload,
                                    uint16_t len, void *user_data)
{
    uint8_t frame[2 + VBRK_SLOT_RECORD_SIZE];

    ARG_UNUSED(user_data);

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

static int lifecycle_notify_table_info(void *user_data)
{
    vbrk_table_info_t info;

    ARG_UNUSED(user_data);

    if (current_conn == NULL) {
        return -ENOTCONN;
    }

    binding_table_get_info(&info);
    return bt_gatt_notify(current_conn, &binding_svc.attrs[5], &info, sizeof(info));
}

static int lifecycle_notify_light_status(uint8_t mode, uint16_t remaining_s,
                                         void *user_data)
{
    uint8_t status[3];

    ARG_UNUSED(user_data);

    if (current_conn == NULL) {
        return -ENOTCONN;
    }

    status[0] = mode;
    sys_put_le16(remaining_s, &status[1]);
    return bt_gatt_notify(current_conn, &light_svc.attrs[4], status, sizeof(status));
}

int app_ble_notify_binding_result(uint8_t op, uint8_t status, const void *payload, uint16_t len)
{
    return vbrk_ble_lifecycle_notify_binding(&ble_lifecycle, op, status,
                                             payload, len);
}

void app_ble_report_binding_changed(void)
{
    vbrk_ble_lifecycle_report_binding_changed(&ble_lifecycle);
}

void app_ble_report_light_changed(void)
{
    vbrk_ble_lifecycle_report_light_changed(&ble_lifecycle,
                                            light_control_mode(),
                                            light_control_remaining_s());
}

void app_ble_report_nfc_fd(void)
{
    vbrk_ble_lifecycle_report_nfc_fd(&ble_lifecycle);
}

int app_ble_init(void)
{
    vbrk_ble_lifecycle_adapter_t adapter = {
        .lock = lifecycle_lock,
        .unlock = lifecycle_unlock,
        .schedule_work = lifecycle_schedule_work,
        .apply_advertising = lifecycle_apply_advertising,
        .notify_binding = lifecycle_notify_binding,
        .notify_table_info = lifecycle_notify_table_info,
        .notify_light_status = lifecycle_notify_light_status,
        .user_data = NULL,
    };
    int err;

    k_work_init(&ble_lifecycle_work, ble_lifecycle_work_handler);
    k_work_init_delayable(&read_all_work, read_all_work_handler);
    vbrk_ble_lifecycle_init(&ble_lifecycle, &adapter);
    vbrk_read_all_pacer_init(&read_all_pacer,
                             &(vbrk_read_all_pacer_adapter_t) {
                                 .read_one = dispatcher_read_one,
                                 .notify_binding = dispatcher_notify_binding,
                                 .schedule_next = read_all_schedule_next,
                                 .user_data = NULL,
                             });

    err = bt_enable(NULL);
    if (err != 0) {
        return err;
    }

    vbrk_ble_lifecycle_bluetooth_initialized(&ble_lifecycle);
    LOG_INF("Bluetooth initialized");
    return 0;
}

int app_ble_start(void)
{
    int err;

    err = vbrk_ble_lifecycle_start(&ble_lifecycle);
    if (err == 0) {
        LOG_INF("advertising started");
    }

    return err;
}
