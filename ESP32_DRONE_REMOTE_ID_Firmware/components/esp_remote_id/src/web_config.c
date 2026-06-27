#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"
#include "psa/crypto.h"
#include "mbedtls/pk.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "web_config.h"
#include "nvs_storage.h"
#include "esp_remote_id.h"
#include "protocol_detect.h"
#include "esp_efuse.h"
#include "led_status.h"

#define TAG "WEB_CFG"
#define BUF_SIZE 4096
#define MAX_POST 4096
#define EFUSE_LOCK_MAGIC 0x52494421 // "RID!" stored as uint32_t

extern const char config_html_start[] asm("_binary_config_html_start");
extern const char config_html_end[] asm("_binary_config_html_end");
#define config_html_size ((size_t)(config_html_end - config_html_start))

static httpd_handle_t g_server = NULL;

static int get_lock_level(void)
{
    uint8_t efuse_data[4] = {0};
    esp_efuse_read_block(EFUSE_BLK3, efuse_data, 0, 32);
    uint32_t magic = (uint32_t)efuse_data[0] | ((uint32_t)efuse_data[1] << 8)
                   | ((uint32_t)efuse_data[2] << 16) | ((uint32_t)efuse_data[3] << 24);
    if (magic == EFUSE_LOCK_MAGIC) return 2;

    rid_config_t cfg;
    esp_rid_get_config(&cfg);
    return cfg.lock_level;
}

/* ---------- Log ring buffer ---------- */
#define LOG_RING_MAX 64
#define LOG_MSG_MAX 240

typedef struct {
    uint32_t time_ms;
    char level;
    char msg[LOG_MSG_MAX];
} log_entry_t;

static log_entry_t s_log_ring[LOG_RING_MAX];
static int s_log_head = 0;
static int s_log_count = 0;
static SemaphoreHandle_t s_log_lock = NULL;
static int (*s_orig_vprintf)(const char *, va_list) = NULL;

static void log_push(char level, const char *msg)
{
    if (!s_log_lock) return;
    if (xSemaphoreTake(s_log_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        int i = (s_log_head + s_log_count) % LOG_RING_MAX;
        s_log_ring[i].time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        s_log_ring[i].level = level;
        strncpy(s_log_ring[i].msg, msg, LOG_MSG_MAX - 1);
        s_log_ring[i].msg[LOG_MSG_MAX - 1] = '\0';
        if (s_log_count < LOG_RING_MAX) s_log_count++;
        else s_log_head = (s_log_head + 1) % LOG_RING_MAX;
        xSemaphoreGive(s_log_lock);
    }
}

static int log_vprintf(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int ret = 0;
    if (s_orig_vprintf) ret = s_orig_vprintf(fmt, args);
    char buf[LOG_MSG_MAX];
    int n = vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);
    if (n > 0) {
        char lv = 'I';
        if (buf[0] == 'E' || buf[0] == 'W' || buf[0] == 'I' || buf[0] == 'D' || buf[0] == 'V') {
            lv = buf[0];
        }
        log_push(lv, buf);
    }
    return ret;
}

