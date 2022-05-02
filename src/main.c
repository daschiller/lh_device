// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "battery.h"
#include "ble.h"
#include "power.h"
#include "temp.h"
#include <bluetooth/services/bas.h>
#include <drivers/watchdog.h>
#include <logging/log.h>
#include <pm/device.h>

LOG_MODULE_REGISTER(main);

#define LOW_POWER
#define LOG_INTERVAL 30000
#define WDT_INTERVAL (2 * LOG_INTERVAL)

extern STATS_SECT_DECL(dev_stats) dev_stats;
static const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt));
static int wdt_channel;

int setup_watchdog(void) {
    const struct wdt_timeout_cfg wdt_cfg = {
        .window = (struct wdt_window){0, WDT_INTERVAL},
        .flags = WDT_FLAG_RESET_SOC,
    };

    wdt_channel = wdt_install_timeout(wdt_dev, &wdt_cfg);
    if (wdt_channel >= 0) {
        return wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    } else {
        return wdt_channel;
    }
}

void main(void) {
    int err;

    printk("Starting LionHearted device ...\n");
    printk("Build time: " __DATE__ " " __TIME__ "\n");
    printk("Reset reason: 0x%X\n", NRF_POWER->RESETREAS);

    err = setup_watchdog();
    if (err) {
        LOG_ERR("WDT setup failed (err %d)", err);
    }
    wdt_feed(wdt_dev, wdt_channel);

    setup_ble();

#ifdef LOW_POWER
    pm_console(PM_DEVICE_ACTION_SUSPEND);
    pm_w1(PM_DEVICE_ACTION_SUSPEND);
    pm_fuel_gauge(PM_DEVICE_ACTION_SUSPEND);
    // k_sleep(K_FOREVER);
#endif

    for (;;) {
        wdt_feed(wdt_dev, wdt_channel);
        STATS_SET(dev_stats, uptime_s, k_uptime_get() / 1000);
        bt_bas_set_battery_level(read_soc());
        LOG_DBG("Batt: %d %%", bt_bas_get_battery_level());
        LOG_DBG("Batt (voltage): %d mV", read_voltage());
        k_msleep(LOG_INTERVAL);
    }
}
