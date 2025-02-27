// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "pwm.h"
#include "power.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm);

#include <nrfx_pwm.h>

#define PWM_PIN 1

static const nrfx_pwm_t pwm_instance = NRFX_PWM_INSTANCE(0);
static const nrfx_pwm_config_t pwm_config =
    NRFX_PWM_DEFAULT_CONFIG(PWM_PIN, NRFX_PWM_PIN_NOT_USED,
                            NRFX_PWM_PIN_NOT_USED, NRFX_PWM_PIN_NOT_USED);
static nrf_pwm_values_common_t pwm_duty_cycle = PWM_PERIOD;
// The second sequence needs to have more than one element due to erratum 183.
// More info here:
// https://devzone.nordicsemi.com/f/nordic-q-a/27891/pwm-finish-flag/110046#110046
static nrf_pwm_values_common_t pwm_off[2] = {PWM_PERIOD, PWM_PERIOD};
static nrf_pwm_sequence_t pwm_seq_0 = {.values.p_common = &pwm_duty_cycle,
                                       .length = sizeof(pwm_duty_cycle) /
                                                 sizeof(uint16_t),
                                       .repeats = PWM_PULSE,
                                       .end_delay = 0};
static nrf_pwm_sequence_t pwm_seq_1 = {.values.p_common = pwm_off,
                                       .length = ARRAY_SIZE(pwm_off),
                                       .repeats = (PWM_PERIOD - PWM_PULSE) / 2,
                                       .end_delay = 0};

int setup_pwm(void) {
    nrfx_err_t err;

    err = nrfx_pwm_init(&pwm_instance, &pwm_config, NULL, NULL);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("init failed (err %d)", err);
    }

    return err;
}

void set_duty_pulse(struct pwm_params_t *params) {
    if (params->duty_percent > 0 && params->duty_percent <= 100) {
        pwm_duty_cycle = PWM_PERIOD * (1 - params->duty_percent / 100.0);
        // wake fuel gauge from hibernation for more accurate discharge tracking
        pm_fuel_gauge(PM_DEVICE_ACTION_RESUME);
    } else if (params->duty_percent == 0) {
        nrfx_pwm_stop(&pwm_instance, true);
        // hibernate fuel gauge
        pm_fuel_gauge(PM_DEVICE_ACTION_SUSPEND);
        return;
    } else {
        // set to default
        params->duty_percent = 0;
        pwm_duty_cycle = PWM_PERIOD;
        // hibernate fuel gauge
        pm_fuel_gauge(PM_DEVICE_ACTION_SUSPEND);
    }

    if (params->pulse_length > 0 && params->pulse_length <= PWM_PERIOD) {
        pwm_seq_0.repeats = params->pulse_length;
        pwm_seq_1.repeats = (PWM_PERIOD - params->pulse_length) / 2;
    } else {
        // set to default
        params->pulse_length = PWM_PULSE;
        pwm_seq_0.repeats = PWM_PULSE;
        pwm_seq_1.repeats = (PWM_PERIOD - PWM_PULSE) / 2;
    }

    LOG_DBG("duty_cycle = %u", pwm_duty_cycle);
    LOG_DBG("pulse_length = %u", pwm_seq_0.repeats);
    nrfx_pwm_complex_playback(&pwm_instance, &pwm_seq_0, &pwm_seq_1, 1,
                              NRFX_PWM_FLAG_LOOP);
}
