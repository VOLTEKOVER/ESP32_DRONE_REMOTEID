#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_remote_id.h"
#include "protocol_detect.h"
#include "nmea_parser.h"
#include "msp_parser.h"
#include "mavlink_parser.h"
#include "wifi_tx.h"
#include "ble_tx.h"
#include "esp_netif.h"
#include "web_config.h"
#include "nvs_storage.h"

#define TAG "ESP_RID"

#define C_BLU  "\x1b[34m"
#define C_GRN  "\x1b[32m"
#define C_AMB  "\x1b[33m"
#define C_RED  "\x1b[31m"
#define C_RST  "\x1b[0m"

static rid_config_t g_config;
static rid_state_t g_state;
static bool g_running = false;

static void default_config(rid_config_t *cfg)
{
    memset(cfg, 0, sizeof(rid_config_t));

    cfg->protocol = RID_PROTOCOL_AUTO;
    cfg->uart_port = 1;
    cfg->baud_rate = 57600;
    cfg->tx_pin = 17;
    cfg->rx_pin = 18;

    cfg->ua_type = 1;
    cfg->id_type = 1;
    snprintf(cfg->uas_id, sizeof(cfg->uas_id), "%s", "ESP32-RID-001");
    snprintf(cfg->operator_id, sizeof(cfg->operator_id), "%s", "OP-UNKNOWN");

    cfg->ua_type_2 = 0;
    cfg->id_type_2 = 0;
    cfg->uas_id_2[0] = '\0';

    cfg->tx_modes = RID_TRANSMIT_WIFI_BCN;
    cfg->wifi_channel = 6;
    cfg->wifi_power_dbm = 20.0f;
    cfg->wifi_bcn_rate_hz = 1.0f;
    cfg->wifi_nan_rate_hz = 0.0f;
    cfg->ble4_rate_hz = 1.0f;
    cfg->ble4_power_dbm = 18.0f;
    cfg->ble5_rate_hz = 1.0f;
    cfg->ble5_power_dbm = 18.0f;

    cfg->wifi_ssid[0] = '\0';
    snprintf(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), "%s", "ESP-RID");
    cfg->wifi_password[0] = '\0';
    cfg->webserver_en = 1;

    cfg->mavlink_sysid = 0;
    cfg->bcast_powerup = 1;

    cfg->options = 0;
    cfg->lock_level = 0;

    for (int i = 0; i < ESP_RID_NUM_KEYS; i++)
        cfg->public_keys[i][0] = '\0';
}

void esp_rid_init(void)
{
    ESP_LOGI(TAG, "ESP DRONE REMOTEID v%s initializing", ESP_RID_VERSION);

    default_config(&g_config);
    memset(&g_state, 0, sizeof(rid_state_t));

    nvs_storage_init();
    nvs_storage_load(&g_config);

    protocol_detect_init();
    nmea_parser_init();
    msp_parser_init();
    mavlink_parser_init();
    mavlink_parser_set_sysid_filter(g_config.mavlink_sysid);

    esp_netif_init();

    wifi_tx_init();
    ble_tx_init();

    web_config_init();

    ESP_LOGI(TAG, "\xE2\x9C\x93 Remote ID initialized");
}

void esp_rid_set_config(const rid_config_t *config)
{
    uint32_t old_baud = g_config.baud_rate;
    memcpy(&g_config, config, sizeof(rid_config_t));
    nvs_storage_save(&g_config);
    if (g_config.baud_rate != old_baud && g_config.baud_rate > 0) {
        protocol_detect_reinit(g_config.baud_rate);
    }
    mavlink_parser_set_sysid_filter(g_config.mavlink_sysid);
}

void esp_rid_get_config(rid_config_t *config)
{
    memcpy(config, &g_config, sizeof(rid_config_t));
}

void esp_rid_get_state(rid_state_t *state)
{
    memcpy(state, &g_state, sizeof(rid_state_t));
}

void esp_rid_factory_reset(void)
{
    nvs_storage_erase();
    default_config(&g_config);
    nvs_storage_save(&g_config);
}

static void update_transmissions(void)
{
    if (!g_state.gps_valid && !g_config.bcast_powerup) return;

    if (g_state.active_protocol == RID_PROTOCOL_UNKNOWN) return;

    if (g_config.tx_modes & RID_TRANSMIT_WIFI_BCN) {
        wifi_tx_transmit(&g_state.gps, &g_state.identity);
        g_state.wifi_bcn_count++;
        g_state.transmissions_count++;
    }

    if (g_config.tx_modes & RID_TRANSMIT_BLE4) {
        ble_tx_transmit_legacy(&g_state.gps, &g_state.identity);
        g_state.ble4_count++;
        g_state.transmissions_count++;
    }

    if (g_config.tx_modes & RID_TRANSMIT_BLE5) {
        ble_tx_transmit_lr(&g_state.gps, &g_state.identity);
        g_state.ble5_count++;
        g_state.transmissions_count++;
    }
}

static const char *proto_name(rid_protocol_t p)
{
    switch (p) {
        case RID_PROTOCOL_UNKNOWN: return "UNKNOWN";
        case RID_PROTOCOL_MAVLINK: return "MAVLink";
        case RID_PROTOCOL_MSP:     return "MSP";
        case RID_PROTOCOL_NMEA:   return "NMEA";
        case RID_PROTOCOL_NONE:   return "NONE";
        case RID_PROTOCOL_AUTO:   return "AUTO";
        default: return "?";
    }
}

