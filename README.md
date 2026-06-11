<p align="center">
  <img src="images/logo%20con%20scritta.svg" alt="ESP Remote ID" width="400">
</p>

[![Build](https://github.com/valeriocomo/ESP32_DRONE_ID/actions/workflows/build.yml/badge.svg)](https://github.com/valeriocomo/ESP32_DRONE_ID/actions/workflows/build.yml)
[![Pages](https://github.com/valeriocomo/ESP32_DRONE_ID/actions/workflows/pages.yml/badge.svg)](https://github.com/valeriocomo/ESP32_DRONE_ID/actions/workflows/pages.yml)
[![Platform](https://img.shields.io/badge/ESP32%2FS3%2FC3-000)](https://www.espressif.com/)
[![License: GPL v3](https://img.shields.io/badge/GPLv3-blue)](LICENSE)

Universal **ASTM F3411-22a / ASD-STAN prEN 4709-002** Open DroneID transmitter for ESP32. Reads GPS from any flight controller (MAVLink, MSP, NMEA) and broadcasts via **WiFi Beacon** + **BLE 4.0/5.0**.

> **Web installer & docs:** https://valeriocomo.github.io/ESP32_DRONE_ID/

---

## Quick Start

1. Connect ESP32 to USB
2. Open https://valeriocomo.github.io/ESP32_DRONE_ID/
3. Click **Install**, select port
4. Power from battery, connect WiFi to **ESP-RID**
5. Configure at **http://192.168.4.1**

## Features

- **Triple protocol** — MAVLink, MSP, NMEA + auto-detect (50 ms)
- **Dual radio** — WiFi Beacon + BLE 4.0 + BLE 5.0 Coded PHY (S3/C3)
- **No second GPS** — reuses existing FC GPS data
- **26 config params** — UAS ID, rates, power, public keys, etc.
- **Web UI** — built-in AP with REST API + OTA updates
- **Browser flashing** — WebSerial, no toolchain needed

## Hardware

| ESP32 | Connect to |
|-------|-----------|
| GPIO16 (UART2 RX) | FC TX |
| GND | FC GND |
| 5V | FC BEC or USB |

For NMEA clone (tap GPS TX): add **1 kΩ series resistor**.

## Build

### GitHub Actions (automatic)
Push to `main` → builds for ESP32/S3/C3. See [Actions](https://github.com/valeriocomo/ESP32_DRONE_ID/actions).

### Locally (ESP-IDF v6.0.1)

```bash
git clone https://github.com/valeriocomo/ESP32_DRONE_ID.git
cd ESP32_DRONE_ID
. $HOME/esp/v6.0.1/esp-idf/export.sh
idf.py set-target esp32
idf.py build flash monitor
```

## Structure

```
ESP32_DRONE_ID/
├── main/                    # Entry point
├── components/esp_remote_id/# Core component (protocol detect, web UI, config)
│   └── webui/               # Embedded web interface (config.html)
├── docs/                    # GitHub Pages site (docs, demo, install)
│   └── config(demo).html    # Standalone config simulator
├── images/                  # Logo assets — <img src="images/logo.svg" width="18" alt="icon"> dragonfly SVG
└── .github/workflows/      # CI/CD
```

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
