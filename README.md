# ESP Remote ID [BETA]

Universal **ASTM F3411-22a / ASD-STAN prEN 4709-002** Open DroneID transmitter for ESP32.
Reads GPS data from any flight controller (MAVLink, MSP, NMEA) and broadcasts via **WiFi Beacon** and **BLE 4.0/5.0**.

> **Full documentation and web installer:** https://valeriocomo.github.io/ESP32_DRONE_ID/

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%2FS3%2FC3-green.svg)](https://www.espressif.com/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

## Quick Start

1. Connect ESP32 to your computer via USB
2. Open https://valeriocomo.github.io/ESP32_DRONE_ID/
3. Click **Install** and select the serial port
4. Power from battery, connect to WiFi **ESP-RID**
5. Configure at **http://192.168.4.1**

## Features

- **Triple protocol support** — MAVLink (ArduPilot), MSP (Betaflight/INAV), NMEA (any GPS)
- **Auto-detect** — protocol detected automatically on UART within 50 ms
- **Dual radio** — WiFi Beacon + BLE 4.0 + BLE 5.0 Coded PHY (S3/C3)
- **No second GPS** — reuses existing flight controller GPS data
- **Web configuration** — built-in WiFi AP at 192.168.4.1 with REST API
- **26 parameters** — all ArduRemoteID equivalents (UAS ID, rates, power, keys, etc.)
- **OTA updates** — firmware update via browser, no USB needed
- **Browser flashing** — WebSerial, no toolchain required

## Hardware

| ESP32 Pin | Connect To |
|-----------|-----------|
| GPIO16 (UART2 RX) | FC TX |
| GND | FC GND |
| 5V | FC BEC or USB |

For NMEA clone (tap GPS TX), add a **1 kΩ series resistor** on the line.

## Building from Source

Requires ESP-IDF v6.0.1:

```bash
git clone https://github.com/valeriocomo/ESP32_DRONE_ID.git
cd ESP32_DRONE_ID
. $HOME/esp/v6.0.1/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

```
ESP32_DRONE_ID/
├── main/                    # Entry point
├── components/
│   └── esp_remote_id/       # Main component
│       ├── include/         # Headers
│       ├── src/             # Core, parsers, TX, web server
│       ├── mavlink/         # MAVLink C library
│       └── webui/           # Embedded config HTML
├── docs/                    # GitHub Pages site
│   ├── index.html           # Wiki + installer
│   ├── config.html          # Config page (identical to webui)
│   └── manifest.json        # WebSerial manifest
└── images/                  # Logo files
```

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE).
