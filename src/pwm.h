// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <stdint.h>

// defaults
#define PWM_PERIOD 1000
#define PWM_PULSE 1000

struct pwm_params_t {
    uint8_t duty_percent;
    uint16_t pulse_length;
};

int setup_pwm(void);
void set_duty_pulse(struct pwm_params_t *params);
