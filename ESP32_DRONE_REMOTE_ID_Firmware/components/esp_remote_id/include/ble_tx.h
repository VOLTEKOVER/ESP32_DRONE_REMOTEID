#ifndef BLE_TX_H
#define BLE_TX_H

#include "esp_remote_id.h"

void ble_tx_init(void);
bool ble_tx_transmit_legacy(rid_gps_data_t *gps, rid_identity_t *identity);
bool ble_tx_transmit_lr(rid_gps_data_t *gps, rid_identity_t *identity);

#endif