static void log_init(void)
{
    s_log_lock = xSemaphoreCreateMutex();
    s_orig_vprintf = esp_log_set_vprintf(log_vprintf);
}

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
    tmp = json_str(json, "self_id_text"); if (tmp) { strncpy(cfg->self_id_text, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }
    tmp = json_str(json, "uas_id_2"); if (tmp) { strncpy(cfg->uas_id_2, tmp, ESP_RID_MAX_STR_LEN); free(tmp); }

    int iv = json_int(json, "id_type"); if (iv > 0) cfg->id_type = (uint8_t)iv;
    iv = json_int(json, "ua_type"); if (iv > 0) cfg->ua_type = (uint8_t)iv;
    iv = json_int(json, "id_type_2"); if (iv > 0) cfg->id_type_2 = (uint8_t)iv;
    iv = json_int(json, "ua_type_2"); if (iv > 0) cfg->ua_type_2 = (uint8_t)iv;

    if (strstr(json, "\"protocol\"")) {
        int p = json_int(json, "protocol");
        if (p >= 1 && p <= 4) cfg->protocol = (rid_protocol_t)p;
        else cfg->protocol = RID_PROTOCOL_AUTO;
    }
    iv = json_int(json, "tx_modes"); cfg->tx_modes = (uint8_t)iv;
    iv = json_int(json, "wifi_channel"); if (iv >= 1 && iv <= 13) cfg->wifi_channel = (uint8_t)iv;
    iv = json_int(json, "webserver_en"); cfg->webserver_en = (uint8_t)iv;
    iv = json_int(json, "mavlink_sysid"); cfg->mavlink_sysid = (uint8_t)iv;
    iv = json_int(json, "bcast_powerup"); cfg->bcast_powerup = (uint8_t)iv;
    iv = json_int(json, "options"); cfg->options = (uint8_t)iv;
    iv = json_int(json, "lock_level");

    /* If transitioning to level 2, burn eFuse for permanence */
    if (iv >= 2) {
        uint8_t efuse_data[4] = {0};
        esp_efuse_read_block(EFUSE_BLK3, efuse_data, 0, 32);
        uint32_t magic = (uint32_t)efuse_data[0] | ((uint32_t)efuse_data[1] << 8)
                       | ((uint32_t)efuse_data[2] << 16) | ((uint32_t)efuse_data[3] << 24);
        if (magic != EFUSE_LOCK_MAGIC) {
            uint32_t val = EFUSE_LOCK_MAGIC;
            esp_err_t err = esp_efuse_write_block(EFUSE_BLK3, &val, 0, 32);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "eFuse permanent lock burned");
            } else {
                ESP_LOGE(TAG, "eFuse write failed: %s", esp_err_to_name(err));
            }
        }
        cfg->lock_level = 2;
    } else if (iv >= 1) {
        cfg->lock_level = (int8_t)iv;
    } else {
        cfg->lock_level = 0;
    }
    iv = json_int(json, "led_r_gpio"); cfg->led_r_gpio = (int8_t)iv;
    iv = json_int(json, "led_g_gpio"); cfg->led_g_gpio = (int8_t)iv;
    iv = json_int(json, "led_b_gpio"); cfg->led_b_gpio = (int8_t)iv;
    if (json_int(json, "baud_rate") > 0) cfg->baud_rate = (uint32_t)json_int(json, "baud_rate");

    float fv = json_float(json, "wifi_power_dbm"); if (fv >= 2 && fv <= 20) cfg->wifi_power_dbm = fv;
    fv = json_float(json, "wifi_bcn_rate_hz"); if (fv >= 0 && fv <= 5) cfg->wifi_bcn_rate_hz = fv;
    fv = json_float(json, "wifi_nan_rate_hz"); if (fv >= 0 && fv <= 5) cfg->wifi_nan_rate_hz = fv;
    fv = json_float(json, "ble4_rate_hz"); if (fv >= 0 && fv <= 5) cfg->ble4_rate_hz = fv;
    fv = json_float(json, "ble4_power_dbm"); if (fv >= -27 && fv <= 18) cfg->ble4_power_dbm = fv;
    fv = json_float(json, "ble5_rate_hz"); if (fv >= 0 && fv <= 5) cfg->ble5_rate_hz = fv;
    fv = json_float(json, "ble5_power_dbm"); if (fv >= -27 && fv <= 18) cfg->ble5_power_dbm = fv;

    double dv = json_float(json, "operator_lat"); if (dv != 0) cfg->operator_lat = dv;
    dv = json_float(json, "operator_lon"); if (dv != 0) cfg->operator_lon = dv;
    fv = json_float(json, "operator_alt"); cfg->operator_alt = fv;

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
        "\"protocol\":%u,"
        "\"uas_id\":\"%s\",\"id_type\":%u,\"ua_type\":%u,\"operator_id\":\"%s\",\"self_id_text\":\"%s\","
        "\"uas_id_2\":\"%s\",\"id_type_2\":%u,\"ua_type_2\":%u,"
        "\"tx_modes\":%u,\"wifi_channel\":%u,\"wifi_power_dbm\":%.1f,"
        "\"wifi_bcn_rate_hz\":%.1f,\"wifi_nan_rate_hz\":%.1f,"
        "\"ble4_rate_hz\":%.1f,\"ble4_power_dbm\":%.1f,"
        "\"ble5_rate_hz\":%.1f,\"ble5_power_dbm\":%.1f,"
        "\"wifi_ssid\":\"%s\",\"wifi_password\":\"%s\",\"webserver_en\":%u,"
        "\"baud_rate\":%lu,\"mavlink_sysid\":%u,\"bcast_powerup\":%u,"
        "\"operator_lat\":%.6f,\"operator_lon\":%.6f,\"operator_alt\":%.1f,"
        "\"options\":%u,\"lock_level\":%d,"
        "\"led_r_gpio\":%d,\"led_g_gpio\":%d,\"led_b_gpio\":%d,"
        "\"public_key_1\":\"%s\",\"public_key_2\":\"%s\","
        "\"public_key_3\":\"%s\",\"public_key_4\":\"%s\",\"public_key_5\":\"%s\""
        "}",
        (unsigned)c->protocol,
        c->uas_id, c->id_type, c->ua_type, c->operator_id, c->self_id_text,
        c->uas_id_2, c->id_type_2, c->ua_type_2,
        c->tx_modes, c->wifi_channel, (double)c->wifi_power_dbm,
        (double)c->wifi_bcn_rate_hz, (double)c->wifi_nan_rate_hz,
        (double)c->ble4_rate_hz, (double)c->ble4_power_dbm,
        (double)c->ble5_rate_hz, (double)c->ble5_power_dbm,
        c->wifi_ssid, c->wifi_password, c->webserver_en,
        (unsigned long)c->baud_rate, c->mavlink_sysid, c->bcast_powerup,
        c->operator_lat, c->operator_lon, (double)c->operator_alt,
        c->options, c->lock_level,
        c->led_r_gpio, c->led_g_gpio, c->led_b_gpio,
        c->public_keys[0], c->public_keys[1],
        c->public_keys[2], c->public_keys[3], c->public_keys[4]);
}

