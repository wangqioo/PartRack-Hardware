#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "viberack_read_all_pacer.h"

#define MAX_ATTEMPTS 40
#define MAX_NOTIFY_LEN (2 + VBRK_SLOT_RECORD_SIZE)

typedef struct {
    uint8_t frames[MAX_ATTEMPTS][MAX_NOTIFY_LEN];
    uint16_t lens[MAX_ATTEMPTS];
    uint16_t delays[MAX_ATTEMPTS];
    uint8_t notify_count;
    uint8_t schedule_count;
    uint8_t read_fail_slot;
    int first_notify_result;
} fake_read_all_t;

static void reset_fake(fake_read_all_t *fake)
{
    memset(fake, 0, sizeof(*fake));
}

static int fake_read_one(uint8_t slot, vbrk_slot_record_t *record, void *user_data)
{
    fake_read_all_t *fake = user_data;

    if (slot == fake->read_fail_slot) {
        return -EINVAL;
    }

    memset(record, 0, sizeof(*record));
    record->slot = slot;
    record->qty_le = (uint16_t)(slot * 10);
    return 0;
}

static int fake_notify_binding(uint8_t op, uint8_t status, const void *payload,
                               uint16_t len, void *user_data)
{
    fake_read_all_t *fake = user_data;
    uint8_t index = fake->notify_count;

    assert(index < MAX_ATTEMPTS);
    assert(len <= VBRK_SLOT_RECORD_SIZE);
    fake->frames[index][0] = op;
    fake->frames[index][1] = status;
    if (payload != NULL && len > 0) {
        memcpy(&fake->frames[index][2], payload, len);
    }
    fake->lens[index] = (uint16_t)(2 + len);
    fake->notify_count++;

    if (index == 0 && fake->first_notify_result != 0) {
        return fake->first_notify_result;
    }

    return 0;
}

static void fake_schedule_next(uint16_t delay_ms, void *user_data)
{
    fake_read_all_t *fake = user_data;

    assert(fake->schedule_count < MAX_ATTEMPTS);
    fake->delays[fake->schedule_count++] = delay_ms;
}

static vbrk_read_all_pacer_adapter_t make_adapter(fake_read_all_t *fake)
{
    vbrk_read_all_pacer_adapter_t adapter = {
        .read_one = fake_read_one,
        .notify_binding = fake_notify_binding,
        .schedule_next = fake_schedule_next,
        .user_data = fake,
    };

    return adapter;
}

static void init_pacer(vbrk_read_all_pacer_t *pacer, fake_read_all_t *fake)
{
    vbrk_read_all_pacer_adapter_t adapter;

    reset_fake(fake);
    adapter = make_adapter(fake);
    vbrk_read_all_pacer_init(pacer, &adapter);
}

static void test_start_schedules_first_frame(void)
{
    vbrk_read_all_pacer_t pacer;
    fake_read_all_t fake;

    init_pacer(&pacer, &fake);
    vbrk_read_all_pacer_start(&pacer);

    assert(vbrk_read_all_pacer_is_active(&pacer));
    assert(fake.schedule_count == 1);
    assert(fake.delays[0] == 0);
}

static void test_process_sends_one_frame_per_tick_and_end_marker(void)
{
    vbrk_read_all_pacer_t pacer;
    fake_read_all_t fake;

    init_pacer(&pacer, &fake);
    vbrk_read_all_pacer_start(&pacer);

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT + 1; i++) {
        vbrk_read_all_pacer_process(&pacer);
    }

    assert(fake.notify_count == VBRK_SLOT_COUNT + 1);
    assert(fake.frames[0][0] == VBRK_OP_READ_ALL);
    assert(fake.frames[0][1] == VBRK_STATUS_OK);
    assert(fake.frames[0][2] == 1);
    assert(fake.lens[0] == 2 + VBRK_SLOT_RECORD_SIZE);

    assert(fake.frames[VBRK_SLOT_COUNT][0] == VBRK_OP_READ_ALL);
    assert(fake.frames[VBRK_SLOT_COUNT][1] == VBRK_STATUS_OK);
    assert(fake.frames[VBRK_SLOT_COUNT][2] == VBRK_READ_ALL_END_MARKER);
    assert(fake.lens[VBRK_SLOT_COUNT] == 3);
    assert(!vbrk_read_all_pacer_is_active(&pacer));
}

static void test_notify_failure_retries_same_slot_without_advancing(void)
{
    vbrk_read_all_pacer_t pacer;
    fake_read_all_t fake;

    init_pacer(&pacer, &fake);
    fake.first_notify_result = -ENOMEM;
    vbrk_read_all_pacer_start(&pacer);

    vbrk_read_all_pacer_process(&pacer);
    assert(vbrk_read_all_pacer_is_active(&pacer));
    assert(fake.notify_count == 1);
    assert(fake.frames[0][2] == 1);

    vbrk_read_all_pacer_process(&pacer);
    assert(fake.notify_count == 2);
    assert(fake.frames[1][2] == 1);
}

static void test_read_failure_sends_error_and_stops(void)
{
    vbrk_read_all_pacer_t pacer;
    fake_read_all_t fake;

    init_pacer(&pacer, &fake);
    fake.read_fail_slot = 2;
    vbrk_read_all_pacer_start(&pacer);

    vbrk_read_all_pacer_process(&pacer);
    vbrk_read_all_pacer_process(&pacer);

    assert(fake.notify_count == 2);
    assert(fake.frames[1][0] == VBRK_OP_READ_ALL);
    assert(fake.frames[1][1] == VBRK_STATUS_ERR_PARAM);
    assert(fake.lens[1] == 2);
    assert(!vbrk_read_all_pacer_is_active(&pacer));
}

static void test_cancel_stops_future_frames(void)
{
    vbrk_read_all_pacer_t pacer;
    fake_read_all_t fake;

    init_pacer(&pacer, &fake);
    vbrk_read_all_pacer_start(&pacer);
    vbrk_read_all_pacer_cancel(&pacer);
    vbrk_read_all_pacer_process(&pacer);

    assert(fake.notify_count == 0);
    assert(!vbrk_read_all_pacer_is_active(&pacer));
}

int main(void)
{
    test_start_schedules_first_frame();
    test_process_sends_one_frame_per_tick_and_end_marker();
    test_notify_failure_retries_same_slot_without_advancing();
    test_read_failure_sends_error_and_stops();
    test_cancel_stops_future_frames();
    return 0;
}
