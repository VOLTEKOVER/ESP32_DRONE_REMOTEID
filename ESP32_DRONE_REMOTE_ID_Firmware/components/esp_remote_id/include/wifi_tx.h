#ifndef WIFI_TX_H
#define WIFI_TX_H

#include "esp_remote_id.h"

void wifi_tx_init(void);
bool wifi_tx_transmit(rid_gps_data_t *gps, rid_identity_t *identity);

#endif
