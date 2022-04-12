// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <drivers/adc.h>

#define AIN1 2
#define RESOLUTION 12

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static const struct adc_channel_cfg adc_cfg = {.gain = ADC_GAIN_1_6,
                                               .reference = ADC_REF_INTERNAL,
                                               .acquisition_time =
                                                   ADC_ACQ_TIME_DEFAULT,
                                               .channel_id = 0,
                                               .differential = 0,
                                               .input_positive = AIN1};

int read_adc(void) {
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
                                   .resolution = RESOLUTION,
                                   .oversampling = 0,
                                   .calibrate = false};

    adc_channel_setup(adc_dev, &adc_cfg);
    adc_read(adc_dev, &adc_seq);
    adc_int = (int32_t)adc_value;

    return adc_raw_to_millivolts(adc_ref_internal(adc_dev), ADC_GAIN_1_6,
                                 RESOLUTION, &adc_int);
}
