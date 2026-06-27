# ESP DRONE REMOTEID — Complete Software Status

> Last updated: 2026-06-22 (v3)
> Scope: all 70+ source files in repository (excluding build artifacts, __pycache__)

---

## `ESP32_DRONE_REMOTE_ID_Firmware/` — Firmware Root

### `CMakeLists.txt` (18 lines)
- Root CMake. Sets project name `esp_remote_id`, registers main component + `esp_remote_id`, includes `mavlink/` as an extra component directory.
- **OK.** No changes needed.

### `sdkconfig.defaults` (1 line)
- `CONFIG_BT_ENABLED=y` — minimal baseline; CI overrides per target.
- ✅ Now includes `CONFIG_MBEDTLS_SHA256_ENABLED=y` for OTA SHA-256 validation.

### `sdkconfig` (3461 lines, generated)
- ✅ Ignored via `.gitignore`, not tracked in git. CI regenerates it fresh per target.
- Informational reference for local builds only.

### `partitions.csv` (7 lines)
- 4 MB flash layout: nvs 20K + otadata 8K + phy_init 4K + factory 1920K + ota_0 1920K.
- **OK.** Standard OTA dual-slot.

### `partitions_2mb.csv` (7 lines)
- 2 MB flash layout: nvs 20K + otadata 8K + phy_init 4K + factory 960K + ota_0 960K.
- Used by ESP32-C2 in CI (`ESP_DRONE_REMOTE_ID_Firmware/build.yml`).
- **OK.**

### `idf_component.yml` (12 lines)
- ESP-IDF registry manifest. Version `1.0.0-beta`.
- ✅ Description fixed to `"ESP32 Remote ID Transmitter"` (was `"Scanner"`).
- **OK.**

---

### `main/CMakeLists.txt` (3 lines)
- Registers `main.c`, requires `esp_remote_id` component.
- **OK.**

### `main/Kconfig.projbuild`
- **Does not exist.** No main-level kconfig. All config is in `components/esp_remote_id/Kconfig.projbuild`.
- **OK** by design.

### `main/main.c` (90 lines)
- Entry point: `app_main()` → splash → `fix_mac_if_needed()` → `esp_rid_init()` → `esp_rid_start()`.
- ✅ Demo mode: if RID_OPT_DEMO enabled, starts GPS patrol simulation instead of waiting for FC.
- **OK.**

---

## `components/esp_remote_id/` — Core Component

### `CMakeLists.txt` (38 lines)
- Registers all .c files in `src/`. REQUIRES `nvs_flash`, `efuse`, `esp_wifi`, `esp_bt`, `mdns`, etc.
- ✅ Added `efuse` to REQUIRES for eFuse lock support.
- ✅ Added `src/cli.c` to SRCS (no external deps — raw UART REPL).
- **OK.**

### `Kconfig.projbuild` (27 lines)
- Menuconfig: `RID_LED_R_GPIO`, `RID_LED_G_GPIO`, `RID_LED_B_GPIO` (int, default -1/disbled).
- **OK.** Allows configuring RGB LED pins without code changes.

---

### `include/` — Headers

#### `esp_remote_id.h` (80 lines)
- Public API: `esp_rid_init()`, `esp_rid_start()`, `esp_rid_stop()`, callback typedef.
- Structs: `rid_config_t`, `rid_state_t`, `rid_gps_data_t`, `rid_identity_t`.
- Options flags: `RID_OPT_DEMO`, `RID_OPT_FORCE_ARM_OK`.
- **Gap:** `altitude_baro` in `rid_gps_data_t` — parsed by all parsers but unused in transmission.
- **Gap:** `RID_OPT_FORCE_ARM_OK` flag exists but may conflict with new `force_tx` logic.

#### `opendroneid.h` (762 lines)
- Intel Open Drone ID C library header: all ASTM enums, message types, encode/decode API.
- **OK.** Upstream library, no modifications needed.

#### `odid_wifi.h` (106 lines)
- IEEE 802.11 packed structs: management frame header, beacon, SSID, rates, vendor-specific IE, NAN attributes.
- `ODID_service_info` — message pack container.
- **OK.**

#### `wifi_tx.h` (30 lines)
- Declares: `wifi_tx_init()`, `wifi_tx_transmit()`, `wifi_tx_transmit_nan()`.
- **OK.**

#### `ble_tx.h` (20 lines)
- Declares: `ble_tx_init()`, `ble_tx_transmit_legacy()`, `ble_tx_transmit_lr()`.
- **OK.**