static void print_status_box(void)
{
    char lat_str[16], lon_str[16];
    if (g_state.gps_valid)
        snprintf(lat_str, sizeof(lat_str), "%.4f", g_state.gps.latitude);
    else
        snprintf(lat_str, sizeof(lat_str), "--");
    if (g_state.gps_valid)
        snprintf(lon_str, sizeof(lon_str), "%.4f", g_state.gps.longitude);
    else
        snprintf(lon_str, sizeof(lon_str), "--");

    const char *gps_str = g_state.gps_valid ? "YES" : "NO";
    const char *proto = proto_name(g_state.active_protocol);

    printf(C_GRN);
    printf("\n");
    printf("  \xE2\x94\x8C\xE2\x94\x80 ESP Drone Remote ID \xE2\x94\x80 status \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n");
    printf("  \xE2\x94\x82  Protocol  : %-33s \xE2\x94\x82\n", proto);
    printf("  \xE2\x94\x82  GPS Fix   : %-3s           (%d/%d)       \xE2\x94\x82\n",
           gps_str, g_state.gps.fix_type, g_state.gps.satellites);
    printf("  \xE2\x94\x82  Lat / Lon : %s / %-12s       \xE2\x94\x82\n", lat_str, lon_str);
    printf("  \xE2\x94\x82  TX count  : %-35lu \xE2\x94\x82\n",
           (unsigned long)g_state.transmissions_count);
    printf("  \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98\n");
    printf(C_RST);
}

static void print_system_box(void)
{
    int64_t us = esp_timer_get_time();
    uint32_t sec = (uint32_t)(us / 1000000);
    uint32_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
    uint32_t heap_free = esp_get_free_heap_size();
    uint32_t heap_total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    printf(C_BLU);
    printf("\n");
    printf("  \xE2\x94\x8C\xE2\x94\x80 System Info \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n");
    printf("  \xE2\x94\x82  Heap free : %-5lu KB / %-5lu KB                        \xE2\x94\x82\n",
           (unsigned long)(heap_free / 1024), (unsigned long)(heap_total / 1024));
    printf("  \xE2\x94\x82  Uptime    : %02lu:%02lu:%02lu                                      \xE2\x94\x82\n",
           (unsigned long)h, (unsigned long)m, (unsigned long)s);
    printf("  \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98\n");
    printf(C_RST);
}

static void rid_task(void *arg)
{
    g_state.active_protocol = RID_PROTOCOL_UNKNOWN;
    uint32_t log_cycle = 0;

    while (g_running) {
        rid_gps_data_t gps_data;
        rid_protocol_t proto = RID_PROTOCOL_UNKNOWN;

        memset(&gps_data, 0, sizeof(rid_gps_data_t));

        if (g_config.protocol == RID_PROTOCOL_AUTO)
            proto = protocol_detect_auto();
        else
            proto = g_config.protocol;

        g_state.active_protocol = proto;

        switch (proto) {
        case RID_PROTOCOL_NMEA:
            nmea_parser_get(&gps_data);
            break;
        case RID_PROTOCOL_MSP:
            msp_parser_get(&gps_data);
            break;
        case RID_PROTOCOL_MAVLINK:
            mavlink_parser_get(&gps_data);
            break;
        default:
            break;
        }

        if (gps_data.fix_type >= 2 && gps_data.latitude != 0.0) {
            bool force_tx = (g_config.options & RID_OPT_FORCE_ARM_OK) && gps_data.armed;

            if (force_tx || gps_data.fix_type >= 2) {
                memcpy(&g_state.gps, &gps_data, sizeof(rid_gps_data_t));
                g_state.gps_valid = true;
                g_state.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

                snprintf(g_state.identity.uas_id, sizeof(g_state.identity.uas_id), "%s", g_config.uas_id);
                snprintf(g_state.identity.operator_id, sizeof(g_state.identity.operator_id), "%s", g_config.operator_id);
                g_state.identity.id_type = g_config.id_type;
                g_state.identity.ua_type = g_config.ua_type;
                snprintf(g_state.identity.uas_id_2, sizeof(g_state.identity.uas_id_2), "%s", g_config.uas_id_2);
                g_state.identity.id_type_2 = g_config.id_type_2;
                g_state.identity.ua_type_2 = g_config.ua_type_2;

                if (g_config.options & RID_OPT_DONT_SAVE_BASIC_ID) {
                    g_state.identity.uas_id[0] = '\0';
                    g_state.identity.uas_id_2[0] = '\0';
                }

                update_transmissions();
            }
        }

        if (g_config.options & RID_OPT_PRINT_RID_MAVLINK) {
            ESP_LOGI(TAG, "RID uas=%s lat=%.6f lon=%.6f alt=%.1f speed=%.1f hdg=%d fix=%d sat=%u",
                g_state.identity.uas_id,
                g_state.gps.latitude, g_state.gps.longitude,
                (double)g_state.gps.altitude_msl, (double)g_state.gps.speed,
                g_state.gps.heading, g_state.gps.fix_type, g_state.gps.satellites);
        }

        log_cycle++;
        if (log_cycle % 100 == 0) {
            print_status_box();
        }
        if (log_cycle % 500 == 0) {
            print_system_box();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL);
}

void esp_rid_start(void)
{
    if (g_running) return;
    g_running = true;
    xTaskCreate(rid_task, "rid_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "\xE2\x9C\x93 Remote ID started");
}

void esp_rid_stop(void)
{
    g_running = false;
    ESP_LOGI(TAG, "Remote ID stopped");
}
