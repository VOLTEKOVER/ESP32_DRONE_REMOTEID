#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mavlink_parser.h"
#include "common/mavlink.h"

#define TAG "MAVLINK"
#define UART_MAV UART_NUM_1
#define MAV_RX_BUF 512

static rid_gps_data_t g_last_gps;
static uint32_t g_last_update = 0;
static mavlink_status_t g_mav_status;
static uint8_t g_mav_buf[MAV_RX_BUF];
static uint8_t g_sysid_filter = 0;

void mavlink_parser_init(void)
{
    memset(&g_last_gps, 0, sizeof(rid_gps_data_t));
    memset(&g_mav_status, 0, sizeof(g_mav_status));
}

void mavlink_parser_set_sysid_filter(uint8_t sysid)
{
    g_sysid_filter = sysid;
}

bool mavlink_parser_get(rid_gps_data_t *gps)
{
    int len = uart_read_bytes(UART_MAV, g_mav_buf, sizeof(g_mav_buf), 0);
    if (len > 0) {
        mavlink_message_t msg;
        mavlink_status_t status;

        for (int i = 0; i < len; i++) {
            if (mavlink_parse_char(MAVLINK_COMM_0, g_mav_buf[i], &msg, &status)) {
                if (g_sysid_filter != 0 && msg.sysid != g_sysid_filter) continue;

                switch (msg.msgid) {
                case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                    mavlink_global_position_int_t pos;
                    mavlink_msg_global_position_int_decode(&msg, &pos);
                    g_last_gps.latitude = pos.lat / 1e7;
                    g_last_gps.longitude = pos.lon / 1e7;
                    g_last_gps.altitude_msl = pos.alt / 1000.0f;
                    g_last_gps.altitude_relative = pos.relative_alt / 1000.0f;
                    g_last_gps.heading = pos.hdg / 100;
                    g_last_gps.fix_type = 3;
                    float vx = pos.vx / 100.0f;
                    float vy = pos.vy / 100.0f;
                    g_last_gps.speed = sqrtf(vx * vx + vy * vy);
                    g_last_gps.speed_vertical = -pos.vz / 100.0f;
                    break;
                }
                case MAVLINK_MSG_ID_GPS_RAW_INT: {
                    mavlink_gps_raw_int_t raw;
                    mavlink_msg_gps_raw_int_decode(&msg, &raw);
                    g_last_gps.latitude = raw.lat / 1e7;
                    g_last_gps.longitude = raw.lon / 1e7;
                    g_last_gps.altitude_msl = raw.alt / 1000.0f;
                    g_last_gps.fix_type = raw.fix_type;
                    g_last_gps.satellites = raw.satellites_visible;
                    g_last_gps.speed = raw.vel / 100.0f;
                    g_last_gps.heading = raw.cog / 100;
                    break;
                }
                case MAVLINK_MSG_ID_VFR_HUD: {
                    mavlink_vfr_hud_t hud;
                    mavlink_msg_vfr_hud_decode(&msg, &hud);
                    g_last_gps.speed = hud.groundspeed;
                    g_last_gps.heading = hud.heading;
                    g_last_gps.altitude_msl = hud.alt;
                    break;
                }
                case MAVLINK_MSG_ID_ATTITUDE: {
                    mavlink_attitude_t att;
                    mavlink_msg_attitude_decode(&msg, &att);
                    g_last_gps.heading = (int16_t)(att.yaw * 180.0f / 3.14159f);
                    if (g_last_gps.heading < 0) g_last_gps.heading += 360;
                    break;
                }
                case MAVLINK_MSG_ID_HEARTBEAT: {
                    const uint8_t *p = (const uint8_t *)msg.payload64;
                    g_last_gps.armed = (p[2] & 128) != 0;
                    break;
                }
                default:
                    break;
                }

                if (g_last_gps.latitude != 0.0 || g_last_gps.longitude != 0.0) {
                    g_last_update = xTaskGetTickCount() * portTICK_PERIOD_MS;
                }
            }
        }
    }

    if (g_last_update != 0 && (xTaskGetTickCount() * portTICK_PERIOD_MS - g_last_update) < 5000) {
        memcpy(gps, &g_last_gps, sizeof(rid_gps_data_t));
        return true;
    }

    return false;
}
