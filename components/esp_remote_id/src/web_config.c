#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "web_config.h"
#include "nvs_storage.h"
#include "esp_remote_id.h"

#define TAG "WEB_CFG"
#define BUF_SIZE 2048
#define MAX_POST 4096

extern const char config_html_start[] asm("_binary_config_html_start");
extern const char config_html_end[] asm("_binary_config_html_end");
#define config_html_size ((size_t)(config_html_end - config_html_start))

static httpd_handle_t g_server = NULL;

static char *json_str(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    const char *e = strchr(p, '"');
    if (!e) return NULL;
    int len = e - p;
    char *v = (char *)malloc(len + 1);
    if (!v) return NULL;
    memcpy(v, p, len);
    v[len] = '\0';
    return v;
}

static int json_int(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ') p++;
    return atoi(p);
}

static float json_float(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ') p++;
    return (float)atof(p);
}

static void apply_json(rid_config_t *cfg, const char *json)
{
    char *tmp;

    tmp = json_str(json, "uas_id"); if (tmp) { strncpy(cfg->uas_id, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }
    tmp = json_str(json, "operator_id"); if (tmp) { strncpy(cfg->operator_id, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }
    tmp = json_str(json, "uas_id_2"); if (tmp) { strncpy(cfg->uas_id_2, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }

    int iv = json_int(json, "id_type"); if (iv > 0) cfg->id_type = (uint8_t)iv;
    iv = json_int(json, "ua_type"); if (iv > 0) cfg->ua_type = (uint8_t)iv;
    iv = json_int(json, "id_type_2"); if (iv > 0) cfg->id_type_2 = (uint8_t)iv;
    iv = json_int(json, "ua_type_2"); if (iv > 0) cfg->ua_type_2 = (uint8_t)iv;

    iv = json_int(json, "tx_modes"); cfg->tx_modes = (uint8_t)iv;
    iv = json_int(json, "wifi_channel"); if (iv >= 1 && iv <= 13) cfg->wifi_channel = (uint8_t)iv;
    iv = json_int(json, "webserver_en"); cfg->webserver_en = (uint8_t)iv;
    iv = json_int(json, "mavlink_sysid"); cfg->mavlink_sysid = (uint8_t)iv;
    iv = json_int(json, "bcast_powerup"); cfg->bcast_powerup = (uint8_t)iv;
    iv = json_int(json, "options"); cfg->options = (uint8_t)iv;
    iv = json_int(json, "lock_level"); cfg->lock_level = (int8_t)iv;
    if (json_int(json, "baud_rate") > 0) cfg->baud_rate = (uint32_t)json_int(json, "baud_rate");

    float fv = json_float(json, "wifi_power_dbm"); if (fv >= 2 && fv <= 20) cfg->wifi_power_dbm = fv;
    fv = json_float(json, "wifi_bcn_rate_hz"); if (fv >= 0 && fv <= 5) cfg->wifi_bcn_rate_hz = fv;
    fv = json_float(json, "wifi_nan_rate_hz"); if (fv >= 0 && fv <= 5) cfg->wifi_nan_rate_hz = fv;
    fv = json_float(json, "ble4_rate_hz"); if (fv >= 0 && fv <= 5) cfg->ble4_rate_hz = fv;
    fv = json_float(json, "ble4_power_dbm"); if (fv >= -27 && fv <= 18) cfg->ble4_power_dbm = fv;
    fv = json_float(json, "ble5_rate_hz"); if (fv >= 0 && fv <= 5) cfg->ble5_rate_hz = fv;
    fv = json_float(json, "ble5_power_dbm"); if (fv >= -27 && fv <= 18) cfg->ble5_power_dbm = fv;

    tmp = json_str(json, "wifi_ssid"); if (tmp) { strncpy(cfg->wifi_ssid, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }
    tmp = json_str(json, "wifi_password"); if (tmp) { strncpy(cfg->wifi_password, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }

    char kname[16];
    for (int i = 1; i <= ESP_RID_NUM_KEYS; i++) {
        snprintf(kname, sizeof(kname), "public_key_%d", i);
        tmp = json_str(json, kname);
        if (tmp) { strncpy(cfg->public_keys[i - 1], tmp, ESP_RID_MAX_KEY_LEN); free(tmp); }
    }
}

static void config_to_json(const rid_config_t *c, char *buf, size_t sz)
{
    snprintf(buf, sz,
        "{"
        "\"uas_id\":\"%s\",\"id_type\":%u,\"ua_type\":%u,\"operator_id\":\"%s\","
        "\"uas_id_2\":\"%s\",\"id_type_2\":%u,\"ua_type_2\":%u,"
        "\"tx_modes\":%u,\"wifi_channel\":%u,\"wifi_power_dbm\":%.1f,"
        "\"wifi_bcn_rate_hz\":%.1f,\"wifi_nan_rate_hz\":%.1f,"
        "\"ble4_rate_hz\":%.1f,\"ble4_power_dbm\":%.1f,"
        "\"ble5_rate_hz\":%.1f,\"ble5_power_dbm\":%.1f,"
        "\"wifi_ssid\":\"%s\",\"wifi_password\":\"%s\",\"webserver_en\":%u,"
        "\"baud_rate\":%lu,\"mavlink_sysid\":%u,\"bcast_powerup\":%u,"
        "\"options\":%u,\"lock_level\":%d,"
        "\"public_key_1\":\"%s\",\"public_key_2\":\"%s\","
        "\"public_key_3\":\"%s\",\"public_key_4\":\"%s\",\"public_key_5\":\"%s\""
        "}",
        c->uas_id, c->id_type, c->ua_type, c->operator_id,
        c->uas_id_2, c->id_type_2, c->ua_type_2,
        c->tx_modes, c->wifi_channel, (double)c->wifi_power_dbm,
        (double)c->wifi_bcn_rate_hz, (double)c->wifi_nan_rate_hz,
        (double)c->ble4_rate_hz, (double)c->ble4_power_dbm,
        (double)c->ble5_rate_hz, (double)c->ble5_power_dbm,
        c->wifi_ssid, c->wifi_password, c->webserver_en,
        (unsigned long)c->baud_rate, c->mavlink_sysid, c->bcast_powerup,
        c->options, c->lock_level,
        c->public_keys[0], c->public_keys[1],
        c->public_keys[2], c->public_keys[3], c->public_keys[4]);
}

static void state_to_json(const rid_state_t *s, char *buf, size_t sz)
{
    snprintf(buf, sz,
        "{"
        "\"protocol\":%d,\"gps_valid\":%s,\"lat\":%.6f,\"lon\":%.6f,"
        "\"alt\":%.1f,\"speed\":%.1f,\"heading\":%d,\"satellites\":%u,\"fix_type\":%u,"
        "\"tx_total\":%lu,\"tx_wifi_bcn\":%lu,\"tx_wifi_nan\":%lu,"
        "\"tx_ble4\":%lu,\"tx_ble5\":%lu,"
        "\"uptime_ms\":%lu"
        "}",
        (int)s->active_protocol, s->gps_valid ? "true" : "false",
        s->gps.latitude, s->gps.longitude,
        (double)s->gps.altitude_msl, (double)s->gps.speed,
        s->gps.heading, s->gps.satellites, s->gps.fix_type,
        (unsigned long)s->transmissions_count,
        (unsigned long)s->wifi_bcn_count, (unsigned long)s->wifi_nan_count,
        (unsigned long)s->ble4_count, (unsigned long)s->ble5_count,
        (unsigned long)s->last_update_ms);
}

static esp_err_t handle_get_config(httpd_req_t *req)
{
    rid_config_t cfg;
    esp_rid_get_config(&cfg);
    char buf[BUF_SIZE];
    config_to_json(&cfg, buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t handle_post_config(httpd_req_t *req)
{
    char *body = (char *)malloc(MAX_POST);
    if (!body) { httpd_resp_send_500(req); return ESP_FAIL; }

    int ret = httpd_req_recv(req, body, MAX_POST - 1);
    if (ret <= 0) { free(body); httpd_resp_send_500(req); return ESP_FAIL; }
    body[ret] = '\0';

    rid_config_t cfg;
    esp_rid_get_config(&cfg);
    apply_json(&cfg, body);
    free(body);

    esp_rid_set_config(&cfg);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
    return ESP_OK;
}

static esp_err_t handle_get_status(httpd_req_t *req)
{
    rid_state_t state;
    esp_rid_get_state(&state);
    char buf[BUF_SIZE];
    state_to_json(&state, buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t handle_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, config_html_start, config_html_size);
    return ESP_OK;
}

static esp_err_t handle_factory_reset(httpd_req_t *req)
{
    esp_rid_factory_reset();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"reset\"}", 18);
    esp_restart();
    return ESP_OK;
}

static esp_err_t handle_ota(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_part = esp_ota_get_next_update_partition(NULL);
    if (!ota_part) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int ret;
    esp_err_t err = esp_ota_begin(ota_part, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_sendstr(req, "OTA begin failed");
        return ESP_FAIL;
    }

    while ((ret = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        if (esp_ota_write(ota_handle, buf, ret) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_part) != ESP_OK) {
        httpd_resp_sendstr(req, "OTA finalize failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OTA OK, rebooting...");
    esp_restart();
    return ESP_OK;
}

static const httpd_uri_t uri_index = { "/", HTTP_GET, handle_index, NULL };
static const httpd_uri_t uri_get_cfg = { "/api/config", HTTP_GET, handle_get_config, NULL };
static const httpd_uri_t uri_set_cfg = { "/api/config", HTTP_POST, handle_post_config, NULL };
static const httpd_uri_t uri_status = { "/api/status", HTTP_GET, handle_get_status, NULL };
static const httpd_uri_t uri_reset = { "/api/reset", HTTP_POST, handle_factory_reset, NULL };
static const httpd_uri_t uri_ota = { "/ota", HTTP_POST, handle_ota, NULL };

void web_config_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;

    if (httpd_start(&g_server, &config) == ESP_OK) {
        httpd_register_uri_handler(g_server, &uri_index);
        httpd_register_uri_handler(g_server, &uri_get_cfg);
        httpd_register_uri_handler(g_server, &uri_set_cfg);
        httpd_register_uri_handler(g_server, &uri_status);
        httpd_register_uri_handler(g_server, &uri_reset);
        httpd_register_uri_handler(g_server, &uri_ota);
        ESP_LOGI(TAG, "Web server started on port 80");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}