#### `mav2odid.h` (30 lines)
- Intel MAVLink-to-ODID conversion: `m2o_*` functions.
- ✅ Compiled and active via `mavlink_parser.c` include.
- **Gap:** Only the high-level `m2o_convert_*` wrappers are used; lower-level scheduling helpers are dead code.

#### `protocol_detect.h` (10 lines)
- Declares: `protocol_detect_init()`, `protocol_detect_get_protocol()`.
- **OK.**

#### `nmea_parser.h` (15 lines)
- Declares: `nmea_parser_init()`, `nmea_parser_get_data()`.
- **OK.**

#### `msp_parser.h` (15 lines)
- Declares: `msp_parser_init()`, `msp_parser_get_data()`.
- **OK.**

#### `mavlink_parser.h` (40 lines)
- Declares: `mavlink_parser_init()`, `mavlink_parser_get_gps()`, `mavlink_parser_get_identity()`.
- **OK.**

#### `web_config.h` (15 lines)
- Declares: `web_config_init()`.
- **OK.**

#### `nvs_storage.h` (20 lines)
- Declares: `nvs_storage_init()`, `nvs_storage_save()`, `nvs_storage_load()`, `nvs_storage_erase()`.
- **OK.**

#### `led_status.h` (30 lines)
- Declares: `led_status_init()`, `led_status_reconfigure()`, `led_status_set_state()`, `led_status_tx_flash()`, `led_status_tick()`.
- Enum: `rid_led_state_t` (BOOT, NO_GPS, GPS_OK, DEMO, LOCKED, OTA, ERROR).
- **OK.**

#### `rid_patrol.h` (10 lines)
- Declares: `rid_patrol_tick()`.
- **OK.**

#### `cli.h` (7 lines)
- Declares: `cli_init()`.
- **OK.**

#### `protocol_detect.h` (10 lines)
- **OK.**

---

### `src/` — Source Files

#### `esp_remote_id.c` (447 lines)
- Main orchestrator: init sequence, RTOS task loop, `update_transmissions()`, rate limiting.
- ✅ Rate limiting: `rate_allowed()` uses `esp_timer_get_time()`.
- ✅ WiFi NAN: called in `update_transmissions()`.
- ✅ GPS staleness: `g_state.gps_valid` cleared after 10s no-update.
- ✅ LED: `led_status_update()` called after each tx.
- ✅ `force_tx` — FIXED: outer condition now only checks `latitude != 0.0` (removed `fix_type >= 2` redundancy).
- ✅ Identity override: MAVLink ODID identity overwrites config identity when valid.
- ✅ CLI: calls `cli_init()` at end of init sequence.
- ✅ LED: uses new state machine API (`set_state` + `tx_flash` + `tick`).
- **OK.**

#### `wifi.c` (614 lines)
- IEEE 802.11 frame builder: NAN sync beacon, NAN action frame, legacy beacon, message pack.
- From Intel ODID library. Stable.
- **OK.**

#### `wifi_tx.c` (204 lines)
- `wifi_tx_init()`: SoftAP + promiscuous mode, random MAC generation.
- `wifi_tx_transmit()`: builds legacy beacon via `esp_wifi_80211_tx()`.
- `wifi_tx_transmit_nan()`: builds NAN action frame.
- ✅ Refactored: duplicate `g_uas_data` population → shared `populate_uas_data()`.
- **OK.**

#### `ble_tx.c` (183 lines)
- BLE 4.0 legacy advertising + BLE 5.0 Long Range (Coded PHY).
- ✅ `ble_tx_transmit_legacy()` uses `ADV_TYPE_SCAN_IND` for better compatibility.
- **Gap:** BLE 5.0 LR Coded PHY requires ESP32-S3/C3 hardware — marked Beta.

#### `web_config.c` (735 lines)
- HTTP server: serves `config.html`, REST API (`/api/config`, `/api/status`, `/api/command`, `/api/logs`), OTA endpoint.
- ✅ Lock level: basic check blocks destructive commands at level ≥ 1, OTA at level ≥ 2.
- ✅ eFuse burn: writes `"RID!"` magic to `EFUSE_BLK3` on transition to level 2; `get_lock_level()` reads eFuse.
- ✅ OTA SHA-256: `X-Expected-SHA256` header mandatory. Rejects missing/mismatched hash.
- ✅ Signature verification: Level ≥ 1 requires `X-Signature` header with ECDSA P-256 signature over SHA-256 hash of JSON body. Uses `mbedtls/pk.h` for key parsing (PEM, DER, or `PUBLIC_KEYV1:` base64) and PSA for hashing.
- ✅ Command signature verification: `/api/command` now also verifies `X-Signature` for restart/reboot/reset/factory commands when locked. `/api/reset` endpoint also accepts signatures (body signed: `"factory_reset"`). Returns `invalid_signature` instead of `locked`.

