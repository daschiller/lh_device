// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <drivers/sensor.h>

static const struct device *temp_chip_dev = DEVICE_DT_GET(DT_ALIAS(chip_temp));
static const struct device *temp_ext_dev = DEVICE_DT_GET(DT_ALIAS(ext_temp));

int read_ext_temp(void) {
    struct sensor_value temp;

    sensor_sample_fetch(temp_ext_dev);
    sensor_channel_get(temp_ext_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

    // temperature in milli degrees C
    return temp.val1 * 1000 + temp.val2 / 1000;
}

int read_chip_temp(void) {
    struct sensor_value temp;

    sensor_sample_fetch(temp_chip_dev);
    sensor_channel_get(temp_chip_dev, SENSOR_CHAN_DIE_TEMP, &temp);

    // temperature in milli degrees C
    return temp.val1 * 1000 + temp.val2 / 1000;
}
