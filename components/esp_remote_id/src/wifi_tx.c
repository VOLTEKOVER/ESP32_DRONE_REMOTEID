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

void wifi_tx_init(void)
{
    if (g_initialized) return;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(ret));
    }

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
            .max_connection = 0,
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
    g_uas_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    g_uas_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    strncpy((char *)g_uas_data.BasicID[0].UASID, identity->uas_id, ODID_ID_SIZE);

    g_uas_data.LocationValid = 1;
    g_uas_data.Location.Latitude = gps->latitude;
    g_uas_data.Location.Longitude = gps->longitude;
    g_uas_data.Location.AltitudeGeo = gps->altitude_msl;
    g_uas_data.Location.Height = gps->altitude_relative;
    g_uas_data.Location.SpeedHorizontal = gps->speed;
    g_uas_data.Location.Direction = gps->heading;
    g_uas_data.Location.SpeedVertical = 0;
    g_uas_data.Location.HorizAccuracy = ODID_HOR_ACC_30_METER;
    g_uas_data.Location.VertAccuracy = ODID_VER_ACC_45_METER;

    g_uas_data.SystemValid = 1;
    g_uas_data.System.OperatorLatitude = gps->latitude;
    g_uas_data.System.OperatorLongitude = gps->longitude;
    g_uas_data.System.AreaCount = 0;
    g_uas_data.System.AreaRadius = 0;

    g_uas_data.OperatorIDValid = 1;
    strncpy((char *)g_uas_data.OperatorID.OperatorId, identity->operator_id, ODID_ID_SIZE);

    uint8_t buffer[1024];
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
