// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Copyright (c) 2022 David Schiller <david.schiller@jku.at>
 */

#include "ble.h"
#include "pwm.h"
#include "temp.h"
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
#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
STATS_SECT_DECL(dev_stats) dev_stats;
#endif

LOG_MODULE_REGISTER(ble);

// BLE
#define DEVICE_NAME "lh_device"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define MIN_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MIN // 1 s
#define MAX_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MAX // 1.2 s
// #define MIN_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MIN * 9
// #define MAX_ADV_INTERVAL BT_GAP_ADV_SLOW_INT_MAX * 8

#define CPF_FORMAT_UINT8 0x04
#define CPF_FORMAT_UINT16 0x06
#define CPF_FORMAT_UINT32 0x08
#define CPF_FORMAT_SINT32 0x10
#define CPF_UNIT_PERCENT 0x27AD
#define CPF_UNIT_S 0x2703
#define CPF_UNIT_C 0x272F

// PWM
#define PWM_SERVICE_UUID 0xFFF0
#define PWM_DUTY_CHAR_UUID 0xFFF1
#define PWM_PULSE_CHAR_UUID 0xFFF2
#define PWM_TIMER_CHAR_UUID 0xFFF3

#define PWM_DUTY_LOWER 0
#define PWM_DUTY_UPPER 100
#define PWM_PULSE_LOWER 0
#define PWM_PULSE_UPPER PWM_PERIOD

static struct bt_uuid_16 pwm_service_uuid = BT_UUID_INIT_16(PWM_SERVICE_UUID);
static const struct bt_uuid_16 pwm_duty_char_uuid =
    BT_UUID_INIT_16(PWM_DUTY_CHAR_UUID);
static const struct bt_gatt_cpf pwm_duty_char_cpf = {.format = CPF_FORMAT_UINT8,
                                                     .unit = CPF_UNIT_PERCENT};
static const struct bt_uuid_16 pwm_pulse_char_uuid =
    BT_UUID_INIT_16(PWM_PULSE_CHAR_UUID);
static const struct bt_gatt_cpf pwm_pulse_char_cpf = {
    .format = CPF_FORMAT_UINT16, .exponent = -3, .unit = CPF_UNIT_S};
static const struct bt_uuid_16 pwm_timer_char_uuid =
    BT_UUID_INIT_16(PWM_TIMER_CHAR_UUID);
static const struct bt_gatt_cpf pwm_timer_char_cpf = {
    .format = CPF_FORMAT_UINT32, .unit = CPF_UNIT_S};
static struct pwm_params_t pwm_params = {.duty_percent = 0,
                                         .pulse_length = PWM_PULSE};
static void turn_off(struct k_timer *dummy) {
    pwm_params.duty_percent = 0;
    set_duty_pulse(&pwm_params);
};
static struct k_timer pwm_timer = {.expiry_fn = turn_off};

static ssize_t read_duty(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
    uint8_t value;

    value = pwm_params.duty_percent;
    LOG_DBG("attribute = 0x%04X", value);
    return bt_gatt_attr_read(conn, attr, buf, sizeof(value), 0, &value,
                             sizeof(value));
};
static ssize_t read_duty_valid_range(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr, void *buf,
                                     uint16_t len, uint16_t offset) {
    uint8_t values[2] = {PWM_DUTY_LOWER, PWM_DUTY_UPPER};

    return bt_gatt_attr_read(conn, attr, buf, sizeof(values), 0, &values,
                             sizeof(values));
};
static ssize_t read_pulse(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    uint16_t value;

    value = pwm_params.pulse_length;
    LOG_DBG("attribute = 0x%04X", value);
    return bt_gatt_attr_read(conn, attr, buf, sizeof(value), 0, &value,
                             sizeof(value));
};
static ssize_t read_pulse_valid_range(struct bt_conn *conn,
                                      const struct bt_gatt_attr *attr,
                                      void *buf, uint16_t len,
                                      uint16_t offset) {
    uint16_t values[2] = {PWM_PULSE_LOWER, PWM_PULSE_UPPER};

    return bt_gatt_attr_read(conn, attr, buf, sizeof(values), 0, &values,
                             sizeof(values));
};

