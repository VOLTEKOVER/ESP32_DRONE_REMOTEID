#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_system.h"

// Include OpenDroneID libraries from id_open component
#include "opendroneid.h"

#define TAG "RID_SCANNER"

// ==================== CONFIGURATION ====================
#define DIAGNOSTICS           1
#define DUMP_ODID_FRAME       0
#define MAX_UAVS              8
#define ID_SIZE               (ODID_ID_SIZE + 1)
#define OP_DISPLAY_LIMIT      16

// ==================== DATA STRUCTURES ====================
typedef struct {
    int       flag;
    uint8_t   mac[6];
    uint32_t  last_seen;
    char      op_id[ID_SIZE];
    char      uav_id[ID_SIZE];
    double    lat_d, long_d, base_lat_d, base_long_d;
    int       altitude_msl, height_agl, speed, heading, rssi;
} id_data_t;

// ==================== GLOBAL VARIABLES ====================
static volatile id_data_t uavs[MAX_UAVS + 1];
static volatile ODID_UAS_Data UAS_data;

static volatile uint32_t callback_counter = 0;
static volatile uint32_t french_wifi = 0;
static volatile uint32_t odid_wifi = 0;

static double base_lat_d = 0.0, base_long_d = 0.0;
static double m_deg_lat = 110000.0, m_deg_long = 110000.0;

// ==================== FUNCTION DECLARATIONS ====================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
static void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);
static id_data_t *next_uav(uint8_t *mac);
static void parse_odid(id_data_t *UAV, ODID_UAS_Data *UAS_data2);
static void parse_french_id(id_data_t *UAV, uint8_t *payload);
static void print_json(int index, uint32_t secs, id_data_t *UAV);
static void calc_m_per_deg(double lat_d, double long_d, double *m_deg_lat, double *m_deg_long);
static char *format_op_id(char *op_id);

// ==================== INITIALIZATION ====================
static void initialize_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void initialize_wifi(void) {
    ESP_LOGD(TAG, "Initializing WiFi...");
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_callback));
    
    // Set channel to 6 (WiFi beacon scanning channel)
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));
    
    ESP_LOGI(TAG, "WiFi promiscuous mode initialized on channel 6");
}

// ==================== EVENT HANDLERS ====================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    ESP_LOGD(TAG, "WiFi event: %ld", event_id);
}

// ==================== WIFI PROMISCUOUS CALLBACK ====================
static void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    
    if (type != WIFI_PKT_MGMT) {
        return;
    }
    
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = packet->payload;
    int length = packet->rx_ctrl.sig_len;
    
    callback_counter++;
    
    // Only process beacon frames (type 0x80)
    if (payload[0] != 0x80) {
        return;
    }
    
    id_data_t *UAV = next_uav(&payload[10]);
    memcpy(UAV->mac, &payload[10], 6);
    UAV->rssi = packet->rx_ctrl.rssi;
    UAV->last_seen = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Parse beacon information elements
    int offset = 36;
    
    while (offset < length) {
        uint8_t type = payload[offset];
        uint8_t len = payload[offset + 1];
        uint8_t *val = &payload[offset + 2];
        
        // Check for French ID format (type 0xdd, vendor-specific)
        if (type == 0xdd && len >= 3 &&
            val[0] == 0x6a && val[1] == 0x5c && val[2] == 0x35) {
            
            french_wifi++;
            parse_french_id(UAV, &payload[offset]);
            
        } 
        // Check for OpenDroneID format
        else if (type == 0xdd && len >= 3 &&
                 ((val[0] == 0x90 && val[1] == 0x3a && val[2] == 0xe6) ||  // Parrot
                  (val[0] == 0xfa && val[1] == 0x0b && val[2] == 0xbc))) { // ODID
            
            odid_wifi++;
            
            if (offset + 7 < length) {
                memset(&UAS_data, 0, sizeof(ODID_UAS_Data));
                odid_message_process_pack(&UAS_data, &payload[offset + 7], length - offset - 7);
                parse_odid(UAV, &UAS_data);
            }
        }
        
        offset += len + 2;
    }
}

