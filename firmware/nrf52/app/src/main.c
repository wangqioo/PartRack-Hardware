#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_ble.h"
#include "binding_table.h"
#include "light_control.h"
#include "nfc_wake.h"

LOG_MODULE_REGISTER(part_rack, LOG_LEVEL_INF);

int main(void)
{
    int err;

    LOG_INF("PartRack nRF52 firmware starting");

    err = binding_table_init();
    if (err != 0) {
        LOG_ERR("binding table init failed: %d", err);
        return err;
    }

    err = light_control_init();
    if (err != 0) {
        LOG_ERR("light control init failed: %d", err);
        return err;
    }

    err = nfc_wake_init();
    if (err != 0) {
        LOG_WRN("nfc wake init failed: %d", err);
    }

    err = app_ble_start();
    if (err != 0) {
        LOG_ERR("ble start failed: %d", err);
        return err;
    }

    LOG_INF("PartRack firmware ready");

    return 0;
}