#### `cli.c` (317 lines)
- Raw UART REPL (no external deps: no esp_console, no linenoise).
- 14 commands + simple argument parser via `fgets()` + `strcmp` dispatch.
- Commands: `help`, `status`, `config`, `restart`/`reboot`, `reset`/`factory`, `protocol`, `heap`, `log_level`, `patrol`, `transmit`, `mac`, `uptime`.
- Spawns `cli_task` (4 KB stack) and returns immediately.
- **OK.**

#### `protocol_detect.c` (75 lines)
- UART auto-detect: `$M<` → MSP, `$G/$N` → NMEA, `0xFE/0xFD` → MAVLink.
- **OK.**

#### `nmea_parser.c` (136 lines)
- Parses GGA + RMC sentences. Extracts lat/lon/alt/speed/heading/fix/sat.
- ✅ `altitude_baro` populated via MSL altitude fallback.
- **OK.**

#### `msp_parser.c` (126 lines)
- Parses MSP_RAW_GPS (106), MSP_ATTITUDE (108), MSP_STATUS (101).
- ✅ `altitude_baro` populated via MSL altitude fallback.
- **OK.**

#### `mavlink_parser.c` (180 lines)
- Parses GLOBAL_POSITION_INT, GPS_RAW_INT, VFR_HUD, ATTITUDE, HEARTBEAT.
- ✅ AHRS2: supports ArduPilot secondary heading.
- ✅ OPEN_DRONE_ID_LOCATION: lat/lon/alt/vel/heading + altitude_baro.
- ✅ OPEN_DRONE_ID_BASIC_ID: UAS ID, id_type, ua_type.
- ✅ OPEN_DRONE_ID_OPERATOR_ID: operator_id.
- ✅ OPEN_DRONE_ID_SYSTEM: operator lat/lon for GPS fallback.
- ✅ `altitude_baro` from OPEN_DRONE_ID_LOCATION.
- ✅ `mavlink_parser_get_identity()` — returns parsed ODID identity.
- Dialect: `ardupilotmega/mavlink.h` (was `common/mavlink.h`).
- 5s GPS timeout, 10s identity timeout.
- **OK.**

#### `mav2odid.c` (636 lines)
- Intel MAVLink-to-ODID conversion library. Scheduling, buffer management, message conversion.
- ✅ Compiled and linked via `mavlink_parser.c` include.
- **Gap:** High-level scheduling logic unused; only `m2o_convert_*` wrappers reachable.

#### `opendroneid.c` (1477 lines)
- Intel ODID encode/decode library. All ASTM message types.
- **OK.** Upstream stable.

#### `nvs_storage.c` (174 lines)
- NVS persistence for `rid_config_t` in namespace `esp_rid`.
- **OK.**

#### `led_status.c` (199 lines)
- RGB LED via 3 configurable GPIOs (Kconfig) with LEDC PWM (5kHz, 8-bit).
- State machine: 7 states with color + pattern encoding.
- Patterns: SOLID, BLINK_1Hz, BLINK_4Hz, DOUBLE_BLINK, PULSE (triangular 2s), RAINBOW (cycling).
- TX flash: 80ms white flash superimposed on base state.
- States: BOOT (blue pulse), NO_GPS (yellow 1Hz), GPS_OK (green solid), DEMO (purple pulse), LOCKED (red double-blink), OTA (rainbow), ERROR (red 4Hz).
- **OK.**

#### `rid_patrol.c` (31 lines)
- Demo GPS patrol simulation: Rome Colosseum (41.9028, 12.4964), 200m radius, 6 m/s, altitude drift, heading calc.
- **OK.**

---

### `webui/` — Embedded Configuration UI

#### `config.html` (~1520 lines)
- Full configuration and control web interface (inline JS + CSS + HTML).
- Dark/light theme, dashboard, forms, logs, OTA, terminal, CLI.
- ✅ Animated splash (Mac-style stepped progress bar).
- ✅ Responsive (off-canvas sidebar, 3 breakpoints).
- ✅ WiFi NAN checkbox, bar chart, counters, purple styling.
- ✅ Guide descriptions on all tabs.
- ✅ Bug fixes: `setAllTx()` single call, Restart with `.catch()`, log pane grid.
- ✅ Lock level + eFuse + signature verification implemented.
- ✅ Command signature support: restart/reset/reboot buttons use `sendSigCmd()`, terminal input includes hidden `X-Signature` field, dialog command supports headers.
- **Gap:** Inline only (~1520 lines) — optional refactor to separate files.

