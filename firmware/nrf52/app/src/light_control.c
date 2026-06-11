#include "light_control.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"
#include "viberack_light_frame.h"
#include "viberack_light_policy.h"

LOG_MODULE_REGISTER(light_control, LOG_LEVEL_INF);

#if DT_HAS_ALIAS(vbrk_led_data) && DT_HAS_ALIAS(vbrk_led_power)
static const struct gpio_dt_spec led_data_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(vbrk_led_data), gpios);
static const struct gpio_dt_spec led_power_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(vbrk_led_power), gpios);
#endif

static struct k_work_delayable off_work;
static uint8_t active_mode;
static int64_t expires_at_ms;
static vbrk_light_frame_t active_frame;

static int configure_light_outputs(void)
{
#if DT_HAS_ALIAS(vbrk_led_data) && DT_HAS_ALIAS(vbrk_led_power)
    int err;

    if (!gpio_is_ready_dt(&led_data_gpio) || !gpio_is_ready_dt(&led_power_gpio)) {
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&led_data_gpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        return err;
    }

    return gpio_pin_configure_dt(&led_power_gpio, GPIO_OUTPUT_INACTIVE);
#else
    LOG_WRN("vbrk-led-data or vbrk-led-power devicetree alias not defined");
    return -ENODEV;
#endif
}

static void set_light_outputs(bool power_on)
{
#if DT_HAS_ALIAS(vbrk_led_data) && DT_HAS_ALIAS(vbrk_led_power)
    (void)gpio_pin_set_dt(&led_data_gpio, 0);
    (void)gpio_pin_set_dt(&led_power_gpio, power_on ? 1 : 0);
#else
    ARG_UNUSED(power_on);
#endif
}

static void drive_leds_off(void)
{
    memset(&active_frame, 0, sizeof(active_frame));
    set_light_outputs(false);
    LOG_INF("light off");
}

static void drive_leds_command(const vbrk_light_command_t *command)
{
    uint8_t active_slots = 0;
    int err;

    err = vbrk_light_frame_build(command, &active_frame);
    if (err != 0) {
        LOG_WRN("light frame build failed: %d", err);
        return;
    }

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        const vbrk_rgb_t *rgb = &active_frame.slots[i];

        if (rgb->r != 0 || rgb->g != 0 || rgb->b != 0) {
            active_slots++;
        }
    }

    set_light_outputs(true);
    /*
     * TODO: implement WS2812 output with PWM + EasyDMA.
     * mask_a/mask_b and RGB colors are already in the protocol frame.
     */
    LOG_INF("light mode=%u active_slots=%u mask_a=0x%08x mask_b=0x%08x",
            command->mode, active_slots, command->mask_a_le, command->mask_b_le);
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
    int err;

    err = configure_light_outputs();
    if (err != 0) {
        return err;
    }

    k_work_init_delayable(&off_work, off_work_handler);
    active_mode = VBRK_LIGHT_OFF;
    expires_at_ms = 0;
    drive_leds_off();
    return 0;
}

int light_control_apply(const vbrk_light_command_t *command)
{
    vbrk_light_policy_t policy;
    int err;

    err = vbrk_light_policy_resolve(command, &policy);
    if (err != 0) {
        return err;
    }

    k_work_cancel_delayable(&off_work);

    if (!policy.power_on) {
        off_work_handler(NULL);
        return 0;
    }

    drive_leds_command(command);
    active_mode = policy.mode;
    expires_at_ms = k_uptime_get() + ((int64_t)policy.timeout_s * MSEC_PER_SEC);

    app_ble_set_light_active(true);
    app_ble_notify_light_status(active_mode, policy.timeout_s);
    app_ble_refresh_advertising();
    k_work_schedule(&off_work, K_SECONDS(policy.timeout_s));

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
