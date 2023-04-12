// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "power.h"
#include <zephyr/drivers/sensor.h>

static const struct device *fuel_dev = DEVICE_DT_GET(DT_ALIAS(fuel_gauge));

int read_soc(void) {
    struct sensor_value soc;

    sensor_sample_fetch(fuel_dev);
    sensor_channel_get(fuel_dev, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &soc);

    return soc.val1;
}

int read_voltage(void) {
    struct sensor_value voltage;

    sensor_sample_fetch(fuel_dev);
    sensor_channel_get(fuel_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage);

    // voltage in millivolts
    return voltage.val1 * 1000 + voltage.val2 / 1000;
}
