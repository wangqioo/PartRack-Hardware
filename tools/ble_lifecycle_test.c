#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "viberack_ble_lifecycle.h"

typedef struct {
    unsigned lock_count;
    unsigned unlock_count;
    unsigned schedule_count;
    unsigned apply_count;
    bool restart_args[16];
    int apply_result;
    int notify_result;
    bool invalidate_during_apply;
    vbrk_ble_lifecycle_t *lifecycle;
    unsigned binding_count;
    unsigned table_info_count;
    unsigned light_status_count;
    uint8_t last_binding_op;
    uint8_t last_binding_status;
    uint16_t last_binding_len;
    uint8_t last_light_mode;
    uint16_t last_light_remaining_s;
} fake_ble_t;

static unsigned int fake_lock(void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->lock_count++;
    return 7;
}

static void fake_unlock(unsigned int key, void *user_data)
{
    fake_ble_t *fake = user_data;

    assert(key == 7);
    fake->unlock_count++;
}

static void fake_schedule_work(void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->schedule_count++;
}

static int fake_apply_advertising(bool restart, void *user_data)
{
    fake_ble_t *fake = user_data;

    assert(fake->apply_count < sizeof(fake->restart_args));
    fake->restart_args[fake->apply_count] = restart;
    fake->apply_count++;
    if (fake->invalidate_during_apply) {
        fake->invalidate_during_apply = false;
        vbrk_ble_lifecycle_report_nfc_fd(fake->lifecycle);
    }
    return fake->apply_result;
}

static int fake_notify_binding(uint8_t op, uint8_t status,
                               const void *payload, uint16_t len,
                               void *user_data)
{
    fake_ble_t *fake = user_data;

    (void)payload;
    fake->binding_count++;
    fake->last_binding_op = op;
    fake->last_binding_status = status;
    fake->last_binding_len = len;
    return fake->notify_result;
}

static int fake_notify_table_info(void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->table_info_count++;
    return fake->notify_result;
}

static int fake_notify_light_status(uint8_t mode, uint16_t remaining_s,
                                    void *user_data)
{
    fake_ble_t *fake = user_data;

    fake->light_status_count++;
    fake->last_light_mode = mode;
    fake->last_light_remaining_s = remaining_s;
    return fake->notify_result;
}

static vbrk_ble_lifecycle_adapter_t make_adapter(fake_ble_t *fake)
{
    vbrk_ble_lifecycle_adapter_t adapter = {
        .lock = fake_lock,
        .unlock = fake_unlock,
        .schedule_work = fake_schedule_work,
        .apply_advertising = fake_apply_advertising,
        .notify_binding = fake_notify_binding,
        .notify_table_info = fake_notify_table_info,
        .notify_light_status = fake_notify_light_status,
        .user_data = fake,
    };

    return adapter;
}

static void init_lifecycle(vbrk_ble_lifecycle_t *lifecycle, fake_ble_t *fake)
{
    vbrk_ble_lifecycle_adapter_t adapter;

    memset(fake, 0, sizeof(*fake));
    fake->apply_result = 0;
    fake->notify_result = 0;
    fake->lifecycle = lifecycle;
    adapter = make_adapter(fake);
    vbrk_ble_lifecycle_init(lifecycle, &adapter);
}

static void test_nfc_before_ready_defers_ble_calls(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    vbrk_ble_lifecycle_report_nfc_fd(&lifecycle);
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(fake.apply_count == 0);
    assert(fake.table_info_count == 0);
    assert(fake.light_status_count == 0);
    assert(fake.schedule_count == 0);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_initial_start_applies_synchronously_and_preserves_dirty_on_failure(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    vbrk_ble_lifecycle_report_nfc_fd(&lifecycle);
    fake.apply_result = -EIO;

    assert(vbrk_ble_lifecycle_start(&lifecycle) == -EIO);
    assert(fake.apply_count == 1);
    assert(fake.restart_args[0]);
    assert(!lifecycle.ready);
    assert(!lifecycle.advertising_running);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));

    fake.apply_result = 0;
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    assert(fake.apply_count == 2);
    assert(fake.restart_args[1]);
    assert(lifecycle.ready);
    assert(lifecycle.advertising_running);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_disconnected_invalidations_coalesce(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_FIND, 30);

    assert(fake.schedule_count == 1);
    assert(lifecycle.work_pending);
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.apply_count == 2);
    assert(!fake.restart_args[1]);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
    assert(!lifecycle.work_pending);
}

