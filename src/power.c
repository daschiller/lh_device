// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "power.h"
#include <zephyr/logging/log.h>

#define NRF_UART_ERRATUM

LOG_MODULE_REGISTER(power);

K_MUTEX_DEFINE(pm_mutex);

int pm_console(enum pm_device_action action) {
    int err;
    const struct device *uart0_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    k_mutex_lock(&pm_mutex, K_FOREVER);
#ifdef NRF_UART_ERRATUM
    // ERRATUM: properly power down UARTE to reduce current consumption
    // https://devzone.nordicsemi.com/f/nordic-q-a/26030/how-to-reach-nrf52840-uarte-current-supply-specification/184882#184882
    // https://devzone.nordicsemi.com/f/nordic-q-a/59407/simple-question-about-the-peripheral-reset-code
    if (action == PM_DEVICE_ACTION_SUSPEND) {
        NRF_UARTE0->TASKS_STOPRX = 1;
        while (!NRF_UARTE0->EVENTS_RXTO) {
        }
    }
#endif
    err = pm_device_action_run(uart0_dev, action);
    k_mutex_unlock(&pm_mutex);
    if (err && (err != -EALREADY)) {
        LOG_ERR("UART (console) action failed (err %d)", err);
        return err;
    }

    return 0;
}

int pm_fuel_gauge(enum pm_device_action action) {
    int err;
    const struct device *fuel_dev = DEVICE_DT_GET(DT_ALIAS(fuel_gauge));

    k_mutex_lock(&pm_mutex, K_FOREVER);
    err = pm_device_action_run(fuel_dev, action);
    k_mutex_unlock(&pm_mutex);
    if (err && (err != -EALREADY)) {
        LOG_ERR("Fuel gauge action failed (err %d)", err);
        return err;
    }

    return 0;
}

int pm_w1(enum pm_device_action action) {
    int err;
    const struct device *uart1_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

    k_mutex_lock(&pm_mutex, K_FOREVER);
#ifdef NRF_UART_ERRATUM
    if (action == PM_DEVICE_ACTION_SUSPEND) {
        NRF_UARTE1->TASKS_STOPRX = 1;
        while (!NRF_UARTE1->EVENTS_RXTO) {
        }
    }
#endif
    err = pm_device_action_run(uart1_dev, action);
    k_mutex_unlock(&pm_mutex);
    if (err && (err != -EALREADY)) {
        LOG_ERR("UART1 action failed (err %d)", err);
        return err;
    }

    k_msleep(10);

    return 0;
}