---

## `mavlink/` — MAVLink v2 C Dialect Headers (auto-generated by `mavlink/generate.sh`)

### Active (used in build)
| Path | Status |
|------|--------|
| `ardupilotmega/` (whole dialect) | ✅ Used (`mavlink_parser.c` includes `ardupilotmega/mavlink.h`) |
| `common/` | ✅ Included by `ardupilotmega/ardupilotmega.h` |
| `minimal/` | ✅ Included by `common/common.h` |
| `protocol.h` | ✅ Core MAVLink protocol |
| `mavlink_types.h` | ✅ Core types |
| `mavlink_get_info.h` | ✅ Included by `common/common.h` |
| `uAvionix/` | 🔶 Included transitively by `ardupilotmega.h` but no uAvionix messages parsed |
| `icarous/` | 🔶 Included transitively but unused |

### Unused (can be pruned)
| Path | Reason |
|------|--------|
| `standard/` | Only `development/` includes it |
| `development/` | Not included by any active dialect |
| `ASLUAV/` | Unused dialect |
| `matrixpilot/` | Unused dialect |
| `test/` | Test data only |
| `mavlink_helpers.h` | Signing not implemented |
| `mavlink_sha256.h` | Only in `mavlink_helpers.h` |
| `mavlink_conversions.h` | Only in `mavlink_helpers.h` |
| All `testsuite.h` | Test data |
| 8 of 11 `message_definitions/*.xml` | Unused definitions |

---

## `ESP_DRONE_REMOTEID_Analyzer/` — Python Analyzer (standalone package)

### Package Structure

#### `__init__.py` (3 lines)
- Package marker. Exports `__version__ = "2.0.0-dev"`.
- **OK.**

#### `__main__.py` (5 lines)
- Entry point: `python -m analyzerex` → launches GUI.
- Calls `gui.main()`.
- **OK.**

#### `capture.py` (~200 lines)
- WiFi beacon capture via Scapy (`sniff` on Dot11Beacon frames).
- `RIDCapture` class: interface listing, monitor-mode helpers (Linux/macOS/Windows).
- **v2:** PCAP batch write via `wrpcap()` for offline analysis in Wireshark.
- **v2:** Multi-channel hopping support (`set_channels()`, `set_channel_hopping()` with configurable interval).
- Callback-based: `on_packet(data)` receives parsed RSSI, source MAC, summary.
- **Gap:** Requires monitor-mode WiFi adapter + root/admin privileges. Not available on all hardware.

#### `decoder.py` (510 lines)
- Pure Python ASTM F3411-22a packet decoder.
- All message types: Basic ID, Location, Auth, Self-ID, System, Operator ID.
- `extract_odid_from_beacon()` — parses raw 802.11 beacon payload → ODID data.
- `format_summary()` — one-line human-readable summary of a decoded packet.
- **OK.** Comprehensive.

#### `server.py` (~550 lines)
- WebSocket broadcast server (`simple-websocket-server`).
- `RIDDevice` dataclass: per-device state (MAC, RSSI samples, trail history, basic ID, operator ID, location, system).
- **v2:** GPS track trail history — 500-point position ring buffer per device.
- **v2:** Session recording — `session_start()`, `session_stop()`, JSON persist/load/replay.
- **v2:** CSV/KML export — `cmd_get_csv()`, `cmd_get_kml()` generate downloadable content.
- **v2:** `to_detail_dict()` — full device snapshot with RSSI samples + trail + message timeline.
- **v2:** Snapshot on WS connect — new clients receive current device state immediately.
- **v2:** Command dispatch — `get_csv`, `get_kml`, `get_session`, `get_device_detail`, `session_start`, `session_stop`.
- Device deduplication + 60s stale timeout.
- **OK.**

#### `rid_cli.py` (115 lines)
- Headless CLI: `rid-capture`, `rid-scan`, `rid-monitor`, `rid-listen`, `rid-list-ifaces`.
- argparse-based subcommands.
- **OK.**

#### `gui.py` (~80 lines)
- **v2: Standalone pywebview desktop app** — no HTTP server, loads `index.html` from `file://`.
- `main()` — starts WebSocket server thread + capture thread, opens native window.
- **Security:** No browser accessibility — only the local pywebview window can connect.
- **OK.**

#### `build.spec` (56 lines)
- PyInstaller spec for Windows executable.
- Bundles `web/` HTML/JS/CSS as data files.
- **OK.**

