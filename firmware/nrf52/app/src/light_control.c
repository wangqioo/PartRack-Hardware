#include "light_control.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"

LOG_MODULE_REGISTER(light_control, LOG_LEVEL_INF);

static struct k_work_delayable off_work;
static uint8_t active_mode;
static int64_t expires_at_ms;

static uint16_t command_timeout_s(const vbrk_light_command_t *command)
{
    uint16_t timeout = command->timeout_s_le;

    if (timeout == 0) {
        return 30;
    }
    if (timeout > 300) {
        return 300;
    }

    return timeout;
}

static void drive_leds_off(void)
{
    /* TODO: disable PWM output, pull DATA low, then turn off LED P-MOS. */
    LOG_INF("light off");
}

static void drive_leds_command(const vbrk_light_command_t *command)
{
    /*
     * TODO: implement WS2812 output with PWM + EasyDMA.
     * mask_a/mask_b and RGB colors are already in the protocol frame.
     */
    LOG_INF("light mode=%u mask_a=0x%08x mask_b=0x%08x",
            command->mode, command->mask_a_le, command->mask_b_le);
}

static void off_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    active_mode = VBRK_LIGHT_OFF;
    expires_at_ms = 0;
    drive_leds_off();
    app_ble_set_light_active(false);
    app_ble_notify_light_status(active_mode, 0);
    app_ble_refresh_advertising();
}

int light_control_init(void)
{
    k_work_init_delayable(&off_work, off_work_handler);
    active_mode = VBRK_LIGHT_OFF;
    expires_at_ms = 0;
    drive_leds_off();
    return 0;
}

int light_control_apply(const vbrk_light_command_t *command)
{
    uint16_t timeout_s;

    if (command == NULL) {
        return -EINVAL;
    }

    if (command->mode > VBRK_LIGHT_FX) {
        return -EINVAL;
    }

    k_work_cancel_delayable(&off_work);

    if (command->mode == VBRK_LIGHT_OFF) {
        off_work_handler(NULL);
        return 0;
    }

    timeout_s = command_timeout_s(command);
    if (command->mode == VBRK_LIGHT_FX && timeout_s > 10) {
        timeout_s = 10;
    }

    drive_leds_command(command);
    active_mode = command->mode;
    expires_at_ms = k_uptime_get() + ((int64_t)timeout_s * MSEC_PER_SEC);

    app_ble_set_light_active(true);
    app_ble_notify_light_status(active_mode, timeout_s);
    app_ble_refresh_advertising();
    k_work_schedule(&off_work, K_SECONDS(timeout_s));

    return 0;
}

uint8_t light_control_mode(void)
{
    return active_mode;
}

uint16_t light_control_remaining_s(void)
{
    int64_t remaining_ms;

    if (active_mode == VBRK_LIGHT_OFF || expires_at_ms == 0) {
        return 0;
    }

    remaining_ms = expires_at_ms - k_uptime_get();
    if (remaining_ms <= 0) {
        return 0;
    }

    return (uint16_t)((remaining_ms + MSEC_PER_SEC - 1) / MSEC_PER_SEC);
}
