#include "light_control.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"
#include "viberack_light_frame.h"
#include "viberack_light_policy.h"

LOG_MODULE_REGISTER(light_control, LOG_LEVEL_INF);

#if DT_HAS_ALIAS(vbrk_led_data)
static const struct gpio_dt_spec led_data_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(vbrk_led_data), gpios);
#endif

#if DT_HAS_ALIAS(vbrk_led_power)
static const struct gpio_dt_spec led_power_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(vbrk_led_power), gpios);
#endif

#if DT_HAS_ALIAS(vbrk_led_strip)
static const struct device *const led_strip = DEVICE_DT_GET(DT_ALIAS(vbrk_led_strip));
static struct led_rgb strip_pixels[VBRK_SLOT_COUNT];
#endif

static struct k_work_delayable off_work;
static uint8_t active_mode;
static int64_t expires_at_ms;
static vbrk_light_frame_t active_frame;
static vbrk_rgb_t active_pixels[VBRK_SLOT_COUNT];

static int configure_light_outputs(void)
{
#if DT_HAS_ALIAS(vbrk_led_power)
    if (!gpio_is_ready_dt(&led_power_gpio)) {
        return -ENODEV;
    }

#if DT_HAS_ALIAS(vbrk_led_data)
    int err;

    if (!gpio_is_ready_dt(&led_data_gpio)) {
        return -ENODEV;
    }
    err = gpio_pin_configure_dt(&led_data_gpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        return err;
    }
#endif

    return gpio_pin_configure_dt(&led_power_gpio, GPIO_OUTPUT_INACTIVE);
#else
    LOG_WRN("vbrk-led-power devicetree alias not defined");
    return -ENODEV;
#endif
}

static int configure_led_strip(void)
{
#if DT_HAS_ALIAS(vbrk_led_strip)
    if (!device_is_ready(led_strip)) {
        return -ENODEV;
    }
    LOG_INF("WS2812 LED strip backend ready");
    return 0;
#else
    LOG_INF("WS2812 LED strip backend not configured");
    return 0;
#endif
}

static void set_light_outputs(bool power_on)
{
#if DT_HAS_ALIAS(vbrk_led_data)
    (void)gpio_pin_set_dt(&led_data_gpio, 0);
#endif
#if DT_HAS_ALIAS(vbrk_led_power)
    (void)gpio_pin_set_dt(&led_power_gpio, power_on ? 1 : 0);
#else
    ARG_UNUSED(power_on);
#endif
}

static void show_active_pixels(uint8_t active_slots)
{
    ARG_UNUSED(active_slots);

#if DT_HAS_ALIAS(vbrk_led_strip)
    int err;

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        strip_pixels[i].r = active_pixels[i].r;
        strip_pixels[i].g = active_pixels[i].g;
        strip_pixels[i].b = active_pixels[i].b;
    }

    err = led_strip_update_rgb(led_strip, strip_pixels, VBRK_SLOT_COUNT);
    if (err != 0) {
        LOG_WRN("LED strip update failed: %d", err);
    }
#endif
}

static void drive_leds_off(void)
{
    uint8_t active_slots = 0;

    memset(&active_frame, 0, sizeof(active_frame));
    memset(active_pixels, 0, sizeof(active_pixels));
    show_active_pixels(active_slots);
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

    err = vbrk_light_frame_copy_pixels(&active_frame, active_pixels,
                                       VBRK_SLOT_COUNT, &active_slots);
    if (err != 0) {
        LOG_WRN("light frame pixel copy failed: %d", err);
        return;
    }

    set_light_outputs(true);
    show_active_pixels(active_slots);
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
    err = configure_led_strip();
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