#### `requirements.txt` (14 lines)
- `scapy>=2.5.0` — WiFi capture
- `simple-websocket-server>=0.4.0` — WebSocket bridge
- `pywebview>=4.4.1` — native window
- **OK.**

#### `ESP_DRONE_REMOTEID_Analyzer.py` / `main_analyzer.pyw`
- ❌ **Removed** from current tree (deprecated launchers).

### Analyzer Web UI (`web/`)

#### `index.html` (~140 lines)
- **v2:** 5 tabs: Devices (default), Map, Timeline, Statistics, About.
- **v2:** Toolbar with search/filter/export/replay controls.
- **v2:** Recording controls — Record/Stop buttons with pulsing REC badge.
- **v2:** Detail panel overlay — slides in from right on device click.
- **v2:** Replay file selector — dropdown to load previous sessions.
- **v2:** UA type distribution grid — metric cards in Statistics tab.
- Leaflet map, device table, packet log.
- **OK.**

#### `app.js` (~850 lines)
- WebSocket client, device table rendering, Leaflet map markers.
- **v2:** Drone trail polylines on map — 500-point path per device.
- **v2:** Detail panel — click device shows full detail, RSSI history, message timeline.
- **v2:** Session recording — Record/Stop/Replay UI logic.
- **v2:** CSV/KML download — fetch blob from server, trigger save.
- **v2:** UA-type search filter, RSSI quality filter (good/warn/bad), active-only toggle.
- **v2:** Desktop notifications — alerts for new devices when tab hidden.
- **v2:** Drone count over time — scrolling line chart.
- **v2:** Statistics tab — UA type distribution + top RSSI leaderboard.
- Dark mode toggle, stats (devices, IDs, packets/min).
- **OK.**

#### `style.css` (~320 lines)
- **v2:** Detail panel — fixed right-side slide-in panel with scroll.
- **v2:** Toolbar layout — horizontal button bar with search/filter controls.
- **v2:** Stat grid + metric cards — responsive grid for Statistics tab.
- **v2:** Responsive breakpoints — adapts to small windows.
- Dark/light theme, badge/tab/table styles.
- **OK.**

---

## `.github/` — CI/CD & Community

### `workflows/build.yml` (193 lines)
- CI on push/PR to main. Matrix: 6 targets (esp32, s2, s3, c2, c3, c6).
- Partition overrides per target (`partitions_2mb.csv` for C2).
- ccache, Python manifest generation, GitHub Pages deploy.
- **OK.**

### `workflows/release.yml` (308 lines)
- Trigger: tag `v*`. Same 6-target matrix.
- Generates zip per target + `manifest.json`, GitHub Release with auto-changelog.
- GitHub Pages deploy.
- **OK.**

### `dependabot.yml` (13 lines)
- Weekly updates for GitHub Actions. Europe/Rome timezone.
- **OK.**

### `ISSUE_TEMPLATE/bug_report.md` (45 lines)
- Hardware (ESP32 model, board, antenna, FC, GPS), firmware (version, target, protocol), config, console output.
- **OK.**

### `ISSUE_TEMPLATE/feature_request.md` (25 lines)
- Problem, solution, alternatives, affected protocols (MAVLink/MSP/NMEA/All).
- **OK.**

### `PULL_REQUEST_TEMPLATE.md` (38 lines)
- Checklist: coding style, hardware test, docs, build, warnings.
- **OK.**

---

## `.vscode/` — Editor Configuration

### `settings.json` (30 lines)
- Clangd config + IDF extension config.
- ✅ Absolute paths removed; uses `${workspaceFolder}` and extension variables.

### `launch.json` (10 lines)
- "Eclipse CDT GDB Adapter" attach config only.
- 🟢 Gitignored, local only. No OpenOCD or JTAG debug profiles.

### `c_cpp_properties.json` (20 lines)
- ESP-IDF IntelliSense config. `compileCommands`, include paths.
- ✅ Gitignored, local only. Hardcoded compiler path removed locally.

---

## `docs/` — GitHub Pages Site

### `index.html` (~901 lines)
- Quick Start landing page + WebSerial ESP Web Tools installer + built-in wiki sections 1–2.
- CSS custom properties design system (`--surface`, `--text`, `--accent`, `--shadow-*`, `--radius-*`).
- Inter font from Google Fonts, full-width layout, dark/light mode.
- Guide sections with gradient `::before` border-left pseudo-element, box-shadow hover.
- Tables: `border-radius` + `overflow:hidden`, `border-collapse:separate`, sticky headers.
- Callouts (info/warning/tip) with colored left border, `var()` backgrounds.
- Code blocks: dark `#1a1a2e` background with light `#e4e4ef` text even in light mode.
- Wiring cards: `box-shadow` + hover `translateY(-1px)`.
- Mermaid diagrams with animations (fade-in, dashed edge flow, hover glow).
- Release bar (version timeline), reading progress bar, TOC panel, lightbox for diagrams.
- Responsive (3 breakpoints), breadcrumbs, footer.
- Dark mode uses neutral black/gray (`#0d0d0d`, `#141414`, `#e0e0e0` etc.).
- **Gap:** Inline CSS/JS — optional refactor.

