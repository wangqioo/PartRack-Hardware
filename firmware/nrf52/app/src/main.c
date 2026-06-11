#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"
#include "binding_table.h"
#include "light_control.h"
#include "nfc_wake.h"

LOG_MODULE_REGISTER(part_rack, LOG_LEVEL_INF);

#define LED_RED_NODE DT_ALIAS(led0)
#define LED_GREEN_NODE DT_ALIAS(led1)
#define LED_BLUE_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static volatile bool ble_connected;

static void status_led_set(const struct gpio_dt_spec *led, bool on)
{
    if (device_is_ready(led->port)) {
        gpio_pin_set_dt(led, on ? 1 : 0);
    }
}

static void status_leds_init(void)
{
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    }
    if (device_is_ready(led_green.port)) {
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }
    if (device_is_ready(led_blue.port)) {
        gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    }
}

static void blink_status(const struct gpio_dt_spec *led, int count, int on_ms, int off_ms)
{
    for (int i = 0; i < count; i++) {
        status_led_set(led, true);
        k_msleep(on_ms);
        status_led_set(led, false);
        k_msleep(off_ms);
    }
}

int main(void)
{
    int err;

    status_leds_init();
    blink_status(&led_blue, 3, 100, 100);
    status_led_set(&led_red, true);

    LOG_INF("PartRack nRF52 firmware starting");

    err = binding_table_init();
    if (err != 0) {
        LOG_ERR("binding table init failed: %d", err);
        while (true) {
            blink_status(&led_red, 1, 500, 500);
        }
        return err;
    }

    err = light_control_init();
    if (err != 0) {
        LOG_ERR("light control init failed: %d", err);
        while (true) {
            blink_status(&led_red, 2, 250, 250);
            k_msleep(750);
        }
        return err;
    }

    err = nfc_wake_init();
    if (err != 0) {
        LOG_WRN("nfc wake init failed: %d", err);
    }

    err = app_ble_start();
    if (err != 0) {
        LOG_ERR("ble start failed: %d", err);
        status_led_set(&led_green, false);
        while (true) {
            blink_status(&led_red, 3, 200, 200);
            k_msleep(1000);
        }
        return err;
    }

    status_led_set(&led_red, false);
    LOG_INF("PartRack firmware ready");

    while (true) {
        if (ble_connected) {
            status_led_set(&led_green, false);
            status_led_set(&led_blue, true);
            k_msleep(200);
            continue;
        }

        status_led_set(&led_blue, false);
        blink_status(&led_green, 1, 200, 1800);
    }
}

void app_status_set_ble_connected(bool connected)
{
    ble_connected = connected;
}
