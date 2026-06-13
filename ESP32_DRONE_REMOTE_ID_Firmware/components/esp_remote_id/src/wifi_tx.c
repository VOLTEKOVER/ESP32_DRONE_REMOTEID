#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "wifi_tx.h"
#include "opendroneid.h"
#include "odid_wifi.h"
#include "esp_remote_id.h"

#define TAG "WIFI_TX"

static bool g_initialized = false;
static uint8_t g_random_mac[6];
static ODID_UAS_Data g_uas_data;


static void generate_random_mac(uint8_t mac[6])
{
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)(esp_random() & 0xFF);
    }
    mac[0] |= 0x02;
    mac[0] &= 0xFE;
}

static ODID_Horizontal_accuracy_t horiz_acc_from_gps(uint8_t fix_type, uint8_t satellites)
{
    if (fix_type >= 4 && satellites >= 15) return ODID_HOR_ACC_1_METER;
    if (fix_type >= 4 && satellites >= 10) return ODID_HOR_ACC_3_METER;
    if (fix_type >= 4) return ODID_HOR_ACC_10_METER;
    if (fix_type >= 3) return ODID_HOR_ACC_10_METER;
    return ODID_HOR_ACC_30_METER;
}

static ODID_Vertical_accuracy_t vert_acc_from_gps(uint8_t fix_type, uint8_t satellites)
{
    if (fix_type >= 4 && satellites >= 15) return ODID_VER_ACC_3_METER;
    if (fix_type >= 4 && satellites >= 10) return ODID_VER_ACC_10_METER;
    if (fix_type >= 4) return ODID_VER_ACC_25_METER;
    if (fix_type >= 3) return ODID_VER_ACC_25_METER;
    return ODID_VER_ACC_45_METER;
}

void wifi_tx_init(void)
{
    if (g_initialized) return;

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(ret));
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    generate_random_mac(g_random_mac);
    esp_base_mac_addr_set(g_random_mac);

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-RID",
            .ssid_len = 7,
            .channel = 6,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
            .beacon_interval = 100,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20);
    esp_wifi_set_max_tx_power(80);

    esp_wifi_set_promiscuous(true);

    g_initialized = true;
    ESP_LOGI(TAG, "WiFi TX initialized, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             g_random_mac[0], g_random_mac[1], g_random_mac[2],
             g_random_mac[3], g_random_mac[4], g_random_mac[5]);
}

bool wifi_tx_transmit(rid_gps_data_t *gps, rid_identity_t *identity)
{
    if (!g_initialized || !gps || !identity) return false;

    memset(&g_uas_data, 0, sizeof(ODID_UAS_Data));

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
    g_uas_data.Location.Direction = gps->heading;
    g_uas_data.Location.SpeedVertical = gps->speed_vertical;
    g_uas_data.Location.HorizAccuracy = horiz_acc_from_gps(gps->fix_type, gps->satellites);
    g_uas_data.Location.VertAccuracy = vert_acc_from_gps(gps->fix_type, gps->satellites);

    g_uas_data.SystemValid = 1;
    g_uas_data.System.OperatorLatitude = gps->latitude;
    g_uas_data.System.OperatorLongitude = gps->longitude;
    g_uas_data.System.AreaCount = 0;
    g_uas_data.System.AreaRadius = 0;

    g_uas_data.OperatorIDValid = 1;
    strncpy((char *)g_uas_data.OperatorID.OperatorId, identity->operator_id, ODID_ID_SIZE);

    static uint8_t buffer[1024];
    int length = odid_wifi_build_message_pack_beacon_frame(
        &g_uas_data, (char *)g_random_mac,
        "ESP-RID", 7, 100, 0,
        buffer, sizeof(buffer));

    if (length > 0) {
        esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_AP, buffer, length, true);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(ret));
            return false;
        }
        return true;
    }

    return false;
}
