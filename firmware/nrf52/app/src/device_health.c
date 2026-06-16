#include "device_health.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "viberack_device_health.h"

LOG_MODULE_REGISTER(device_health, LOG_LEVEL_INF);

#define BATTERY_EMPTY_MV 3000
#define BATTERY_FULL_MV 4200
#define WATCHDOG_TIMEOUT_MS 8000

#if DT_HAS_ALIAS(vbrk_battery_adc)
static const struct adc_dt_spec battery_adc =
    ADC_DT_SPEC_GET(DT_ALIAS(vbrk_battery_adc));
static int16_t battery_raw;
#endif

#if DT_HAS_ALIAS(watchdog0)
static const struct device *const watchdog = DEVICE_DT_GET(DT_ALIAS(watchdog0));
#endif

static uint8_t battery_pct = 100;
static uint16_t reset_reason = VBRK_RESET_REASON_UNKNOWN;
static bool watchdog_enabled;
static bool health_fault;
static int watchdog_channel_id = -1;

static uint16_t map_reset_reason(uint32_t cause)
{
    uint16_t mapped = VBRK_RESET_REASON_UNKNOWN;

    if ((cause & RESET_POR) != 0u) {
        mapped |= VBRK_RESET_REASON_POWER_ON;
    }
    if ((cause & RESET_PIN) != 0u) {
        mapped |= VBRK_RESET_REASON_PIN;
    }
    if ((cause & RESET_SOFTWARE) != 0u) {
        mapped |= VBRK_RESET_REASON_SOFTWARE;
    }
    if ((cause & RESET_WATCHDOG) != 0u) {
        mapped |= VBRK_RESET_REASON_WATCHDOG;
    }
    if ((cause & RESET_CPU_LOCKUP) != 0u) {
        mapped |= VBRK_RESET_REASON_LOCKUP;
    }
    if ((cause & RESET_LOW_POWER_WAKE) != 0u) {
        mapped |= VBRK_RESET_REASON_OFF;
    }
    if ((cause & ~(RESET_POR | RESET_PIN | RESET_SOFTWARE | RESET_WATCHDOG |
                   RESET_CPU_LOCKUP | RESET_LOW_POWER_WAKE)) != 0u) {
        mapped |= VBRK_RESET_REASON_OTHER;
    }

    return mapped;
}

static void load_reset_reason(void)
{
    uint32_t cause = 0;
    int err;

    if (!IS_ENABLED(CONFIG_HWINFO)) {
        LOG_WRN("hwinfo disabled; reset reason unavailable");
        return;
    }

    err = hwinfo_get_reset_cause(&cause);
    if (err != 0) {
        LOG_WRN("failed to read reset reason: %d", err);
        health_fault = true;
        return;
    }

    reset_reason = map_reset_reason(cause);
    LOG_INF("reset reason cause=0x%08x mapped=0x%04x", cause, reset_reason);

    err = hwinfo_clear_reset_cause();
    if (err != 0) {
        LOG_WRN("failed to clear reset reason: %d", err);
    }
}

static void sample_battery(void)
{
#if DT_HAS_ALIAS(vbrk_battery_adc)
    struct adc_sequence sequence = {
        .buffer = &battery_raw,
        .buffer_size = sizeof(battery_raw),
    };
    int32_t mv;
    int err;

    if (!adc_is_ready_dt(&battery_adc)) {
        LOG_WRN("battery ADC device is not ready");
        health_fault = true;
        return;
    }

    err = adc_channel_setup_dt(&battery_adc);
    if (err != 0) {
        LOG_WRN("battery ADC channel setup failed: %d", err);
        health_fault = true;
        return;
    }

    err = adc_sequence_init_dt(&battery_adc, &sequence);
    if (err != 0) {
        LOG_WRN("battery ADC sequence setup failed: %d", err);
        health_fault = true;
        return;
    }

    err = adc_read_dt(&battery_adc, &sequence);
    if (err != 0) {
        LOG_WRN("battery ADC read failed: %d", err);
        health_fault = true;
        return;
    }

    mv = battery_raw;
    err = adc_raw_to_millivolts_dt(&battery_adc, &mv);
    if (err != 0) {
        LOG_WRN("battery ADC mv conversion failed: %d", err);
        health_fault = true;
        return;
    }

    battery_pct = vbrk_battery_percent_from_mv((uint16_t)mv,
                                               BATTERY_EMPTY_MV,
                                               BATTERY_FULL_MV);
    LOG_INF("battery raw=%d mv=%d pct=%u", battery_raw, mv, battery_pct);
#else
    battery_pct = 100;
#endif
}

static int init_watchdog(void)
{
    if (!IS_ENABLED(CONFIG_VBRK_WATCHDOG_ENABLE)) {
        return 0;
    }

#if DT_HAS_ALIAS(watchdog0)
    struct wdt_timeout_cfg watchdog_cfg = {
        .window = {
            .min = 0,
            .max = WATCHDOG_TIMEOUT_MS,
        },
        .flags = WDT_FLAG_RESET_SOC,
    };
    int err;

    if (!device_is_ready(watchdog)) {
        LOG_WRN("watchdog device is not ready");
        health_fault = true;
        return -ENODEV;
    }

    watchdog_channel_id = wdt_install_timeout(watchdog, &watchdog_cfg);
    if (watchdog_channel_id < 0) {
        LOG_WRN("watchdog install failed: %d", watchdog_channel_id);
        health_fault = true;
        return watchdog_channel_id;
    }

    err = wdt_setup(watchdog, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (err != 0) {
        LOG_WRN("watchdog setup failed: %d", err);
        health_fault = true;
        return err;
    }

    watchdog_enabled = true;
    LOG_INF("watchdog enabled timeout_ms=%u", WATCHDOG_TIMEOUT_MS);
    return 0;
#else
    LOG_WRN("watchdog enabled in Kconfig, but watchdog0 alias is missing");
    health_fault = true;
    return -ENODEV;
#endif
}

int device_health_init(void)
{
    load_reset_reason();
    sample_battery();
    return init_watchdog();
}

uint8_t device_health_battery_pct(void)
{
    sample_battery();
    return battery_pct;
}

uint16_t device_health_reset_reason(void)
{
    return reset_reason;
}

bool device_health_watchdog_enabled(void)
{
    return watchdog_enabled;
}

bool device_health_fault(void)
{
    return health_fault;
}

void device_health_feed_watchdog(void)
{
#if DT_HAS_ALIAS(watchdog0)
    if (watchdog_enabled && watchdog_channel_id >= 0) {
        int err = wdt_feed(watchdog, watchdog_channel_id);

        if (err != 0) {
            LOG_WRN("watchdog feed failed: %d", err);
            health_fault = true;
        }
    }
#endif
}

void device_health_get(vbrk_device_health_t *health)
{
    if (health == NULL) {
        return;
    }

    health->battery_pct = device_health_battery_pct();
    health->reset_reason = reset_reason;
    health->watchdog_enabled = watchdog_enabled;
    health->fault = health_fault;
}