### `guide.html` (~1864 lines)
- Technical wiki: sections 3–16 (protocol detection, MAVLink/MSP/NMEA details, BLE, WiFi NAN, Web UI, OTA, API reference, lock security, troubleshooting).
- Same CSS design system as index.html with 16-color gradient palette for section headers.
- Protocol auto-detection Mermaid diagram with correct NMEA "No" path.
- Wiring diagrams, support matrices, table of contents, search.
- Dark mode uses same neutral black/gray as index.html.
- Mermaid animations, anchor links, copy buttons.
- **Gap:** Inline CSS/JS — optional refactor.

### `config(demo).html` (~2546 lines)
- Standalone offline demo of config UI. Full telemetry simulation:
  - GPS circular patrol (Rome Colosseum, 200m radius, ±15m altitude drift)
  - Battery drain (12.2V→10.8V), per-protocol counters, RSSI/noise, CPU/HEAP
  - Dynamic logs from 12 templates, simulated OTA progress bar
  - Fetch interceptor covers all `/api/*` endpoints
- ✅ Splash, responsive, icons, descriptions ported from config.html.
- **Gap:** Must be manually synced with `config.html` changes.

### `manifest.json` (57 lines)
- ESP Web Tools firmware manifest: all 6 chip families (ESP32, S2, S3, C2, C3, C6).
- Version `1.0.0-beta`, config URL `http://192.168.4.1`.
- **Gap:** Hardcoded version (should be auto-generated from CI).

### `images/` (5 files)
- `logo.svg`, `logo con scritta.svg` — project logos.
- `ardupilot_logo.webp`, `betaflight_logo.svg`, `inav_logo.png` — FC compatibility icons.
- **OK.**

---

## Root Files

### `README.md` (~120 lines)
- Project overview, CI badges, supported platforms, quick start, wiring, build.
- ✅ Updated: removed "hardware testing pending" notes.
- **Gap:** Mentions "no extra GPS needed" — firmware actually requires FC with GPS or demo mode.

