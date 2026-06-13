#ifndef ESP_REMOTE_ID_H
#define ESP_REMOTE_ID_H

#include <stdint.h>
#include <stdbool.h>

#define ESP_RID_VERSION "1.0.0"
#define ESP_RID_MAX_STR_LEN 20
#define ESP_RID_MAX_KEY_LEN 64
#define ESP_RID_NUM_KEYS 5

#define RID_OPT_FORCE_ARM_OK      (1 << 0)
#define RID_OPT_DONT_SAVE_BASIC_ID (1 << 1)
#define RID_OPT_PRINT_RID_MAVLINK  (1 << 2)

typedef enum {
    RID_PROTOCOL_UNKNOWN = 0,
    RID_PROTOCOL_MAVLINK,
    RID_PROTOCOL_MSP,
    RID_PROTOCOL_NMEA,
    RID_PROTOCOL_NONE,
    RID_PROTOCOL_AUTO = 255,
} rid_protocol_t;

typedef enum {
    RID_TRANSMIT_WIFI_BCN = (1 << 0),
    RID_TRANSMIT_WIFI_NAN = (1 << 1),
    RID_TRANSMIT_BLE4     = (1 << 2),
    RID_TRANSMIT_BLE5     = (1 << 3),
} rid_transmit_mode_t;

typedef struct {
    double latitude;
    double longitude;
    float altitude_msl;
    float altitude_relative;
    float altitude_baro;
    float speed;
    float speed_vertical;
    int16_t heading;
    uint8_t fix_type;
    uint8_t satellites;
    bool armed;
} rid_gps_data_t;

typedef struct {
    char uas_id[ESP_RID_MAX_STR_LEN + 1];
    char operator_id[ESP_RID_MAX_STR_LEN + 1];
    uint8_t id_type;
    uint8_t ua_type;
    char uas_id_2[ESP_RID_MAX_STR_LEN + 1];
    uint8_t id_type_2;
    uint8_t ua_type_2;
} rid_identity_t;

typedef struct {
    rid_protocol_t protocol;
    uint8_t uart_port;
    uint32_t baud_rate;
    uint8_t tx_pin;
    uint8_t rx_pin;

    uint8_t ua_type;
    uint8_t id_type;
    char uas_id[ESP_RID_MAX_STR_LEN + 1];
    char operator_id[ESP_RID_MAX_STR_LEN + 1];

    uint8_t ua_type_2;
    uint8_t id_type_2;
    char uas_id_2[ESP_RID_MAX_STR_LEN + 1];

    uint8_t tx_modes;
    uint8_t wifi_channel;
    float wifi_power_dbm;
    float wifi_bcn_rate_hz;
    float wifi_nan_rate_hz;
    float ble4_rate_hz;
    float ble4_power_dbm;
    float ble5_rate_hz;
    float ble5_power_dbm;

    char wifi_ssid[ESP_RID_MAX_STR_LEN + 1];
    char wifi_password[ESP_RID_MAX_STR_LEN + 1];
    uint8_t webserver_en;

    uint8_t mavlink_sysid;
    uint8_t bcast_powerup;

    uint8_t options;
    int8_t lock_level;

    char public_keys[ESP_RID_NUM_KEYS][ESP_RID_MAX_KEY_LEN + 1];
} rid_config_t;

typedef struct {
    rid_gps_data_t gps;
    rid_identity_t identity;
    rid_protocol_t active_protocol;
    uint32_t last_update_ms;
    uint32_t transmissions_count;
    uint32_t wifi_bcn_count;
    uint32_t wifi_nan_count;
    uint32_t ble4_count;
    uint32_t ble5_count;
    bool gps_valid;
} rid_state_t;

void esp_rid_init(void);
void esp_rid_set_config(const rid_config_t *config);
void esp_rid_get_config(rid_config_t *config);
void esp_rid_get_state(rid_state_t *state);
void esp_rid_start(void);
void esp_rid_stop(void);
void esp_rid_factory_reset(void);

#endif
