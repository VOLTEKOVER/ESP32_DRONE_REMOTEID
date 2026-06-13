#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#endif
#include "ble_tx.h"
#include "opendroneid.h"

#define TAG "BLE_TX"

#define RID_SERVICE_UUID 0xFFFA

static bool g_initialized = false;
#ifdef CONFIG_BT_BLUEDROID_ENABLED
static ODID_UAS_Data g_uas_data;
static uint8_t g_adv_data[64];

static ODID_Horizontal_accuracy_t ble_horiz_acc(uint8_t fix_type, uint8_t satellites)
{
    if (fix_type >= 4 && satellites >= 15) return ODID_HOR_ACC_1_METER;
    if (fix_type >= 4 && satellites >= 10) return ODID_HOR_ACC_3_METER;
    if (fix_type >= 3) return ODID_HOR_ACC_10_METER;
    return ODID_HOR_ACC_30_METER;
}

static ODID_Vertical_accuracy_t ble_vert_acc(uint8_t fix_type, uint8_t satellites)
{
    if (fix_type >= 4 && satellites >= 15) return ODID_VER_ACC_3_METER;
    if (fix_type >= 4 && satellites >= 10) return ODID_VER_ACC_10_METER;
    if (fix_type >= 3) return ODID_VER_ACC_25_METER;
    return ODID_VER_ACC_45_METER;
}

static void build_legacy_adv(rid_gps_data_t *gps, rid_identity_t *identity, uint8_t *buf, uint16_t *len)
{
    memset(buf, 0, 64);
    uint16_t idx = 0;

    buf[idx++] = 2;
    buf[idx++] = 0x01;
    buf[idx++] = 0x06;

    buf[idx++] = 3;
    buf[idx++] = 0x03;
    buf[idx++] = 0xFA;
    buf[idx++] = 0xFF;

    memset(&g_uas_data, 0, sizeof(g_uas_data));

    g_uas_data.BasicIDValid[0] = 1;
    g_uas_data.BasicID[0].IDType = (ODID_idtype_t)identity->id_type;
    g_uas_data.BasicID[0].UAType = (ODID_uatype_t)identity->ua_type;
    strncpy((char *)g_uas_data.BasicID[0].UASID, identity->uas_id, ODID_ID_SIZE);

    if (identity->uas_id_2[0] != '\0') {
        g_uas_data.BasicIDValid[1] = 1;
        g_uas_data.BasicID[1].IDType = (ODID_idtype_t)identity->id_type_2;
        g_uas_data.BasicID[1].UAType = (ODID_uatype_t)identity->ua_type_2;
        strncpy((char *)g_uas_data.BasicID[1].UASID, identity->uas_id_2, ODID_ID_SIZE);
    }

    g_uas_data.LocationValid = 1;
    g_uas_data.Location.Latitude = gps->latitude;
    g_uas_data.Location.Longitude = gps->longitude;
    g_uas_data.Location.AltitudeGeo = gps->altitude_msl;
    g_uas_data.Location.Height = gps->altitude_relative;
    g_uas_data.Location.SpeedHorizontal = gps->speed;
    g_uas_data.Location.SpeedVertical = gps->speed_vertical;
    g_uas_data.Location.Direction = gps->heading;
    g_uas_data.Location.HorizAccuracy = ble_horiz_acc(gps->fix_type, gps->satellites);
    g_uas_data.Location.VertAccuracy = ble_vert_acc(gps->fix_type, gps->satellites);

    g_uas_data.SystemValid = 1;
    g_uas_data.System.OperatorLatitude = gps->latitude;
    g_uas_data.System.OperatorLongitude = gps->longitude;

    g_uas_data.OperatorIDValid = 1;
    strncpy((char *)g_uas_data.OperatorID.OperatorId, identity->operator_id, ODID_ID_SIZE);

    uint8_t pack_buf[ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE + 8];
    int pack_len = odid_message_build_pack(&g_uas_data, pack_buf, sizeof(pack_buf));

    uint8_t adv_idx = idx;
    buf[idx++] = 0;
    buf[idx++] = 0x16;
    buf[idx++] = 0xFA;
    buf[idx++] = 0xFF;

    int copy_len = pack_len;
    if (copy_len > 60) copy_len = 60;
    memcpy(buf + idx, pack_buf, copy_len);
    idx += copy_len;
    buf[adv_idx] = idx - adv_idx - 1;

    *len = idx;
}
#endif

void ble_tx_init(void)
{
    if (g_initialized) return;
#ifdef CONFIG_BT_BLUEDROID_ENABLED
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) return;
    if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) return;
    if (esp_bluedroid_init() != ESP_OK) return;
    if (esp_bluedroid_enable() != ESP_OK) return;

    g_initialized = true;
    ESP_LOGI(TAG, "BLE initialized");
#else
    ESP_LOGW(TAG, "BLE not available on this target");
#endif
}

bool ble_tx_transmit_legacy(rid_gps_data_t *gps, rid_identity_t *identity)
{
    if (!g_initialized || !gps || !identity) return false;

#ifdef CONFIG_BT_BLUEDROID_ENABLED
    uint16_t len;
    build_legacy_adv(gps, identity, g_adv_data, &len);

    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x100,
        .adv_int_max = 0x100,
        .adv_type = ADV_TYPE_NONCONN_IND,
        .channel_map = ADV_CHNL_ALL,
        .own_addr_type = BLE_ADDR_TYPE_RANDOM,
        .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .peer_addr = {0},
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    esp_ble_gap_config_adv_data_raw(g_adv_data, len);
    esp_ble_gap_start_advertising(&adv_params);

    return true;
#else
    return false;
#endif
}

bool ble_tx_transmit_lr(rid_gps_data_t *gps, rid_identity_t *identity)
{
    if (!g_initialized || !gps || !identity) return false;

#if defined(CONFIG_BT_BLUEDROID_ENABLED) && defined(CONFIG_BT_BLE_50_EXTEND_ADV_EN)
    uint16_t len;
    build_legacy_adv(gps, identity, g_adv_data, &len);

    esp_ble_gap_ext_adv_params_t ext_params = {
        .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED,
        .interval_min = 0x100,
        .interval_max = 0x100,
        .channel_map = ADV_CHNL_ALL,
        .own_addr_type = BLE_ADDR_TYPE_RANDOM,
        .primary_phy = ESP_BLE_GAP_PHY_CODED,
        .secondary_phy = ESP_BLE_GAP_PHY_CODED,
        .scan_req_notif = false,
    };

    uint8_t inst = 0;
    esp_ble_gap_ext_adv_set_params(inst, &ext_params);
    esp_ble_gap_config_ext_adv_data_raw(inst, len, g_adv_data);

    esp_ble_gap_ext_adv_t ext_adv = {
        .instance = inst,
        .duration = 0,
        .max_events = 0,
    };
    esp_ble_gap_ext_adv_start(1, &ext_adv);

    return true;
#else
    return false;
#endif
}
