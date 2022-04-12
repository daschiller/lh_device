// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "adc.h"
#include "ble.h"
#include "temp.h"
#include <bluetooth/services/bas.h>
#include <drivers/sensor.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <pm/device.h>

LOG_MODULE_REGISTER(main);

#define NRF_UART_ERRATUM
#define LOW_POWER

#ifdef LOW_POWER
static int pm_devices(enum pm_device_action action) {
    int err;

#ifdef NRF_UART_ERRATUM
    if (action == PM_DEVICE_ACTION_SUSPEND) {
        // ERRATUM: properly power down UARTE1 to reduce current consumption
        // https://devzone.nordicsemi.com/f/nordic-q-a/26030/how-to-reach-nrf52840-uarte-current-supply-specification/184882#184882
        // https://devzone.nordicsemi.com/f/nordic-q-a/59407/simple-question-about-the-peripheral-reset-code
        NRF_UARTE1->TASKS_STOPRX = 1;
        while (!NRF_UARTE1->EVENTS_RXTO) {
        }
    }
#endif
    // const struct device *fuel_dev = DEVICE_DT_GET(DT_ALIAS(fuel_gauge));
    // err = pm_device_action_run(fuel_dev, action);
    // if (err) {
    //     LOG_ERR("Fuel gauge action failed (err %d)", err);
    //     return err;
    // }

    const struct device *uart1_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    err = pm_device_action_run(uart1_dev, action);
    if (err) {
        LOG_ERR("UART1 action failed (err %d)", err);
        return err;
    }

    const struct device *uart0_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    err = pm_device_action_run(uart0_dev, action);
    if (err) {
        LOG_ERR("UART (console) action failed (err %d)", err);
        return err;
    }

    return 0;
}
#endif /* LOW_POWER */

extern STATS_SECT_DECL(dev_stats) dev_stats;

void main(void) {
    printk("Starting LionHearted device ...\n");
    printk("Build time: " __DATE__ " " __TIME__ "\n");

    setup_ble();

    printk("Temperature (chip): %f\n", read_chip_temp() / 1000.0);
    printk("Temperature (probe): %f\n", read_ext_temp() / 1000.0);
    printk("ADC: %f\n", read_adc() / 1000.0);

#ifdef LOW_POWER
    // pm_devices(PM_DEVICE_ACTION_SUSPEND);
#endif

    // k_sleep(K_FOREVER);

    const struct device *fuel_dev = DEVICE_DT_GET(DT_ALIAS(fuel_gauge));
    struct sensor_value voltage;
    sensor_sample_fetch(fuel_dev);
    sensor_channel_get(fuel_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage);

    uint8_t level = 100;
    bt_bas_set_battery_level(level);
    for (;;) {
        STATS_SET(dev_stats, uptime_s, k_uptime_get() / 1000);
        printk("Batt: %d\n", bt_bas_get_battery_level());
        if (bt_bas_set_battery_level(--level) == -EINVAL) {
            level = 100;
        }
        pm_devices(PM_DEVICE_ACTION_SUSPEND);
        k_msleep(5000);
        pm_devices(PM_DEVICE_ACTION_RESUME);
        printk("\nwakeup\n");
        // printk("Temperature (probe): %f\n", read_ext_temp() / 1000.0);
    }
}
