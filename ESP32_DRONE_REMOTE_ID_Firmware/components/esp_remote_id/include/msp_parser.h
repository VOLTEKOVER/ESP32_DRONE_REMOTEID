#ifndef MSP_PARSER_H
#define MSP_PARSER_H

#include "esp_remote_id.h"

void msp_parser_init(void);
bool msp_parser_get(rid_gps_data_t *gps);

#endif
