#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include "esp_remote_id.h"

void nmea_parser_init(void);
bool nmea_parser_get(rid_gps_data_t *gps);

#endif