static void state_to_json(const rid_state_t *s, char *buf, size_t sz)
{
    snprintf(buf, sz,
        "{"
        "\"fw_version\":\"%s\",\"protocol\":%d,\"gps_valid\":%s,\"lat\":%.6f,\"lon\":%.6f,"
        "\"alt\":%.1f,\"speed\":%.1f,\"heading\":%d,\"satellites\":%u,\"fix_type\":%u,"
        "\"tx_total\":%lu,\"tx_wifi_bcn\":%lu,\"tx_wifi_nan\":%lu,"
        "\"tx_ble4\":%lu,\"tx_ble5\":%lu,"
        "\"uptime_ms\":%lu"
        "}",
        ESP_RID_VERSION,
        (int)s->active_protocol, s->gps_valid ? "true" : "false",
        s->gps.latitude, s->gps.longitude,
        (double)s->gps.altitude_msl, (double)s->gps.speed,
        s->gps.heading, s->gps.satellites, s->gps.fix_type,
        (unsigned long)s->transmissions_count,
        (unsigned long)s->wifi_bcn_count, (unsigned long)s->wifi_nan_count,
        (unsigned long)s->ble4_count, (unsigned long)s->ble5_count,
        (unsigned long)s->last_update_ms);
}

static const char b64_tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_decode(const char *in, size_t in_len, uint8_t *out, size_t out_size)
{
    int len = 0;
    uint8_t buf[4];
    int buf_i = 0;
    for (size_t i = 0; i < in_len && in[i] != '='; i++) {
        char c = in[i];
        const char *p = strchr(b64_tab, c);
        if (!p) continue;
        buf[buf_i++] = (uint8_t)(p - b64_tab);
        if (buf_i == 4) {
            if (len >= (int)out_size) return -1;
            out[len++] = (buf[0] << 2) | (buf[1] >> 4);
            if (len >= (int)out_size) return -1;
            out[len++] = (buf[1] << 4) | (buf[2] >> 2);
            if (len >= (int)out_size) return -1;
            out[len++] = (buf[2] << 6) | buf[3];
            buf_i = 0;
        }
    }
    if (buf_i >= 2) {
        if (len >= (int)out_size) return -1;
        out[len++] = (buf[0] << 2) | (buf[1] >> 4);
    }
    if (buf_i >= 3) {
        if (len >= (int)out_size) return -1;
        out[len++] = (buf[1] << 4) | (buf[2] >> 2);
    }
    return len;
}

