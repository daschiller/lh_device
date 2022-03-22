// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <drivers/pwm.h>
#include <logging/log.h>

// PWM
#define PWM_CTLR DT_ALIAS(pwm_led)
#define PWM_CHANNEL 0
#define PWM_FLAGS 0
#define PERIOD 1000 // in us

static const struct device *pwm_dev = DEVICE_DT_GET(PWM_CTLR);

void set_duty_cycle(uint8_t percent) {
    uint64_t pulse;

    if (percent > 0 && percent <= 100) {
        pulse = PERIOD * percent / 100.0;
    } else {
        pulse = 0;
    }
    pwm_pin_set_usec(pwm_dev, PWM_CHANNEL, PERIOD, pulse, PWM_FLAGS);
}
