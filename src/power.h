// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <pm/device.h>

int pm_console(enum pm_device_action action);
int pm_fuel_gauge(enum pm_device_action action);
int pm_w1(enum pm_device_action action);