static ssize_t write_duty(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          const void *buf, uint16_t len, uint16_t offset,
                          uint8_t flags) {
    uint16_t value;

    if (offset + len > sizeof(value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(&value + offset, buf, len);
    LOG_DBG("attribute = 0x%04X", value);
    pwm_params.duty_percent = value;
    set_duty_pulse(&pwm_params);

    return len;
};
static ssize_t write_pulse(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, const void *buf,
                           uint16_t len, uint16_t offset, uint8_t flags) {
    uint16_t value;

    if (offset + len > sizeof(value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(&value + offset, buf, len);
    LOG_DBG("attribute = 0x%04X", value);
    pwm_params.pulse_length = value;
    set_duty_pulse(&pwm_params);

    return len;
};
static ssize_t read_timer(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    uint32_t value;

    value = k_timer_remaining_get(&pwm_timer) / 1000; // in milliseconds
    LOG_DBG("attribute = 0x%04X", value);
    return bt_gatt_attr_read(conn, attr, buf, sizeof(value), 0, &value,
                             sizeof(value));
};
static ssize_t write_timer(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, const void *buf,
                           uint16_t len, uint16_t offset, uint8_t flags) {
    uint32_t value;

    if (offset + len > sizeof(value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(&value + offset, buf, len);
    k_timer_start(&pwm_timer, K_MSEC(value * 1000), K_NO_WAIT);
    LOG_DBG("attribute = 0x%04X", value);

    return len;
};

BT_GATT_SERVICE_DEFINE(
    pwm_service, BT_GATT_PRIMARY_SERVICE(&pwm_service_uuid),
    BT_GATT_CHARACTERISTIC(&pwm_duty_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_duty,
                           write_duty, NULL),
    BT_GATT_CUD("PWM duty cycle", BT_GATT_PERM_READ),
    BT_GATT_CPF(&pwm_duty_char_cpf),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ,
                       read_duty_valid_range, NULL, NULL),
    BT_GATT_CHARACTERISTIC(&pwm_pulse_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_pulse,
                           write_pulse, NULL),
    BT_GATT_CUD("PWM pulse length", BT_GATT_PERM_READ),
    BT_GATT_CPF(&pwm_pulse_char_cpf),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ,
                       read_pulse_valid_range, NULL, NULL),
    BT_GATT_CHARACTERISTIC(&pwm_timer_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_timer,
                           write_timer, NULL),
    BT_GATT_CUD("PWM timer", BT_GATT_PERM_READ),
    BT_GATT_CPF(&pwm_timer_char_cpf), );

// TEMP
#define TEMP_SERVICE_UUID 0xFFE0
#define TEMP_EXT_CHAR_UUID 0xFFE1

static struct bt_uuid_16 temp_service_uuid = BT_UUID_INIT_16(TEMP_SERVICE_UUID);
static const struct bt_uuid_16 temp_ext_char_uuid =
    BT_UUID_INIT_16(TEMP_EXT_CHAR_UUID);
static const struct bt_gatt_cpf temp_ext_char_cpf = {
    .format = CPF_FORMAT_SINT32, .exponent = -3, .unit = CPF_UNIT_C};

static ssize_t read_temp(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
    int32_t value;

    value = read_ext_temp();
    LOG_DBG("attribute = 0x%04X", value);
    return bt_gatt_attr_read(conn, attr, buf, sizeof(value), 0, &value,
                             sizeof(value));
};

BT_GATT_SERVICE_DEFINE(
    temp_service, BT_GATT_PRIMARY_SERVICE(&temp_service_uuid),
    BT_GATT_CHARACTERISTIC(&temp_ext_char_uuid.uuid, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_temp, NULL, NULL),
    BT_GATT_CUD("Probe temperature", BT_GATT_PERM_READ),
    BT_GATT_CPF(&temp_ext_char_cpf), );

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    // "Generic Light Source" appearance
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0xC0, 0x07),
};
static const struct bt_data sd[] = {
    // PWM service
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(PWM_SERVICE_UUID)),
    // OTA service
    BT_DATA_BYTES(
        BT_DATA_UUID128_ALL,
        BT_UUID_128_ENCODE(0x8d53dc1d, 0x1db7, 0x4cd3, 0x868b, 0x8a527460aa84)),
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
    stat_mgmt_register_group();
    err = smp_bt_register();
    if (err) {
        LOG_ERR("BT SMP failed to register (err %d)", err);
        return err;
    }
    STATS_INIT_AND_REG(dev_stats, STATS_SIZE_32, "dev_stats");

    setup_pwm();

    return 0;
}
