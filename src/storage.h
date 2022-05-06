// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#define REBOOT_ID 1

int setup_storage(void);
int write_storage_i32(uint16_t id, uint32_t data);
int read_storage_i32(uint16_t id);
