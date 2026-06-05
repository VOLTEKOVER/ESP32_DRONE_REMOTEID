/*
 * OpenDroneID wrapper header for ESP-IDF project
 * 
 * This header provides access to the OpenDroneID libraries
 * located in the parent id_open directory.
 */

#ifndef OPENDRONEID_H
#define OPENDRONEID_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for the OpenDroneID library structures and functions
// Include the actual definitions from id_open

#include "../../id_open/id_open.h"

// Additional decoding functions
void odid_message_process_pack(void *UAS_data, uint8_t *payload, int length);
int odid_wifi_receive_message_pack_nan_action_frame(void *UAS_data, char *mac, 
                                                     uint8_t *payload, int length);

#ifdef __cplusplus
}
#endif

#endif // OPENDRONEID_H
