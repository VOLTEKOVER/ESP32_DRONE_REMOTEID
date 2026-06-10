# Changelog

## [0.1.0-dev] — 2026-06-10

### Added
- Initial project structure with `components/esp_remote_id/`
- Three protocol parsers: MAVLink, MSP, NMEA
- Protocol auto-detection on UART (50ms timeout)
- WiFi Beacon TX via `esp_wifi_80211_tx`
- BLE 4.0 Legacy + BLE 5.0 Coded PHY advertising
- 26 configurable parameters stored in NVS
- Web configuration UI with REST API (`/api/config`, `/api/status`, `/api/reset`)
- OTA firmware update via web interface
- Factory reset via web interface
- WebSerial-based flashing page (`docs/index.html`)
- Standalone config page (`docs/config.html`) identical to embedded web UI
- GitHub Pages site at `https://valeriocomo.github.io/ESP32_DRONE_ID/`
- Custom partition table for OTA dual-slot updates
- GitHub Actions CI for automated builds (ESP32, ESP32-S3, ESP32-C3)

### Known Issues
- ESP-IDF build environment not yet set up locally
- BLE 5.0 coded PHY untested on real hardware
- Wi-Fi NAN support disabled (beta)
