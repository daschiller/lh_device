// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(max17048, CONFIG_SENSOR_LOG_LEVEL);

#include "max17048.h"

#define DT_DRV_COMPAT maxim_max17048

/**
 * @brief Read a register value
 *
 * Registers have an address and a 16-bit value
 *
 * @param priv Private data for the driver
 * @param reg_addr Register address to read
 * @param val Place to put the value on success
 * @return 0 if successful, or negative error code from I2C API
 */
static int max17048_reg_read(const struct device *dev, int reg_addr,
                             int16_t *valp) {
    const struct max17048_config *config = dev->config;
    uint8_t i2c_data[2];
    int rc;

    rc = i2c_burst_read_dt(&config->i2c, reg_addr, i2c_data, 2);
    if (rc < 0) {
        LOG_ERR("Unable to read register");
        return rc;
    }
    *valp = (i2c_data[0] << 8) | i2c_data[1];

    return 0;
}

static int max17048_reg_write(const struct device *dev, int reg_addr,
                              uint16_t val) {
    const struct max17048_config *config = dev->config;
    uint8_t buf[3];

    buf[0] = (uint8_t)reg_addr;
    sys_put_le16(val, &buf[1]);

    return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

/**
 * @brief sensor value get
 *
 * @param dev MAX17048 device to access
 * @param chan Channel number to read
 * @param valp Returns the sensor value read on success
 * @return 0 if successful
 * @return -ENOTSUP for unsupported channels
 */
static int max17048_channel_get(const struct device *dev,
                                enum sensor_channel chan,
                                struct sensor_value *valp) {
    struct max17048_data *const priv = dev->data;
    uint32_t tmp;

    switch (chan) {
    case SENSOR_CHAN_GAUGE_VOLTAGE:
        /* Get voltage in uV */
        tmp = priv->voltage * 625 / 8;
        valp->val1 = tmp / 1000000;
        valp->val2 = tmp % 1000000;
        break;
    case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
        valp->val1 = priv->state_of_charge / 256;
        valp->val2 = priv->state_of_charge % 256 * 1000000 / 256;
        break;
    case SENSOR_CHAN_GAUGE_TIME_TO_EMPTY:
        /* C-rate is in 0.208%/h */
        tmp = priv->c_rate;
        if (tmp > 0) {
            valp->val1 = (priv->state_of_charge / 256) / (tmp * 26 / 125) * 60;
            valp->val2 = 0; // TODO: implement fractional part
        } else {
            /* C-rate is too low */
            valp->val1 = 0;
            valp->val2 = 0;
        }
        break;
    default:
        return -ENOTSUP;
    }

    return 0;
}

static int max17048_sample_fetch(const struct device *dev,
                                 enum sensor_channel chan) {
    struct max17048_data *priv = dev->data;

    struct {
        int reg_addr;
        int16_t *dest;
    } regs[] = {
        {VCELL, &priv->voltage},
        {SOC, &priv->state_of_charge},
        {CRATE, &priv->c_rate},
    };

    __ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

#ifdef CONFIG_PM_DEVICE
    enum pm_device_state state;
    (void)pm_device_state_get(dev, &state);
    /* Do not allow sample fetching from off state */
    if (state == PM_DEVICE_STATE_OFF)
        return -EIO;
#endif

    for (size_t i = 0; i < ARRAY_SIZE(regs); i++) {
        int rc;

        rc = max17048_reg_read(dev, regs[i].reg_addr, regs[i].dest);
        if (rc != 0) {
            LOG_ERR("Failed to read channel %d", chan);
            return rc;
        }
    }

    return 0;
}

#ifdef CONFIG_PM
static int max17048_pm_action(const struct device *dev,
                              enum pm_device_action action) {
    int ret;
    uint16_t tmp;

    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        ret = max17048_reg_write(dev, HIBRT, HIBRT_OFF);
        break;

    case PM_DEVICE_ACTION_SUSPEND:
        ret = max17048_reg_write(dev, HIBRT, HIBRT_ON);
        break;

    case PM_DEVICE_ACTION_TURN_OFF:
        ret = max17048_reg_read(dev, CONFIG, &tmp);
        if (!ret) {
            tmp |= SLEEP;
            ret = max17048_reg_write(dev, CONFIG, tmp);
        }
        break;

    default:
        ret = -ENOTSUP;
    }

    return ret;
}
#endif

/**
 * @brief initialise the fuel gauge
 *
 * @return 0 for success
 * @return -EIO on I2C communication error
 * @return -EINVAL if the I2C controller could not be found
 */
static int max17048_gauge_init(const struct device *dev) {
    const struct max17048_config *const config = dev->config;
    int ret = 0;
    uint16_t tmp;

    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("Bus device is not ready");
        return -ENODEV;
    }

    if (max17048_reg_read(dev, STATUS, &tmp)) {
        return -EIO;
    }

    if (config->enable_sleep) {
        /* returns 0 or -EIO */
        ret = max17048_reg_write(dev, MODE, ENSLEEP);
    }

    return ret;
}

static const struct sensor_driver_api max17048_battery_driver_api = {
    .sample_fetch = max17048_sample_fetch,
    .channel_get = max17048_channel_get,
};

#define MAX17048_INIT(index)                                                   \
    static struct max17048_data max17048_driver_##index;                       \
                                                                               \
    static const struct max17048_config max17048_config_##index = {            \
        .i2c = I2C_DT_SPEC_INST_GET(index),                                    \
        .enable_sleep = DT_INST_PROP(index, enable_sleep),                     \
    };                                                                         \
                                                                               \
    PM_DEVICE_DT_INST_DEFINE(index, max17048_pm_action);                       \
                                                                               \
    /* NOTE: replace PM_DEVICE_DT_INST_REF with PM_DEVICE_DT_INST_GET in the   \
     * future */                                                               \
    DEVICE_DT_INST_DEFINE(                                                     \
        index, &max17048_gauge_init, PM_DEVICE_DT_INST_GET(index),             \
        &max17048_driver_##index, &max17048_config_##index, POST_KERNEL,       \
        CONFIG_SENSOR_INIT_PRIORITY, &max17048_battery_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAX17048_INIT)
