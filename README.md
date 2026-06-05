# ESP32 Drone ID

Sistema di identificazione remota per droni (Remote ID) basato su **ESP32** e **OpenDroneID**, conforme ai standard internazionali EU/US. 

**Progetto in riscrittura con ESP-IDF** per:
- Migliore performance e stabilità
- Compatibilità universale con tutti i flight controller
- Configurazione grafica via interfaccia web
- Flashing firmware diretto dal browser

Il sistema trasmette l'identità del drone via Bluetooth, WiFi Beacon e NAN, consentendo il rilevamento remoto da dispositivi mobili e stazioni di controllo.

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)
![Status: Active Development](https://img.shields.io/badge/Status-Active%20Development-orange.svg)

## 🎯 Features

- **OpenDroneID Support**
  - Full EU/US compatible implementation
  - Multi-region regulatory support
  - Extensible architecture for additional standards

- **Multi-Protocol Broadcasting**
  - Bluetooth Low Energy (BLE) 4.0
  - WiFi Beacon
  - WiFi Neighbor Awareness Networking (NAN)

- **Web-Based Configuration Tool**
  - Graphical GUI for easy ID setup
  - Real-time firmware flashing via USB
  - No external tools needed (Chrome/Edge compatible)
  - Configuration save/load support

- **Hardware Optimized**
  - Runs on affordable ESP32 dev boards
  - Dual-core processing support
  - Minimal power consumption

- **Flexible Configuration**
  - Easy ID parameter setup
  - Support for multiple transmission modes simultaneously

## 🚀 Quick Start

### Requirements
- ESP32 Development Board
- Platform.io or Arduino IDE
- OpenDroneID core library (for id_open module)

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/VOLTEKOVER/ESP32_DRONE_ID.git
   cd ESP32_DRONE_ID
   ```

2. **Install dependencies**
   ```bash
   # For id_open module, download opendroneid core files
   # Copy opendroneid.c, opendroneid.h, odid_wifi.h, and wifi.c
   # from https://github.com/opendroneid/opendroneid-core-c
   # into the id_open/ directory
   ```

3. **Compile and upload**
   ```bash
   # Using Arduino IDE or Platform.io
   ```

## 📦 Project Structure

### id_open
Complete OpenDroneID implementation wrapper for ESP32.

**Supported Protocols:**
- BLE 4.0
- WiFi Beacon
- WiFi NAN

**Compatibility:** OpenDroneID release 2.0+

**Note:** Known ESP32 limitation - some devices may experience reboots when both WiFi and Bluetooth are enabled simultaneously. If this occurs, use one protocol at a time.

## 🛠️ Web Flasher & Configuration Tool

Use `web_flasher.html` for a complete graphical interface to:
- Configure drone IDs and parameters
- Flash firmware directly from your browser
- Monitor flashing progress
- Save/load configurations

**See [WEB_FLASHER_README.md](WEB_FLASHER_README.md) for detailed instructions.**

## ⚠️ Regulatory Compliance Notice

When developing remote IDs for use in regulated airspace:

1. **ANSI/CTA Serial Numbers** - US and EU regulations require specific serial number formats
2. **Tamper Resistance** - FAA and EASA mandate tamper-resistant implementations
3. **Local Regulations** - Always verify compliance with your local aviation authority

Refer to:
- [OpenDroneID Regulatory Overview](https://github.com/opendroneid/opendroneid-core-c)
- [FAA Remote ID Requirements](https://www.faa.gov/uas/remote_id/)
- [EASA Remote ID Guidelines](https://www.easa.europa.eu/)

## 📚 Documentation

- [OpenDroneID Specification](https://github.com/opendroneid/opendroneid-core-c)
- [ESP32 Hardware Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- Module-specific READMEs in respective directories

## 🤝 Contributing

Contributions are welcome! Areas needing help:

- [ ] ESP-IDF migration and testing
- [ ] Graphical configuration tool development
- [ ] Regional regulatory compliance modules
- [ ] Hardware compatibility testing
- [ ] Documentation and examples

## 📋 Supported Hardware

- **ESP32-WROOM** (Recommended)
- **ESP32-WROVER**
- Any standard ESP32 dev board with WiFi + Bluetooth

## 📄 License

This project is licensed under the MIT License - see LICENSE file for details.

## 🔗 Related Projects

- [OpenDroneID Core (C)](https://github.com/opendroneid/opendroneid-core-c)
- [Gendarmerie Nationale Drone Reception System](https://github.com/GendarmerieNationale/ReceptionInfoDrone)
- [nRF52840 Remote ID (Bluetooth 5)](https://github.com/sxjack/remote_id_bt5)

## 📞 Support & Issues

- Report bugs and issues via GitHub Issues
- Check existing documentation and issues before submitting
- Provide hardware details and reproduction steps with bug reports

---

**Last Updated:** 2026
**Maintainer:** VOLTEKOVER