static void test_connected_changes_wait_for_disconnect(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_connected(&lifecycle);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(fake.apply_count == 1);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
    vbrk_ble_lifecycle_disconnected(&lifecycle);
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.apply_count == 2);
    assert(fake.restart_args[1]);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_runtime_advertising_failure_preserves_dirty_state(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    fake.apply_result = -EIO;
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(fake.apply_count == 2);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
    assert(!lifecycle.work_pending);
    fake.apply_result = 0;
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    assert(fake.schedule_count == 2);
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.apply_count == 3);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_invalidation_during_update_is_not_lost(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;

    init_lifecycle(&lifecycle, &fake);
    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    fake.invalidate_during_apply = true;
    vbrk_ble_lifecycle_process(&lifecycle);

    assert(fake.apply_count == 2);
    assert(vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
    vbrk_ble_lifecycle_process(&lifecycle);
    assert(fake.apply_count == 3);
    assert(!vbrk_ble_lifecycle_is_advertising_dirty(&lifecycle));
}

static void test_notifications_are_online_only_and_non_durable(void)
{
    vbrk_ble_lifecycle_t lifecycle;
    fake_ble_t fake;
    uint8_t payload[3] = {0xAA, 0xBB, 0xCC};

    init_lifecycle(&lifecycle, &fake);
    assert(vbrk_ble_lifecycle_notify_binding(&lifecycle, VBRK_OP_READ_ONE,
                                             VBRK_STATUS_OK, payload,
                                             sizeof(payload)) == -ENOTCONN);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_PICK, 10);
    assert(fake.binding_count == 0);
    assert(fake.table_info_count == 0);
    assert(fake.light_status_count == 0);

    assert(vbrk_ble_lifecycle_start(&lifecycle) == 0);
    assert(fake.table_info_count == 0);
    assert(fake.light_status_count == 0);

    vbrk_ble_lifecycle_connected(&lifecycle);
    fake.notify_result = -EIO;
    assert(vbrk_ble_lifecycle_notify_binding(&lifecycle, VBRK_OP_READ_ONE,
                                             VBRK_STATUS_OK, payload,
                                             sizeof(payload)) == -EIO);
    vbrk_ble_lifecycle_report_binding_changed(&lifecycle);
    vbrk_ble_lifecycle_report_light_changed(&lifecycle, VBRK_LIGHT_PICK, 10);
    assert(fake.binding_count == 1);
    assert(fake.table_info_count == 1);
    assert(fake.light_status_count == 1);
    assert(fake.last_binding_op == VBRK_OP_READ_ONE);
    assert(fake.last_binding_status == VBRK_STATUS_OK);
    assert(fake.last_binding_len == sizeof(payload));
    assert(fake.last_light_mode == VBRK_LIGHT_PICK);
    assert(fake.last_light_remaining_s == 10);

    vbrk_ble_lifecycle_disconnected(&lifecycle);
    assert(vbrk_ble_lifecycle_notify_binding(&lifecycle, VBRK_OP_READ_ONE,
                                             VBRK_STATUS_OK, payload,
                                             sizeof(payload)) == -ENOTCONN);
    assert(fake.binding_count == 1);
}

int main(void)
{
    test_nfc_before_ready_defers_ble_calls();
    test_initial_start_applies_synchronously_and_preserves_dirty_on_failure();
    test_disconnected_invalidations_coalesce();
    test_connected_changes_wait_for_disconnect();
    test_runtime_advertising_failure_preserves_dirty_state();
    test_invalidation_during_update_is_not_lost();
    test_notifications_are_online_only_and_non_durable();
    return 0;
}
