<p align="center">
  <img src="docs/images/logo%20con%20scritta.svg" alt="ESP DRONE REMOTEID" width="420">
</p>

<p align="center">
  <a href="https://github.com/VOLTEKOVER/ESP_DRONE_REMOTEID/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/VOLTEKOVER/ESP_DRONE_REMOTEID/build.yml?logo=github&label=build" alt="Build"></a>
  <a href="https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/"><img src="https://img.shields.io/badge/BETA-000?logo=esphome&color=f9a825" alt="BETA"></a>
  <a href="https://github.com/VOLTEKOVER/ESP_DRONE_REMOTEID/releases"><img src="https://img.shields.io/github/v/release/VOLTEKOVER/ESP_DRONE_REMOTEID?include_prereleases&logo=github&label=version" alt="Release"></a>
  <a href="https://www.espressif.com/"><img src="https://img.shields.io/badge/ESP32%20|%20S3%20|%20C6-000?logo=espressif" alt="Platform"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/VOLTEKOVER/ESP_DRONE_REMOTEID?color=blue" alt="License"></a>
</p>

<p align="center">
  <img src="docs/images/ardupilot_logo.webp" height="28" alt="ArduPilot">&nbsp;&nbsp;
  <img src="docs/images/betaflight_logo.svg" height="28" alt="Betaflight">&nbsp;&nbsp;
  <img src="docs/images/inav_logo.png" height="28" alt="INAV">
</p>

<p align="center">
  <b>ASTM F3411-22a</b> Open DroneID transmitter.<br>
  Parses MAVLink · MSP · NMEA · DroneCAN · broadcasts via <b>WiFi Beacon + NAN + BLE 4.0/5.0</b>.<br>
  Features Kalman filter, Ed25519 auth, OTA, MAVLink ARM_STATUS, WS2812 LED, GPIO lighting.<br>
  Targets: ESP32, ESP32-S3, ESP32-C6.<br>
  Companion ground station: <b>RID Hub</b> (Electron, no Python).<br>
See the <a href="https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/">online wiki</a> for wiring, flashing, and configuration.
</p>

<p align="center">
  <a href="https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/"><b>Wiki & Demo</b></a>
</p>

---

## Quick Start

| Step | Action |
|------|--------|
| 1 | Connect ESP32 to USB |
| 2 | Open [VOLTEKOVER.github.io/ESP_DRONE_REMOTEID](https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/) |
| 3 | Select your chip (**ESP32** / **ESP32-S3** / **ESP32-C6**) |
| 4 | Click **Install**, pick serial port |
| 5 | Connect to WiFi **ESP-RID** |
| 6 | Configure at `http://192.168.4.1` |

> <a href="https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/config(demo).html">Try the live demo</a> — full web UI simulation without hardware.

## Features

