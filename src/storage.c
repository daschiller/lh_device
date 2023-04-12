// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(storage);

static struct nvs_fs fs;

int setup_storage(void) {
    int err;
    struct flash_pages_info info;

    fs.flash_device = FLASH_AREA_DEVICE(storage);
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Flash device %s is not ready\n", fs.flash_device->name);
        return -EIO;
    }
    fs.offset = FLASH_AREA_OFFSET(storage);
    err = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (err) {
        LOG_ERR("Unable to get page info\n");
        return err;
    }
    fs.sector_size = info.size;
    fs.sector_count = 2U; // 8096 bytes on nRF52840

    err = nvs_mount(&fs);
    if (err) {
        LOG_ERR("Flash Init failed\n");
        return err;
    }

    return 0;
}

int write_storage_i32(uint16_t id, int32_t data) {
    int err;

    err = nvs_write(&fs, id, &data, sizeof(data));
    if (err > 0) {
        return 0;
    } else {
        return err;
    }
}

int read_storage_i32(uint16_t id) {
    int err;
    int32_t data;

    err = nvs_read(&fs, id, &data, sizeof(data));
    if (err == sizeof(data)) {
        return (int)data;
    } else {
        return err;
    }
}