static bool verify_signed_body(const char *body, const char *sig_b64, const rid_config_t *cfg)
{
    if (!body || !sig_b64 || !*sig_b64 || !cfg) return false;

    size_t b64_len = strlen(sig_b64);
    size_t sig_max = (b64_len * 3) / 4 + 4;
    uint8_t *sig = (uint8_t *)malloc(sig_max);
    if (!sig) return false;

    int sig_len = b64_decode(sig_b64, b64_len, sig, sig_max);
    if (sig_len <= 0) { free(sig); return false; }

    uint8_t hash[32];
    size_t hash_len;
    if (psa_hash_compute(PSA_ALG_SHA_256, (const uint8_t *)body, strlen(body),
                         hash, sizeof(hash), &hash_len) != PSA_SUCCESS) {
        free(sig);
        return false;
    }

    bool verified = false;
    for (int i = 0; i < ESP_RID_NUM_KEYS; i++) {
        const char *key_str = cfg->public_keys[i];
        if (!key_str || !*key_str) continue;

        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);

        int ret;
        size_t key_len = strlen(key_str);

        /* Try as PEM or DER first */
        ret = mbedtls_pk_parse_public_key(&pk, (const uint8_t *)key_str, key_len);
        if (ret != 0) {
            /* Try after stripping PUBLIC_KEYV1: prefix */
            const char *prefix = "PUBLIC_KEYV1:";
            size_t plen = strlen(prefix);
            if (key_len > plen && strncasecmp(key_str, prefix, plen) == 0) {
                const char *payload = key_str + plen;
                size_t payload_len = key_len - plen;
                uint8_t *key_bin = (uint8_t *)malloc(payload_len);
                if (key_bin) {
                    int key_bin_len = b64_decode(payload, payload_len, key_bin, payload_len);
                    if (key_bin_len > 0) {
                        mbedtls_pk_free(&pk);
                        mbedtls_pk_init(&pk);
                        ret = mbedtls_pk_parse_public_key(&pk, key_bin, key_bin_len);
                    }
                    free(key_bin);
                }
            }
        }

        if (ret != 0) {
            mbedtls_pk_free(&pk);
            continue;
        }

        ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, hash_len, sig, sig_len);
        mbedtls_pk_free(&pk);

        if (ret == 0) {
            verified = true;
            break;
        }
    }

    free(sig);
    return verified;
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

    if (get_lock_level() >= 1) {
        char sig_hdr[512] = {0};
        size_t hdr_len = httpd_req_get_hdr_value_len(req, "X-Signature");
        if (hdr_len > 0 && hdr_len < sizeof(sig_hdr)) {
            httpd_req_get_hdr_value_str(req, "X-Signature", sig_hdr, sizeof(sig_hdr));
        }

        rid_config_t cfg;
        esp_rid_get_config(&cfg);

        if (!verify_signed_body(body, sig_hdr, &cfg)) {
            free(body);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"status\":\"invalid_signature\"}", 33);
            return ESP_OK;
        }
    }

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
    if (get_lock_level() >= 1) {
        char sig_hdr[512] = {0};
        size_t hdr_len = httpd_req_get_hdr_value_len(req, "X-Signature");
        if (hdr_len > 0 && hdr_len < sizeof(sig_hdr)) {
            httpd_req_get_hdr_value_str(req, "X-Signature", sig_hdr, sizeof(sig_hdr));
        }
        rid_config_t cfg;
        esp_rid_get_config(&cfg);
        if (!verify_signed_body("factory_reset", sig_hdr, &cfg)) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"status\":\"invalid_signature\"}", 33);
            return ESP_OK;
        }
    }
    esp_rid_factory_reset();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"reset\"}", 18);
    esp_restart();
    return ESP_OK;
}

static const char hex_chars[] = "0123456789abcdef";

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex_chars[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex_chars[bytes[i] & 0xF];
    }
    out[len * 2] = '\0';
}

