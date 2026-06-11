# ESP Remote ID — Roadmap

## Vision
Universal ESP32 module that transmits Remote ID (OpenDroneID) for **any** drone, reading GPS data from the Flight Controller via **MAVLink**, **MSP**, or directly from the **GPS** (clone). Configurable via web.

---

## Architecture

```
                    ┌──────────────────────────────────────────┐
                    │              ESP Remote ID               │
                    │                                          │
  FC ──UART──→  ProtocolDetect  ──→  Parser  ──→  ODID Packer
                    │                  │               │
                    │            ┌─────┴─────┐         │
                    │         MAVLink  MSP  NMEA       │
                    │                                  │
                    │        ┌──────────────────┐      │
                    │        │  TX Backend       │      │
                    │        │  WiFi Beacon      │      │
                    │        │  BLE 4.0 Legacy   │      │
                    │        │  BLE 5.0 Coded PHY│      │
                    │        └──────────────────┘      │
                    │                                  │
                    │    Web Config ←→ NVS Storage     │
                    └──────────────────────────────────────────┘
```

---

## Implementation Phases

### PHASE 1 — Project skeleton
- [x] Base ESP-IDF repository
- [x] Create `esp_remote_id/` component
- [x] Folder structure and CMakeLists
- [x] Copy opendroneid-core-c (Intel) + mavlink v2 library

### PHASE 2 — Protocol parsing
- [x] **Autodetect**: auto-detect MAVLink/MSP/NMEA on UART
- [x] **NMEA Parser**: $GPGGA (lat/lon/alt/fix), $GPRMC (speed/status), $GPVTG (heading)
- [x] **MSP Parser**: MSP_RAW_GPS (106), MSP_ATTITUDE (108) for Betaflight/INAV
- [x] **MAVLink Parser**: GLOBAL_POSITION_INT, GPS_RAW_INT, VFR_HUD, ATTITUDE

### PHASE 3 — OpenDroneID Transmission
- [x] **WiFi Beacon**: 802.11 beacon frame with ODID message via `esp_wifi_80211_tx`
- [ ] **WiFi NAN**: Neighbor Awareness Networking action frames (beta, disabled)
- [x] **BLE 4.0**: legacy non-connectable advertising (non-configured raw adv)
- [ ] **BLE 5.0**: coded PHY Long Range (esp_ble_gap_ext_adv — needs HW verification)
- [x] **OpenDroneID Packer**: Intel opendroneid.c (odid_message_build_pack)

### PHASE 4 — Web Configuration
- [x] HTTP web server on ESP32 AP (port 80)
- [x] NVS parameter storage (26 parameters)
- [x] Full config page: Identity, Transmission, WiFi AP, System
- [x] Real-time status page (GPS fix, transmission counts)
- [x] OTA firmware update via web
- [x] Factory reset via web

### PHASE 5 — Integration
- [x] Main loop: init → autodetect → parse → transmit
- [x] OTA partition (dual-slot) for web updates
- [x] CI/CD: GitHub Actions for automatic build on ESP32/S3/C3
- [ ] Health check and heartbeat
- [ ] Build & test on real HW
- [ ] Test OTA update

---

## File Structure

```
components/esp_remote_id/
├── CMakeLists.txt
├── include/
│   ├── esp_remote_id.h          # Main header (all types)
│   ├── protocol_detect.h
│   ├── mavlink_parser.h
│   ├── msp_parser.h
│   ├── nmea_parser.h
│   ├── wifi_tx.h
│   ├── ble_tx.h
│   ├── web_config.h
│   └── nvs_storage.h
├── mavlink/                    # MAVLink C Library v2 (from ArduRemoteID)
│   ├── common/
│   ├── minimal/
│   └── ...
└── src/
    ├── esp_remote_id.c          # Main loop + config/state
    ├── protocol_detect.c        # Protocol auto-detect
    ├── mavlink_parser.c         # MAVLink GPS message parsing
    ├── msp_parser.c             # MSP parsing (Betaflight/INAV)
    ├── nmea_parser.c            # NMEA parsing (GPS clone)
    ├── wifi_tx.c                # WiFi Beacon TX (Intel odid_wifi_build)
    ├── ble_tx.c                 # BLE 4.0 + BLE 5.0 advertising
    ├── web_config.c             # HTTP server + REST API + OTA
    └── nvs_storage.c            # NVS parameter persistence
```