### `.gitignore` (140 lines)
- Covers: build/, sdkconfig, __pycache__, *.bin, .vscode/*.json.
- `!.vscode/settings.json` — intentionally re-includes.
- **OK.**

### `.gitattributes` (40 lines)
- LF normalization for C/PY/JS/JSON/MD/YML/YAML; CRLF for BAT/PS1.
- **OK.**

### `LICENSE`
- ✅ Apache 2.0 license added. Derived from [Intel Open Drone ID](https://github.com/opendroneid).
- Copyright notice credits both VOLTEKOVER (port) and Intel Corporation (original ODID).

---

## `todolist/` — Documentation & Assets

### `softwarestatus.md` (this file)
- Comprehensive per-file analysis.
- **OK** (self-referencing).

### `guide.md`, `build_guide.md`, `faq.md`, `firmware_flashing_guide.md`
- ❌ **Removed from tree.** No longer present in `todolist/` (only `softwarestatus.md` remains).
- Content may have been merged into `docs/index.html`.

### `3D_case/`
- ❌ **Not in current tree.** STL/F3D files for 3D-printable enclosure.
- May have been relocated or removed.

---

## Priority Summary

| Priority | Item | Location | Status |
|----------|------|----------|--------|
| 🔴 HIGH | `sdkconfig` tracked in git | root | ✅ Already in `.gitignore`, not tracked |
| 🔴 HIGH | `LICENSE` missing | root | ✅ Apache 2.0 added |
| 🔴 HIGH | `docs/manifest.json` missing targets | `docs/manifest.json` | ✅ All 6 targets present |
| 🟡 MEDIUM | Lock level command signature | `web_config.c` + `config.html` | ✅ ECDSA P-256 via mbedTLS PK |
| 🟡 MEDIUM | BLE 5.0 LR Coded PHY = Beta | `ble_tx.c` | 🟡 Needs testing on S3/C3 |
| 🟡 MEDIUM | Analyzer requires monitor mode + root | `capture.py` | 🟡 Hardware limitation |
| 🟢 LOW | `c_cpp_properties.json` hardcoded path | `.vscode/c_cpp_properties.json` | ✅ Gitignored, local only — fixed locally |
| 🟢 LOW | `launch.json` no debug profiles | `.vscode/launch.json` | ✅ Gitignored, local only |
| 🟢 LOW | `config(demo).html` manual sync needed | `docs/config(demo).html` | 🟡 Maintenance burden |
| 🟢 LOW | `config.html` inline (~2340 lines) | `webui/config.html` | Optional refactor |
| 🟢 LOW | `docs/index.html` inline (~901 lines) | `docs/index.html` | Optional refactor |
| 🟢 LOW | `docs/guide.html` inline (~1864 lines) | `docs/guide.html` | Optional refactor |
| ✅ DONE | `altitude_baro` populated by all parsers | All parsers | ✅ |
| ✅ DONE | `force_tx` redundant logic fixed | `esp_remote_id.c` | ✅ |
| ✅ DONE | MAVLink ODID identity override | `mavlink_parser.c` + `esp_remote_id.c` | ✅ |
| ✅ DONE | eFuse lock level 2 burn | `web_config.c` | ✅ |
| ✅ DONE | OTA SHA-256 mandatory | `web_config.c` | ✅ |
| ✅ DONE | WiFi NAN enabled + UI | `wifi_tx.c` + `config.html` | ✅ |
| ✅ DONE | BLE SCAN_IND (nonconn→scan) | `ble_tx.c` | ✅ |
| ✅ DONE | Duplicate UAS data refactored | `wifi_tx.c` | ✅ |
| ✅ DONE | `.vscode/` absolute paths removed | `.vscode/*.json` | ✅ |
| ✅ DONE | Demo GPS patrol mode | `rid_patrol.c` + `main.c` | ✅ |
| ✅ DONE | `idf_component.yml` description | `idf_component.yml` | ✅ |
| ✅ DONE | `docs/index.html` BETA status | `docs/index.html` | ✅ |
| ✅ DONE | `README.md` outdated notes removed | `README.md` | ✅ |
| ✅ DONE | `docs/config(demo).html` simulation engine | `docs/config(demo).html` | ✅ |
| ✅ DONE | Wiki split (index.html + guide.html) | `docs/` | ✅ |
| ✅ DONE | Dark mode color fix (blue→neutral black) | `docs/index.html` + `docs/guide.html` | ✅ |
| ✅ DONE | Diagram animations + Mermaid bug fixes | `docs/index.html` + `docs/guide.html` | ✅ |
| ✅ DONE | Command signature for restart/reset | `web_config.c` + `config.html` | ✅ |

---

## File Inventory (complete)

| # | File | Lines | Status |
|---|------|-------|--------|
| 1 | `ESP32_DRONE_REMOTE_ID_Firmware/CMakeLists.txt` | 18 | ✅ |
| 2 | `ESP32_DRONE_REMOTE_ID_Firmware/sdkconfig.defaults` | 1 | ✅ |
| 3 | `ESP32_DRONE_REMOTE_ID_Firmware/sdkconfig` | 3461 | ❌ generated (tracked) |
| 4 | `ESP32_DRONE_REMOTE_ID_Firmware/partitions.csv` | 7 | ✅ |
| 5 | `ESP32_DRONE_REMOTE_ID_Firmware/partitions_2mb.csv` | 7 | ✅ |
| 6 | `ESP32_DRONE_REMOTE_ID_Firmware/idf_component.yml` | 12 | ✅ |
| 7 | `ESP32_DRONE_REMOTE_ID_Firmware/main/CMakeLists.txt` | 3 | ✅ |
| 8 | `ESP32_DRONE_REMOTE_ID_Firmware/main/main.c` | 90 | ✅ |
| 9 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/CMakeLists.txt` | 38 | ✅ |
| 10 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/Kconfig.projbuild` | 27 | ✅ |
| 12 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/esp_remote_id.h` | 80 | ✅ |
| 13 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/opendroneid.h` | 762 | ✅ |
| 14 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/odid_wifi.h` | 106 | ✅ |
| 15 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/wifi_tx.h` | 30 | ✅ |
| 16 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/ble_tx.h` | 20 | ✅ |
| 17 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/mav2odid.h` | 30 | ✅ |
| 18 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/protocol_detect.h` | 10 | ✅ |
| 19 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/nmea_parser.h` | 15 | ✅ |
| 20 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/msp_parser.h` | 15 | ✅ |
| 21 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/mavlink_parser.h` | 40 | ✅ |
| 22 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/web_config.h` | 15 | ✅ |
| 23 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/nvs_storage.h` | 20 | ✅ |
| 24 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/led_status.h` | 30 | ✅ |
| 25 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/rid_patrol.h` | 10 | ✅ |
| 26 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/include/cli.h` | 7 | ✅ |
| 27 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/cli.c` | 317 | ✅ |
| 28 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/esp_remote_id.c` | 458 | ✅ |
| 29 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/wifi.c` | 614 | ✅ |
| 30 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/wifi_tx.c` | 204 | ✅ |
| 31 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/ble_tx.c` | 183 | ✅ |
| 32 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/web_config.c` | 738 | ✅ |
| 33 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/protocol_detect.c` | 75 | ✅ |
| 34 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/nmea_parser.c` | 136 | ✅ |
| 35 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/msp_parser.c` | 126 | ✅ |
| 36 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/mavlink_parser.c` | 180 | ✅ |
| 37 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/mav2odid.c` | 636 | ✅ (partial use) |
| 38 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/opendroneid.c` | 1477 | ✅ |
| 39 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/nvs_storage.c` | 174 | ✅ |
| 40 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/led_status.c` | 199 | ✅ |
| 41 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/src/rid_patrol.c` | 31 | ✅ |
| 42 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/webui/config.html` | ~2340 | ✅ |
| 43-122 | `ESP32_DRONE_REMOTE_ID_Firmware/components/esp_remote_id/mavlink/**/*.h/.xml` | ~80 files | 🔶 many unused |
| 123 | `ESP_DRONE_REMOTEID_Analyzer/__init__.py` | 3 | ✅ |
| 124 | `ESP_DRONE_REMOTEID_Analyzer/__main__.py` | 5 | ✅ |
| 125 | `ESP_DRONE_REMOTEID_Analyzer/capture.py` | 176 | ✅ |
| 126 | `ESP_DRONE_REMOTEID_Analyzer/decoder.py` | 510 | ✅ |
| 127 | `ESP_DRONE_REMOTEID_Analyzer/server.py` | 197 | ✅ |
| 128 | `ESP_DRONE_REMOTEID_Analyzer/rid_cli.py` | 115 | ✅ |
| 129 | `ESP_DRONE_REMOTEID_Analyzer/gui.py` | 109 | ✅ |
| 130 | `ESP_DRONE_REMOTEID_Analyzer/build.spec` | 56 | ✅ |
| 131 | `ESP_DRONE_REMOTEID_Analyzer/requirements.txt` | 14 | ✅ |
| 132 | `ESP_DRONE_REMOTEID_Analyzer/web/index.html` | 94 | ✅ |
| 133 | `ESP_DRONE_REMOTEID_Analyzer/web/app.js` | 438 | ✅ |
| 134 | `ESP_DRONE_REMOTEID_Analyzer/web/style.css` | 176 | ✅ |
| 135 | `docs/index.html` | ~901 | ✅ (inline, wiki split) |
| 136 | `docs/guide.html` | ~1864 | ✅ (inline, technical wiki) |
| 137 | `docs/config(demo).html` | ~2546 | ✅ |
| 138 | `docs/manifest.json` | 57 | ✅ (all 6 targets present) |
| 139 | `docs/images/logo.svg` | — | ✅ |
| 140 | `docs/images/logo con scritta.svg` | — | ✅ |
| 141 | `docs/images/ardupilot_logo.webp` | — | ✅ |
| 142 | `docs/images/betaflight_logo.svg` | — | ✅ |
| 143 | `docs/images/inav_logo.png` | — | ✅ |
| 144 | `.github/workflows/build.yml` | 193 | ✅ |
| 145 | `.github/workflows/release.yml` | 308 | ✅ |
| 146 | `.github/dependabot.yml` | 13 | ✅ |
| 147 | `.github/ISSUE_TEMPLATE/bug_report.md` | 45 | ✅ |
| 148 | `.github/ISSUE_TEMPLATE/feature_request.md` | 25 | ✅ |
| 149 | `.github/PULL_REQUEST_TEMPLATE.md` | 38 | ✅ |
| 150 | `.vscode/settings.json` | 30 | ✅ |
| 151 | `.vscode/launch.json` | 10 | 🟢 gitignored, local only |
| 152 | `.vscode/c_cpp_properties.json` | 20 | 🟢 gitignored, local only |
| 153 | `README.md` | ~120 | ✅ |
| 154 | `.gitignore` | 140 | ✅ |
| 155 | `.gitattributes` | 40 | ✅ |
| 156 | `todolist/softwarestatus.md` | — | ✅ (this file) |
