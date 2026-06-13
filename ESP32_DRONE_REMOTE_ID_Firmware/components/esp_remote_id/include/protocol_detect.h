#ifndef PROTOCOL_DETECT_H
#define PROTOCOL_DETECT_H

#include "esp_remote_id.h"

void protocol_detect_init(void);
void protocol_detect_reinit(uint32_t baud);
rid_protocol_t protocol_detect_auto(void);

#endif