// ==================== UAV MANAGEMENT ====================
static id_data_t *next_uav(uint8_t *mac) {
    
    id_data_t *UAV = NULL;
    
    // Check if MAC already in list
    for (int i = 0; i < MAX_UAVS; i++) {
        if (memcmp(uavs[i].mac, mac, 6) == 0) {
            return (id_data_t *)&uavs[i];
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_UAVS; i++) {
        if (uavs[i].mac[0] == 0) {
            return (id_data_t *)&uavs[i];
        }
    }
    
    // Use last slot if full
    return (id_data_t *)&uavs[MAX_UAVS - 1];
}

// ==================== PARSING FUNCTIONS ====================
static void parse_odid(id_data_t *UAV, ODID_UAS_Data *UAS_data2) {
    
    if (UAS_data2->BasicIDValid[0]) {
        UAV->flag = 1;
        strncpy(UAV->uav_id, (char *)UAS_data2->BasicID[0].UASID, ODID_ID_SIZE);
        UAV->uav_id[ODID_ID_SIZE] = '\0';
    }
    
    if (UAS_data2->OperatorIDValid) {
        UAV->flag = 1;
        strncpy(UAV->op_id, (char *)UAS_data2->OperatorID.OperatorId, ODID_ID_SIZE);
        UAV->op_id[ODID_ID_SIZE] = '\0';
    }
    
    if (UAS_data2->LocationValid) {
        UAV->flag = 1;
        UAV->lat_d = UAS_data2->Location.Latitude;
        UAV->long_d = UAS_data2->Location.Longitude;
        UAV->altitude_msl = (int)UAS_data2->Location.AltitudeGeo;
        UAV->height_agl = (int)UAS_data2->Location.Height;
        UAV->speed = (int)UAS_data2->Location.SpeedHorizontal;
        UAV->heading = (int)UAS_data2->Location.Direction;
    }
    
    if (UAS_data2->SystemValid) {
        UAV->flag = 1;
        UAV->base_lat_d = UAS_data2->System.OperatorLatitude;
        UAV->base_long_d = UAS_data2->System.OperatorLongitude;
    }
}

static void parse_french_id(id_data_t *UAV, uint8_t *payload) {
    
    uint8_t length = payload[1];
    int j = 6;
    
    union {int32_t i32; uint32_t u32;} uav_lat = {0}, uav_long = {0},
                                       base_lat = {0}, base_long = {0};
    union {int16_t i16; uint16_t u16;} alt = {0}, height = {0};
    
    UAV->flag = 1;
    
    while (j < length) {
        uint8_t type = payload[j];
        uint8_t len = payload[j + 1];
        uint8_t *val = &payload[j + 2];
        
        switch (type) {
        case 1:
            if (val[0] != 1) return;
            break;
            
        case 2: // Operator ID
            for (int i = 0; i < len - 6 && i < ID_SIZE - 1; i++) {
                UAV->op_id[i] = (char)val[i + 6];
            }
            UAV->op_id[len - 6] = '\0';
            break;
            
        case 3: // UAV ID
            for (int i = 0; i < len && i < ID_SIZE - 1; i++) {
                UAV->uav_id[i] = (char)val[i];
            }
            UAV->uav_id[len] = '\0';
            break;
            
        case 4: // UAV Latitude
            uav_lat.u32 = 0;
            for (int i = 0; i < 4; i++) {
                uav_lat.u32 = (uav_lat.u32 << 8) | val[i];
            }
            break;
            
        case 5: // UAV Longitude
            uav_long.u32 = 0;
            for (int i = 0; i < 4; i++) {
                uav_long.u32 = (uav_long.u32 << 8) | val[i];
            }
            break;
            
        case 6: // Altitude
            alt.u16 = ((uint16_t)val[0] << 8) | (uint16_t)val[1];
            break;
            
        case 7: // Height AGL
            height.u16 = ((uint16_t)val[0] << 8) | (uint16_t)val[1];
            break;
            
        case 8: // Base Latitude
            base_lat.u32 = 0;
            for (int i = 0; i < 4; i++) {
                base_lat.u32 = (base_lat.u32 << 8) | val[i];
            }
            break;
            
        case 9: // Base Longitude
            base_long.u32 = 0;
            for (int i = 0; i < 4; i++) {
                base_long.u32 = (base_long.u32 << 8) | val[i];
            }
            break;
            
        case 10: // Speed
            UAV->speed = val[0];
            break;
            
        case 11: // Heading
            UAV->heading = ((uint16_t)val[0] << 8) | (uint16_t)val[1];
            break;
        }
        
        j += len + 2;
    }
    
    UAV->lat_d = 1.0e-5 * (double)uav_lat.i32;
    UAV->long_d = 1.0e-5 * (double)uav_long.i32;
    UAV->base_lat_d = 1.0e-5 * (double)base_lat.i32;
    UAV->base_long_d = 1.0e-5 * (double)base_long.i32;
    UAV->altitude_msl = alt.i16;
    UAV->height_agl = height.i16;
}

// ==================== UTILITY FUNCTIONS ====================
static void print_json(int index, uint32_t secs, id_data_t *UAV) {
    
    printf("{ \"index\": %d, \"runtime\": %u, \"mac\": \"%02x:%02x:%02x:%02x:%02x:%02x\", ",
           index, secs,
           UAV->mac[0], UAV->mac[1], UAV->mac[2], UAV->mac[3], UAV->mac[4], UAV->mac[5]);
    printf("\"id\": \"%s\", \"uav_latitude\": %.6f, \"uav_longitude\": %.6f, \"altitude_msl\": %d, ",
           UAV->op_id, UAV->lat_d, UAV->long_d, UAV->altitude_msl);
    printf("\"height_agl\": %d, \"base_latitude\": %.6f, \"base_longitude\": %.6f, "
           "\"speed\": %d, \"heading\": %d }\r\n",
           UAV->height_agl, UAV->base_lat_d, UAV->base_long_d, UAV->speed, UAV->heading);
}

static void calc_m_per_deg(double lat_d, double long_d, double *m_deg_lat, double *m_deg_long) {
    
    double pi = 4.0 * atan(1.0);
    double deg2rad = pi / 180.0;
    
    double sin_lat = sin(lat_d * deg2rad);
    double cos_lat = cos(lat_d * deg2rad);
    double a = 0.08181922;
    double b = a * sin_lat;
    double radius = 6378137.0 * cos_lat / sqrt(1.0 - (b * b));
    
    *m_deg_long = deg2rad * radius;
    *m_deg_lat = 111132.954 - (559.822 * cos(2.0 * lat_d * deg2rad)) -
                 (1.175 * cos(4.0 * lat_d * deg2rad));
}

static char *format_op_id(char *op_id) {
    
    static char short_id[OP_DISPLAY_LIMIT + 2];
    const char *_op_ = "-OP-";
    
    strncpy(short_id, op_id, OP_DISPLAY_LIMIT);
    short_id[OP_DISPLAY_LIMIT] = '\0';
    
    int len = strlen(op_id);
    if (len > OP_DISPLAY_LIMIT) {
        char *a = strstr(short_id, _op_);
        char *b = strstr(op_id, _op_);
        if (a && b) {
            int j = strlen(a);
            strncpy(a, &b[3], j);
            short_id[OP_DISPLAY_LIMIT] = '\0';
        }
    }
    
    return short_id;
}

// ==================== MAIN SCANNER TASK ====================
static void scanner_task(void *pvParameter) {
    
    uint32_t last_json = 0;
    
    ESP_LOGI(TAG, "Scanner task started");
    ESP_LOGI(TAG, "{ \"title\": \"RID Scanner\", \"version\": \"ESP-IDF\" }");
    
    memset((void *)uavs, 0, (MAX_UAVS + 1) * sizeof(id_data_t));
    memset((void *)&UAS_data, 0, sizeof(ODID_UAS_Data));
    
    strcpy((char *)uavs[MAX_UAVS].op_id, "NONE");
    
    while (1) {
        uint32_t msecs = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t secs = msecs / 1000;
        
        // Check for stale UAVs (timeout after 5 minutes)
        for (int i = 0; i < MAX_UAVS; i++) {
            if (uavs[i].last_seen && (msecs - uavs[i].last_seen) > 300000L) {
                uavs[i].last_seen = 0;
                uavs[i].mac[0] = 0;
                ESP_LOGD(TAG, "UAV %d timed out", i);
            }
        }
        
        // Process and output UAVs with new data
        for (int i = 0; i < MAX_UAVS; i++) {
            if (uavs[i].flag) {
                print_json(i, secs, (id_data_t *)&uavs[i]);
                
                // Calculate relative position if we have operator location
                if (uavs[i].lat_d && uavs[i].base_lat_d) {
                    if (base_lat_d == 0.0) {
                        base_lat_d = uavs[i].base_lat_d;
                        base_long_d = uavs[i].base_long_d;
                        calc_m_per_deg(base_lat_d, base_long_d, &m_deg_lat, &m_deg_long);
                    }
                }
                
                uavs[i].flag = 0;
                last_json = msecs;
            }
        }
        
        // Keep serial link active with periodic heartbeat
        if ((msecs - last_json) > 60000UL) {
            printf("{ \"heartbeat\": %u, \"callback_counter\": %u, \"odid_wifi\": %u, \"french_wifi\": %u, \"free_heap\": %u }\r\n",
                   secs, callback_counter, odid_wifi, french_wifi, esp_get_free_heap_size());
            last_json = msecs;
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==================== APP MAIN ====================
void app_main(void) {
    
    ESP_LOGI(TAG, "ESP32 Remote ID Scanner initializing");
    
    // Initialize NVS
    initialize_nvs();
    
    // Initialize WiFi in promiscuous mode
    initialize_wifi();
    
    // Create scanner task
    xTaskCreate(&scanner_task, "scanner_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Startup complete, scanning for Remote ID broadcasts...");
}
