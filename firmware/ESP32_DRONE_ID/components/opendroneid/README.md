# OpenDroneID Component per ESP-IDF

Componente ESP-IDF che wrappa la libreria OpenDroneID.

## 📋 Contenuti

Questa componente include i file del progetto OpenDroneID da `../../id_open/`:

- `id_open_esp32.cpp` - ESP32-specific OpenDroneID functions
- `id_open.cpp` - Core OpenDroneID library
- `id_open_beacon.cpp` - WiFi beacon parsing
- `alt_unix_time.c` - Unix timestamp utilities

## 🔗 Dipendenze

- `id_open.h` - Header principale (in `../../id_open/`)

## 📖 Utilizzo

### Nel tuo codice:

```c
#include "opendroneid.h"

// Decodifica beacon WiFi
ODID_UAS_Data uas_data;
odid_message_process_pack(&uas_data, payload, length);

// Decodifica frame NAN
odid_wifi_receive_message_pack_nan_action_frame(&uas_data, mac, payload, length);
```

## ⚙️ Configurazione

Il CMakeLists.txt include automaticamente:
- Path alla cartella parent `id_open`
- Linking a libreria C++ standard

## 🚀 Build

La componente è compilata automaticamente come dipendenza dal progetto principale.

```bash
idf.py build
```

## 📚 Riferimenti

Vedi [id_open README](../../id_open/README.md) per documentazione del protocollo OpenDroneID.
