/*
 * Copyright 2020 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_BATTERY_MAX17048_H_
#define ZEPHYR_DRIVERS_SENSOR_BATTERY_MAX17048_H_

/* Register addresses */
enum {
    VCELL = 0x02,
    SOC = 0x04,
    MODE = 0x06,
    VERSION = 0x08,
    HIBRT = 0x0a,
    CONFIG = 0x0c,
    VALRT = 0x14,
    CRATE = 0x16,
    VRESET_ID = 0x18,
    STATUS = 0x1a,
    TABLE = 0x40,
    CMD = 0xfe,
};

/* Masks */
enum {
    HIBRT_ON = 0xFFFF,
    HIBRT_OFF = 0x0000,
};

struct max17048_data {
    const struct device *i2c;
    /* Current cell voltage in units of 78.125 uV */
    uint16_t voltage;
    /* Remaining capacity in 1% / 256 */
    uint16_t state_of_charge;
    /* discharge rate in 0.208% / h */
    uint16_t c_rate;
};

struct max17048_config {
    char *bus_name;
};

#endif