### Dependencies from Intel opendroneid-core-c

| Source File | Provides |
|---|---|
| `libopendroneid/opendroneid.c` + `.h` | ODID message encode/decode |
| `libopendroneid/wifi.c` | WiFi 802.11 beacon/NAN frames + message_pack |
| `libmav2odid/mav2odid.c` + `.h` | MAVLink ←→ ODID conversion |
| `mavlink_c_library_v2/` | MAVLink v2 headers (common, minimal, ardupilotmega) |

### Written from scratch (new, not in ArduRemoteID)

| File | Why |
|---|---|
| `msp_parser.c` | Betaflight/INAV use MSP, not supported by ArduRemoteID |
| `nmea_parser.c` | Direct GPS clone reading. New feature. |
| `protocol_detect.c` | Incoming protocol auto-detect. New feature. |

---

## Configurable Parameters (26 total)

| Parameter | Type | Default | Range |
|---|---|---|---|
| UAS ID | string | "ESP32-RID-001" | 20 char |
| ID Type | uint8 | 1 (Serial) | 0-4 |
| UA Type | uint8 | 1 (Aeroplane) | 0-15 |
| Operator ID | string | "OP-UNKNOWN" | 20 char |
| UAS ID 2 | string | "" | 20 char |
| ID Type 2 | uint8 | 0 (None) | 0-4 |
| UA Type 2 | uint8 | 0 (None) | 0-15 |
| Baud Rate | uint32 | 57600 | 9600-921600 |
| WiFi Channel | uint8 | 6 | 1-13 |
| WiFi Power | float | 20 dBm | 2-20 dBm |
| WiFi Beacon Rate | float | 1.0 Hz | 0-5 Hz |
| WiFi NAN Rate | float | 0 (off) | 0-5 Hz |
| BLE 4.0 Rate | float | 1.0 Hz | 0-5 Hz |
| BLE 4.0 Power | float | 18 dBm | -27..18 dBm |
| BLE 5.0 Rate | float | 1.0 Hz | 0-5 Hz |
| BLE 5.0 Power | float | 18 dBm | -27..18 dBm |
| WiFi SSID | string | "ESP-RID" | 20 char |
| WiFi Password | string | "" (open) | 20 char |
| Web Server | uint8 | 1 (on) | 0-1 |
| MAVLink SysID | uint8 | 0 (auto) | 0-254 |
| Broadcast without GPS | uint8 | 1 (on) | 0-1 |
| Lock Level | int8 | 0 | -1..2 |
| Options bitmask | uint8 | 0 | 0-7 |
| Public Key 1-5 | string | "" | 64 char |

---

## Important Notes

- **Build**: `idf.py build flash monitor` (requires ESP-IDF v6.0.1 in `C:\esp\v6.0.1\`); or via GitHub Actions (push to main)
- **Config**: Open `http://192.168.4.1` after connecting WiFi to the "ESP-RID" AP
- **OTA**: Upload firmware .bin from the web page (`/ota`); custom `partitions.csv` (dual-slot)
- **CI**: Workflow `.github/workflows/build.yml` builds for ESP32, ESP32-S3, ESP32-C3 and creates nightly release
- **Antenna**: Use module with U.FL connector (carbon shields PCB antenna)
- **GPS split resistor**: Place 1kΩ series resistor on the branch toward ESP32
- **BLE 5.0**: Requires HW with Coded PHY support (ESP32-C3, ESP32-S3)
