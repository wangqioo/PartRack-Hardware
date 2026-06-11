#include "nfc_wake.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"

LOG_MODULE_REGISTER(nfc_wake, LOG_LEVEL_INF);

#if DT_ALIAS_EXISTS(vbrk_nfc_fd)
static const struct gpio_dt_spec fd_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(vbrk_nfc_fd), gpios);
static struct gpio_callback fd_callback;
static int64_t last_fd_ms;

static void nfc_fd_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int64_t now = k_uptime_get();

    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    if (now - last_fd_ms < 50) {
        return;
    }

    last_fd_ms = now;
    LOG_INF("NFC field detect wake");
    app_ble_refresh_advertising();
}
#endif

int nfc_wake_init(void)
{
#if DT_ALIAS_EXISTS(vbrk_nfc_fd)
    int err;

    if (!gpio_is_ready_dt(&fd_gpio)) {
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&fd_gpio, GPIO_INPUT);
    if (err != 0) {
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&fd_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    if (err != 0) {
        return err;
    }

    gpio_init_callback(&fd_callback, nfc_fd_handler, BIT(fd_gpio.pin));
    err = gpio_add_callback(fd_gpio.port, &fd_callback);
    if (err != 0) {
        return err;
    }

    LOG_INF("NFC FD wake GPIO armed");
    return 0;
#else
    LOG_WRN("vbrk-nfc-fd devicetree alias not defined");
    return -ENODEV;
#endif
}
