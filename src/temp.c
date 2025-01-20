// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "power.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(temp);

static const struct device *temp_chip_dev = DEVICE_DT_GET(DT_ALIAS(chip_temp));
static const struct device *temp_ext_dev = DEVICE_DT_GET(DT_ALIAS(ext_temp));
K_MUTEX_DEFINE(temp_mutex);

int read_ext_temp(void) {
    struct sensor_value temp;

    k_mutex_lock(&temp_mutex, K_FOREVER);
    pm_w1(PM_DEVICE_ACTION_RESUME);
    k_msleep(50);
    sensor_sample_fetch(temp_ext_dev);
    sensor_channel_get(temp_ext_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    pm_w1(PM_DEVICE_ACTION_SUSPEND);
    LOG_DBG("ext_temp: %d.%d C", temp.val1, temp.val2);
    k_mutex_unlock(&temp_mutex);

    // temperature in milli degrees C
    return temp.val1 * 1000 + temp.val2 / 1000;
}

int read_board_temp(void) {
    struct sensor_value temp;

    sensor_sample_fetch(temp_chip_dev);
    sensor_channel_get(temp_chip_dev, SENSOR_CHAN_DIE_TEMP, &temp);

    // temperature in milli degrees C
    return temp.val1 * 1000 + temp.val2 / 1000;
}
