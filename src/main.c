// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "battery.h"
#include "ble.h"
#include "power.h"
#include "storage.h"
#include "temp.h"
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(main);

#define SERIALNO "001"
#define LOW_POWER
#define LOG_INTERVAL 300000
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
    int reboot_count = -1;

    printk("Starting LionHearted device ...\n");
    printk("Serial No.: " SERIALNO "\n");
    printk("Build time: " __DATE__ " " __TIME__ "\n");
    printk("Reset reason: 0x%X\n", NRF_POWER->RESETREAS);

    err = setup_storage();
    if (err) {
        LOG_ERR("Storage setup failed (err %d)", err);
    } else {
        reboot_count = read_storage_i32(REBOOT_ID);
        if (reboot_count < 0) {
            write_storage_i32(REBOOT_ID, 0);
        } else {
            write_storage_i32(REBOOT_ID, ++reboot_count);
        }
        reboot_count = read_storage_i32(REBOOT_ID);
        printk("Reboot count: %d\n", reboot_count);
    }

    err = setup_watchdog();
    if (err) {
        LOG_ERR("WDT setup failed (err %d)", err);
    }
    wdt_feed(wdt_dev, wdt_channel);

    setup_ble();
    STATS_SET(dev_stats, reset_reason, NRF_POWER->RESETREAS);
    STATS_SET(dev_stats, reboots, reboot_count);
    printk("\n");

#ifdef LOW_POWER
    pm_console(PM_DEVICE_ACTION_SUSPEND);
    pm_w1(PM_DEVICE_ACTION_SUSPEND);
    pm_fuel_gauge(PM_DEVICE_ACTION_SUSPEND);
#endif

    for (;;) {
        wdt_feed(wdt_dev, wdt_channel);
        STATS_SET(dev_stats, uptime_s, k_uptime_get() / 1000);
        bt_bas_set_battery_level(read_soc());
        LOG_DBG("Batt: %d %%", bt_bas_get_battery_level());
        LOG_DBG("Batt (voltage): %d mV", read_voltage());
        update_adv();
        k_msleep(LOG_INTERVAL);
    }
}
