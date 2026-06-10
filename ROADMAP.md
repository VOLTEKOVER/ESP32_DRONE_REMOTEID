# ESP Remote ID — Roadmap

## Visione
Modulo ESP32 universale che trasmette Remote ID (OpenDroneID) per **qualsiasi** drone, leggendo i dati GPS dalla Flight Controller via **MAVLink**, **MSP**, o direttamente dal **GPS** (clone). Configurabile via web.

---

## Architettura

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

## Fasi di Implementazione

### FASE 1 — Scheletro del progetto
- [x] Repository base con ESP-IDF
- [x] Creare componente `esp_remote_id/`
- [x] Struttura cartelle e CMakeLists
- [x] Copiare libreria opendroneid-core-c (Intel) + mavlink v2

### FASE 2 — Parsing protocolli
- [x] **Autodetect**: riconoscere automaticamente MAVLink/MSP/NMEA su UART
- [x] **Parser NMEA**: $GPGGA (lat/lon/alt/fix), $GPRMC (speed/status), $GPVTG (heading)
- [x] **Parser MSP**: MSP_RAW_GPS (106), MSP_ATTITUDE (108) per Betaflight/INAV
- [x] **Parser MAVLink**: GLOBAL_POSITION_INT, GPS_RAW_INT, VFR_HUD, ATTITUDE

### FASE 3 — Trasmissione OpenDroneID
- [x] **WiFi Beacon**: frame 802.11 beacon con messaggio ODID via `esp_wifi_80211_tx`
- [ ] **WiFi NAN**: Neighbor Awareness Networking action frames (beta, disabilitato)
- [x] **BLE 4.0**: legacy advertising non-connectable (non-configured raw adv)
- [ ] **BLE 5.0**: coded PHY Long Range (esp_ble_gap_ext_adv — da verificare su HW)
- [x] **Packer OpenDroneID**: Intel opendroneid.c (odid_message_build_pack)

### FASE 4 — Configurazione Web
- [x] Web server HTTP su ESP32 AP (porta 80)
- [x] Salvataggio parametri in NVS (26 parametri)
- [x] Pagina configurazione completa: Identità, Trasmissione, WiFi AP, Sistema
- [x] Pagina stato实时 (GPS fix, conteggi trasmissione)
- [x] Aggiornamento firmware OTA via web
- [x] Reset fabbrica via web

### FASE 5 — Integrazione
- [x] Main loop: init → autodetect → parse → transmit
- [x] Partizione OTA (dual-slot) per aggiornamenti via web
- [x] CI/CD: GitHub Actions per build automatica su ESP32/S3/C3
- [ ] Health check e heartbeat
- [ ] Build & test su HW reale
- [ ] Test OTA update

---

## Struttura File

```
components/esp_remote_id/
├── CMakeLists.txt
├── include/
│   ├── esp_remote_id.h         # Header principale (tutti i tipi)
│   ├── protocol_detect.h
│   ├── mavlink_parser.h
│   ├── msp_parser.h
│   ├── nmea_parser.h
│   ├── wifi_tx.h
│   ├── ble_tx.h
│   ├── web_config.h
│   └── nvs_storage.h
├── mavlink/                    # MAVLink C Library v2 (da ArduRemoteID)
│   ├── common/
│   ├── minimal/
│   └── ...
└── src/
    ├── esp_remote_id.c          # Loop principale + config/state
    ├── protocol_detect.c        # Autodetect protocollo
    ├── mavlink_parser.c         # Parsing messaggi MAVLink GPS
    ├── msp_parser.c             # Parsing MSP (Betaflight/INAV)
    ├── nmea_parser.c            # Parsing NMEA (GPS clone)
    ├── wifi_tx.c                # WiFi Beacon TX (Intel odid_wifi_build)
    ├── ble_tx.c                 # BLE 4.0 + BLE 5.0 advertising
    ├── web_config.c             # HTTP server + REST API + OTA
    └── nvs_storage.c            # NVS persistenza parametri
```

### Dipendenze da Intel opendroneid-core-c

| File sorgente | Cosa fornisce |
|---|---|
| `libopendroneid/opendroneid.c` + `.h` | Codifica/decodifica messaggi ODID |
| `libopendroneid/wifi.c` | Frame WiFi 802.11 beacon/NAN + message_pack |
| `libmav2odid/mav2odid.c` + `.h` | Conversione MAVLink ←→ ODID |
| `mavlink_c_library_v2/` | MAVLink v2 headers (common, minimal, ardupilotmega) |

### Cosa SCRITTO da zero (nuovo, non in ArduRemoteID)

| File | Perché |
|---|---|
| `msp_parser.c` | Betaflight/INAV usano MSP, non supportato da ArduRemoteID |
| `nmea_parser.c` | Lettura diretta GPS clone. Nuova feature. |
| `protocol_detect.c` | Autodetect del protocollo in ingresso. Nuova feature. |

---

## Parametri Configurabili (26 totali)

| Parametro | Tipo | Default | Range |
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
| WiFi Password | string | "" (aperto) | 20 char |
| Web Server | uint8 | 1 (on) | 0-1 |
| MAVLink SysID | uint8 | 0 (auto) | 0-254 |
| Broadcast senza GPS | uint8 | 1 (on) | 0-1 |
| Lock Level | int8 | 0 | -1..2 |
| Options bitmask | uint8 | 0 | 0-7 |
| Public Key 1-5 | string | "" | 64 char |

---

## Note Importanti

- **Build**: `idf.py build flash monitor` (richiede ESP-IDF v6.0.1 in `C:\esp\v6.0.1\`); oppure via GitHub Actions (push su main)
- **Config**: Aprire `http://192.168.4.1` dopo aver connesso il WiFi all'AP "ESP-RID"
- **OTA**: Caricare firmware .bin dalla pagina web (`/ota`); partizione custom `partitions.csv` (dual-slot)
- **CI**: Workflow `.github/workflows/build.yml` compila per ESP32, ESP32-S3, ESP32-C3 e crea nightly release
- **Antenna**: Usare modulo con connettore U.FL (carbonio scherma antenna PCB)
- **Resistenza sdoppio GPS**: Mettere 1kΩ in serie sul ramo verso l'ESP32
- **BLE 5.0**: Richiede HW con supporto Coded PHY (ESP32-C3, ESP32-S3)