| | |
|---|---|
| **Protocols** | MAVLink (incl. ArduPilot ODID), MSP, NMEA, DroneCAN — auto-detect |
| **Radio** | WiFi Beacon + WiFi NAN + BLE 4.0 + BLE 5.0 Coded PHY |
| **GPS source** | Flight controller (MAVLink/MSP/NMEA/DroneCAN) or direct GPS module |
| **Position filter** | Kalman 1D×3 (lat/lon/alt) with velocity prediction, 3s timeout |
| **Authentication** | Ed25519 signing (F3411-22a), 4 auth pages per cycle |
| **MAVLink out** | HEARTBEAT (MAV_TYPE_ODID) + ARM_STATUS + operator-location loop |
| **OTA updates** | Wi-Fi AP mode HTTP server with SHA-256 verification |
| **Web UI** | Built-in AP + REST API + live telemetry + CLI terminal |
| **Config** | 50+ parameters: UAS ID, rates, power, public keys, auth, lock levels, lighting |
| **Security** | 3 lock levels (normal / ECDSA signed / eFuse permanent) |
| **Lighting** | WS2812 RGB LED (RMT) + 5-ch GPIO lighting, 6 patterns |
| **Demo** | [Offline demo page](https://VOLTEKOVER.github.io/ESP_DRONE_REMOTEID/config(demo).html) |

## Communication Protocols

| Category | Protocol | Direction | Details |
|---|---|---|---|
| **Input** (position) | **MAVLink v2** | FC → ESP | ArduPilot/PX4: `GPS_RAW_INT`, `GLOBAL_POSITION_INT`, `HEARTBEAT`, `OPEN_DRONE_ID_*` |
| | **MSP** | FC → ESP | Betaflight/iNAV: `MSP_RAW_GPS` (106), `MSP_ATTITUDE` (108), `MSP_STATUS` (101) |
| | **NMEA 0183** | GPS → ESP | `$GPGGA`, `$GNGGA`, `$GPRMC`, `$GNRMC`, `$GPVTG`, `$GNVTG` |
| | **DroneCAN / CAN bus** | FC → ESP | TWAI decode `Fix2` lat/lon/alt/speed/heading, AHRS solution |
| **Output** (broadcast RID) | **WiFi Beacon** (802.11 mgmt) | ESP → Air | ASTM F3411-22a over WiFi Beacon |
| | **WiFi NAN** | ESP → Air | Service Discovery Frame NAN |
| | **BLE 4.0 Legacy** | ESP → Air | Bluetooth 4.0 advertising, non-connectable |
| | **BLE 5.0 Coded PHY** (LR) | ESP → Air | Bluetooth 5.0 Long Range, `PHY_CODED` (S3/C6) |
| **Config/control** | **HTTP / Web UI** | Browser ↔ ESP | AP-mode `192.168.4.1`, REST API, live telemetry |
| | **Serial CLI** (UART0) | Terminal ↔ ESP | 15 commands: `help`, `status`, `config`, `patrol`, `transmit`, `kalman`, ... |
| | **NVS storage** | ESP → Flash | Persistent configuration storage |
| **Config/control** | **GPIO Lighting** | ESP → LEDs | 5-ch configurable patterns, phase offsets |
| | **WS2812 RGB LED** | ESP → LED | Addressable RGB via RMT, HSV/RGB/brightness |
| **Future** | **ESP-NOW mesh** | Drone ↔ Drone | RID relay between drones (TODO) |
| | **LoRa SX1262** | ESP → Air | Backup RID 10+ km (TODO) |

## Hardware

| ESP32 Pin | Connect to |
|-----------|-----------|
| `GPIO16` (UART2 RX) | FC TX (output) or GPS TX |
| `GND` | FC / GPS GND |
| `5V` (VIN) | FC BEC or USB |

> **&#x26a0;** NMEA clone (tap GPS TX): add **1 k&Omega; series resistor** on the tap line.

### Recommended board: Seeed XIAO ESP32-C6

Small & most innovative option:
- **ESP32-C6**: WiFi 6 (OFDMA), BT 5.3, 802.15.4
- **21 &times; 17.8 mm**, USB-C, LiPo charger
- **U.FL** + onboard ceramic antenna (selectable via SW)
- Stackable with **L76K GNSS** module (multi-constellation)
- Total cost: **~$15**, zero soldering

> See [`docs/prototype_bom.md`](docs/prototype_bom.md) for full BOM.

## Build

Push to `main` &rarr; automatic build for ESP32, ESP32-S3, ESP32-C6 via GitHub Actions.  
[Latest builds](https://github.com/VOLTEKOVER/ESP_DRONE_REMOTEID/actions/workflows/build.yml)

### Local (ESP-IDF v6.0.1)

```bash
git clone https://github.com/VOLTEKOVER/ESP_DRONE_REMOTEID.git
cd ESP_DRONE_REMOTEID
idf.py set-target esp32       # or esp32s3 / esp32c6
idf.py build
```

## Project Structure

```
ESP_DRONE_REMOTEID/
├── ESP32_DRONE_REMOTE_ID_Firmware/  # ESP-IDF project
│   ├── main/                        # app_main entry point
│   ├── components/esp_remote_id/    # Core: protocol detect, web UI, NVS config, Kalman, auth, OTA
│   │   ├── src/                     # 23 source files (ESP-IDF C)
│   │   ├── include/                 # 20 headers
│   │   │   ├── rid_kalman.h         # 1D×3 Kalman filter
│   │   │   ├── rid_auth.h           # Ed25519 auth signing
│   │   │   ├── rid_ota.h            # OTA update server
│   │   │   ├── rid_dronecan.h       # DroneCAN CAN bus input
│   │   │   ├── rid_mavlink_tx.h     # MAVLink heartbeat out
│   │   │   ├── rid_mavlink_usb.h    # MAVLink USB CDC transport
│   │   │   ├── led_ws2812.h         # WS2812 RGB LED RMT driver
│   │   │   └── rid_lighting.h       # 5-ch GPIO lighting
│   │   ├── webui/config.html        # Embedded web interface (~1520 lines)
│   │   └── mavlink/                 # MAVLink v2 dialect headers
│   └── partitions*.csv              # OTA partition layouts
├── RID_Hub/                         # Ground Station (Electron, no Python)
│   ├── main.js                      # Electron main process (IPC nativa)
│   ├── preload.js                   # contextBridge IPC esposto al renderer
│   ├── package.json                 # Dipendenze Electron + serialport
│   ├── src/                         # Core Node.js: decoder, tracker, capture
│   └── renderer/                    # Frontend: HTML/JS/CSS (Leaﬂet + Chart.js)
├── docs/                            # GitHub Pages site
│   ├── index.html                   # WebSerial flasher + wiki
│   ├── guide.html                   # Technical wiki (sections 3–16)
│   ├── config(demo).html            # Offline config UI demo
│   ├── ~~innovation_diy.md~~ ❌       # (Deleted — merged into todolist/softwarestatus.md)
│   ├── prototype_bom.md             # Recommended hardware BOM
│   ├── manifest.json                # Firmware manifest (auto-generated by CI)
│   └── images/                      # Logo assets
├── .github/workflows/               # CI/CD (3-target matrix + release + Pages)
└── todolist/                        # Software status inventory
```

## License

```
   ESP Remote ID — Open DroneID Transmitter for ESP32
   Copyright (C) 2024 VOLTEKOVER

   Based on Intel Open Drone ID (https://github.com/opendroneid)
   Copyright (C) 2019-2023 Intel Corporation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```

### Apache License, Version 2.0

<details>
<summary>View full license text</summary>

```
                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS
```

</details>
