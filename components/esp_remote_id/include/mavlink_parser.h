#ifndef MAVLINK_PARSER_H
#define MAVLINK_PARSER_H

#include "esp_remote_id.h"

void mavlink_parser_init(void);
bool mavlink_parser_get(rid_gps_data_t *gps);
void mavlink_parser_set_sysid_filter(uint8_t sysid);

#endif
