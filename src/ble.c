// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "pwm.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/uuid.h>
#include <logging/log.h>
#include <settings/settings.h>

// OTA
#ifdef CONFIG_MCUMGR_SMP_BT
#include <mgmt/mcumgr/smp_bt.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include "os_mgmt/os_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif

LOG_MODULE_REGISTER(ble);

// BLE
#define DEVICE_NAME "lh_device"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define MIN_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MIN // 1 s
#define MAX_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MAX // 1.2 s
// #define MIN_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MIN * 9
// #define MAX_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MAX * 8

#define SERVICE_UUID 0xFFF0
#define CHARACTERISTIC_UUID 0xFFF1

static struct bt_uuid_16 service_uuid = BT_UUID_INIT_16(SERVICE_UUID);
static struct bt_uuid_16 characteristic_uuid =
    BT_UUID_INIT_16(CHARACTERISTIC_UUID);
static uint8_t custom_value;

static ssize_t read_custom(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset) {

    const uint8_t *value = attr->user_data;

    LOG_DBG("attribute = 0x%02X", *value);
    return bt_gatt_attr_read(conn, attr, buf, sizeof(*value), 0, value,
                             sizeof(*value));
};
static ssize_t write_custom(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr, const void *buf,
                            uint16_t len, uint16_t offset, uint8_t flags) {

    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(*value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(value + offset, buf, len);
    LOG_DBG("attribute = 0x%02X", *value);
    set_duty_cycle(*value);

    return len;
};

BT_GATT_SERVICE_DEFINE(
    custom_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),
    BT_GATT_CHARACTERISTIC(&characteristic_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_custom,
                           write_custom, &custom_value), );

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};
static const struct bt_data sd[] = {
    // PWM service
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(SERVICE_UUID)),
    // OTA service
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b,
                  0x86, 0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};
static const struct bt_le_adv_param adv_params = BT_LE_ADV_PARAM_INIT(
    BT_LE_ADV_OPT_CONNECTABLE, MIN_ADV_INTERVAL, MAX_ADV_INTERVAL, NULL);

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    LOG_ERR("Disconnected (reason %u)", reason);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

int setup_ble(void) {
    int err;

    bt_conn_cb_register(&conn_callbacks);
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        err = settings_load();
        if (err) {
            LOG_ERR("Failed to load settings (err %d)", err);
            return err;
        }
    }

    err = bt_le_adv_start(&adv_params, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }
    LOG_INF("Advertising successfully started");

    os_mgmt_register_group();
    img_mgmt_register_group();
    err = smp_bt_register();
    if (err) {
        LOG_ERR("BT SMP failed to register (err %d)", err);
        return err;
    }

    return 0;
}
