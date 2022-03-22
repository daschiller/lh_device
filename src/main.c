// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "ble.h"
#include <drivers/adc.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <pm/device.h>

LOG_MODULE_REGISTER(main);

// #define LOW_POWER

void main(void) {
    int err;

    printk("build time: " __DATE__ " " __TIME__ "\n");
    printk("Starting LionHearted device ...\n");

    setup_ble();

    const struct device *temp_dev =
        device_get_binding(DT_LABEL(DT_ALIAS(chip_temp)));
    struct sensor_value temp;
    sensor_sample_fetch(temp_dev);
    sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &temp);
    printk("Temperature: %f\n", sensor_value_to_double(&temp));

    const struct device *temp_ext_dev = device_get_binding("DS18B20");
    struct sensor_value temp_ext;
    sensor_sample_fetch(temp_ext_dev);
    sensor_channel_get(temp_ext_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_ext);
    printk("Temperature (probe): %f\n", sensor_value_to_double(&temp_ext));

#define AIN1 2
    const struct device *adc_dev =
        device_get_binding(DT_LABEL(DT_NODELABEL(adc)));
    struct adc_channel_cfg adc_cfg = {.gain = ADC_GAIN_1_6,
                                      .reference = ADC_REF_INTERNAL,
                                      .acquisition_time = ADC_ACQ_TIME_DEFAULT,
                                      .channel_id = 0,
                                      .differential = 0,
                                      .input_positive = AIN1};
    adc_channel_setup(adc_dev, &adc_cfg);
    int16_t adc_value = 0;
    int32_t adc_int;
    struct adc_sequence_options adc_seq_opts = {.interval_us = 0,
                                                .callback = NULL,
                                                .user_data = NULL,
                                                .extra_samplings = 0};
    struct adc_sequence adc_seq = {.options = &adc_seq_opts,
                                   .channels = BIT(0),
                                   .buffer = &adc_value,
                                   .buffer_size = sizeof(adc_value),
                                   .resolution = 12,
                                   .oversampling = 0,
                                   .calibrate = false};
    adc_read(adc_dev, &adc_seq);
    adc_int = (int32_t)adc_value;
    adc_raw_to_millivolts(adc_ref_internal(adc_dev), ADC_GAIN_1_6, 12,
                          &adc_int);
    printk("ADC: %d\n", adc_value);

    // printk("%X\n", NRF_RADIO->TXPOWER);

// suspend devices
#ifdef LOW_POWER
    // err = pm_device_action_run(adc_dev, PM_DEVICE_ACTION_SUSPEND);
    // printk("%d\n", err);
    // err = pm_device_action_run(temp_ext_dev, PM_DEVICE_ACTION_SUSPEND);
    // printk("%d\n", err);
    const struct device *uart1_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    err = pm_device_action_run(uart1_dev, PM_DEVICE_ACTION_SUSPEND);
    printk("%d\n", err);

    const struct device *uart0_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    err = pm_device_action_run(uart0_dev, PM_DEVICE_ACTION_SUSPEND);
    if (err) {
        LOG_ERR("UART suspension failed (err %d)", err);
    }
#endif
    // printk("new image!\n");

    k_sleep(K_FOREVER);
}