static bool hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) return false;
    for (size_t i = 0; i < out_len; i++) {
        char hi = hex[i * 2], lo = hex[i * 2 + 1];
        uint8_t b = 0;
        if (hi >= '0' && hi <= '9') b = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') b = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') b = (hi - 'A' + 10) << 4;
        else return false;
        if (lo >= '0' && lo <= '9') b |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') b |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') b |= (lo - 'A' + 10);
        else return false;
        out[i] = b;
    }
    return true;
}

static esp_err_t handle_ota(httpd_req_t *req)
{
    if (get_lock_level() >= 2) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"locked\"}", 20);
        return ESP_OK;
    }

    /* Read optional X-Expected-SHA256 header */
    char expected_hex[65] = {0};
    bool has_expected = false;
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "X-Expected-SHA256");
    if (hdr_len > 0 && hdr_len <= 64) {
        httpd_req_get_hdr_value_str(req, "X-Expected-SHA256", expected_hex, sizeof(expected_hex));
        has_expected = true;
    }

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

    psa_hash_operation_t sha_ctx = psa_hash_operation_init();
    if (psa_hash_setup(&sha_ctx, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        esp_ota_abort(ota_handle);
        httpd_resp_sendstr(req, "OTA failed: SHA-256 setup error");
        return ESP_FAIL;
    }

    led_status_set_state(RID_LED_OTA);

    while ((ret = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        if (psa_hash_update(&sha_ctx, (const unsigned char *)buf, ret) != PSA_SUCCESS) {
            psa_hash_abort(&sha_ctx);
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (esp_ota_write(ota_handle, buf, ret) != ESP_OK) {
            psa_hash_abort(&sha_ctx);
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    uint8_t hash[32];
    size_t hash_len;
    if (psa_hash_finish(&sha_ctx, hash, sizeof(hash), &hash_len) != PSA_SUCCESS) {
        esp_ota_abort(ota_handle);
        httpd_resp_sendstr(req, "OTA failed: SHA-256 finalize error");
        return ESP_FAIL;
    }

    if (!has_expected) {
        esp_ota_abort(ota_handle);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OTA rejected: X-Expected-SHA256 header required");
        return ESP_FAIL;
    }

    {
        uint8_t expected_hash[32];
        if (!hex_to_bytes(expected_hex, expected_hash, 32) ||
            memcmp(hash, expected_hash, 32) != 0) {
            char got_hex[65];
            bytes_to_hex(hash, 32, got_hex);
            esp_ota_abort(ota_handle);
            char err_msg[192];
            snprintf(err_msg, sizeof(err_msg),
                "SHA-256 mismatch\nexpected: %s\nreceived: %s",
                expected_hex, got_hex);
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, err_msg);
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

static esp_err_t handle_get_logs(httpd_req_t *req)
{
    char *buf = (char *)malloc(4096);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    int off = 0;
    off += snprintf(buf + off, 4096 - off, "[");
    if (s_log_lock && xSemaphoreTake(s_log_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        int n = s_log_count;
        int start = (n < LOG_RING_MAX) ? 0 : s_log_head;
        for (int i = 0; i < n; i++) {
            int idx = (start + i) % LOG_RING_MAX;
            log_entry_t *e = &s_log_ring[idx];
            char lvstr[2] = { e->level, '\0' };
            char escaped[LOG_MSG_MAX * 2];
            int eo = 0;
            for (int si = 0; e->msg[si] && eo < (int)sizeof(escaped) - 4; si++) {
                char c = e->msg[si];
                if (c == '"' || c == '\\') { escaped[eo++] = '\\'; escaped[eo++] = c; }
                else if (c == '\n') { escaped[eo++] = '\\'; escaped[eo++] = 'n'; }
                else if (c == '\r') { escaped[eo++] = '\\'; escaped[eo++] = 'r'; }
                else if (c == '\t') { escaped[eo++] = '\\'; escaped[eo++] = 't'; }
                else if (c < 0x20) continue;
                else escaped[eo++] = c;
            }
            escaped[eo] = '\0';
            if (i > 0) off += snprintf(buf + off, 4096 - off, ",");
            off += snprintf(buf + off, 4096 - off,
                "{\"t\":%lu,\"l\":\"%s\",\"m\":\"%s\"}",
                (unsigned long)e->time_ms, lvstr, escaped);
            if (off >= 4096 - 128) { break; }
        }
        xSemaphoreGive(s_log_lock);
    }
    off += snprintf(buf + off, 4096 - off, "]");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    free(buf);
    return ESP_OK;
}

static esp_err_t handle_post_command(httpd_req_t *req)
{
    int locked = get_lock_level();

    char body[256];
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    body[ret] = '\0';

    /* strip quotes if wrapped */
    char *cmd = body;
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (cmd[0] == '"') { cmd++; char *e = strchr(cmd, '"'); if (e) *e = '\0'; }

    /* Check if command needs auth when locked */
    bool needs_auth = (strcmp(cmd, "restart") == 0 || strcmp(cmd, "reboot") == 0 ||
                       strcmp(cmd, "reset") == 0 || strcmp(cmd, "factory") == 0);

    if (locked >= 1 && needs_auth) {
        char sig_hdr[512] = {0};
        size_t hdr_len = httpd_req_get_hdr_value_len(req, "X-Signature");
        if (hdr_len > 0 && hdr_len < sizeof(sig_hdr)) {
            httpd_req_get_hdr_value_str(req, "X-Signature", sig_hdr, sizeof(sig_hdr));
        }
        rid_config_t cfg;
        esp_rid_get_config(&cfg);
        if (!verify_signed_body(cmd, sig_hdr, &cfg)) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"status\":\"invalid_signature\"}", 33);
            return ESP_OK;
        }
    }

    esp_err_t res = ESP_OK;
    const char *reply = "ok";

    if (strcmp(cmd, "restart") == 0 || strcmp(cmd, "reboot") == 0) {
        reply = "restarting";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"restarting\"}", 22);
        esp_restart();
        return ESP_OK;
    } else if (strcmp(cmd, "reset") == 0 || strcmp(cmd, "factory") == 0) {
        esp_rid_factory_reset();
        reply = "factory reset, restarting";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"reset\"}", 18);
        esp_restart();
        return ESP_OK;
    } else if (strcmp(cmd, "status") == 0) {
        rid_state_t st;
        esp_rid_get_state(&st);
        char tmp[512];
        state_to_json(&st, tmp, sizeof(tmp));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, tmp, strlen(tmp));
        return ESP_OK;
    } else {
        /* forward unknown command as log entry */
        ESP_LOGI("CMD", "Received command: %s", cmd);
        reply = "unknown command";
    }

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\"}", reply);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return res;
}

static const httpd_uri_t uri_index = { "/", HTTP_GET, handle_index, NULL };
static const httpd_uri_t uri_get_cfg = { "/api/config", HTTP_GET, handle_get_config, NULL };
static const httpd_uri_t uri_set_cfg = { "/api/config", HTTP_POST, handle_post_config, NULL };
static const httpd_uri_t uri_status = { "/api/status", HTTP_GET, handle_get_status, NULL };
static const httpd_uri_t uri_reset = { "/api/reset", HTTP_POST, handle_factory_reset, NULL };
static const httpd_uri_t uri_ota = { "/ota", HTTP_POST, handle_ota, NULL };
static const httpd_uri_t uri_logs = { "/api/logs", HTTP_GET, handle_get_logs, NULL };
static const httpd_uri_t uri_cmd = { "/api/command", HTTP_POST, handle_post_command, NULL };

void web_config_init(void)
{
    log_init();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;

    if (httpd_start(&g_server, &config) == ESP_OK) {
        httpd_register_uri_handler(g_server, &uri_index);
        httpd_register_uri_handler(g_server, &uri_get_cfg);
        httpd_register_uri_handler(g_server, &uri_set_cfg);
        httpd_register_uri_handler(g_server, &uri_status);
        httpd_register_uri_handler(g_server, &uri_reset);
        httpd_register_uri_handler(g_server, &uri_ota);
        httpd_register_uri_handler(g_server, &uri_logs);
        httpd_register_uri_handler(g_server, &uri_cmd);
        ESP_LOGI(TAG, "Web server started on port 80");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}
