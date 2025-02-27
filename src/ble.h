// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "pwm.h"
#include <stdint.h>

#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
#include "stat_mgmt/stat_mgmt.h"
#include <zephyr/stats/stats.h>
STATS_SECT_START(dev_stats)
STATS_SECT_ENTRY(uptime_s)
STATS_SECT_ENTRY(reboots)
STATS_SECT_ENTRY(reset_reason)
STATS_SECT_END;

STATS_NAME_START(dev_stats)
STATS_NAME(dev_stats, uptime_s)
STATS_NAME(dev_stats, reboots)
STATS_NAME(dev_stats, reset_reason)
STATS_NAME_END(dev_stats);
#endif

int setup_ble(void);
void update_adv(void);
struct pwm_repetitions_t {
    unsigned int repetitions;
    uint8_t duty_percent;
    uint32_t timer_duration;
};
