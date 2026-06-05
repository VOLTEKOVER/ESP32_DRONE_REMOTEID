# ESP32 Remote ID - Build & Flash Guide

Guida completa per compilare e flashare il progetto ESP-IDF.

## 🚀 Quick Start

```bash
# Entrare nel progetto principale
cd firmware/ESP32_DRONE_ID

# Impostare il target ESP32
idf.py set-target esp32

# Build
idf.py build

# Flash e monitor (sostituisci COM3 con la tua porta)
idf.py -p COM3 flash monitor
```

## 📋 Prerequisiti

1. **ESP-IDF installato** (v5.0+)
   ```bash
   # Verifica
   idf.py --version
   ```

2. **ESP32 collegato via USB**
   - Driver USB installati
   - Porta COM disponibile

3. **Ambiente configurato**
   - Apri "ESP-IDF CMD Prompt" da Start Menu (Windows)
   - O esegui `. ./export.sh` su Linux/macOS

## 🛠️ Comandi Frequenti

### Build

```bash
# Build standard
idf.py build

# Build con output dettagliato
idf.py build -v

# Rebuilding (pulisci prima)
idf.py fullclean
idf.py build
```

### Flashing

```bash
# Auto-detect porta
idf.py flash

# Porta specifica
idf.py -p COM3 flash

# Baud rate personalizzato
idf.py -p COM3 -b 460800 flash

# Erase flash prima di flashing
idf.py erase-flash
idf.py -p COM3 flash
```

### Monitoraggio

```bash
# Monitor standard
idf.py monitor

# Con porta specifica
idf.py -p COM3 monitor

# Con baud rate personale
idf.py -p COM3 -b 115200 monitor

# Esci monitor: Ctrl+]
```

### Combinato

```bash
# Build + Flash + Monitor (3 in 1)
idf.py -p COM3 build flash monitor

# Uguale ma con velocità
idf.py -p COM3 -b 921600 build flash monitor
```

## 🔌 Ricerca Porta

### Windows (PowerShell)

```powershell
# Lista porte COM
Get-WmiObject Win32_SerialPort | Select-Object Name, Description

# Oppure tramite Device Manager:
# Dispositivi > Porte COM
```

### Linux

```bash
# Lista porte
ls /dev/ttyUSB*
ls /dev/ttyACM*

# Permessi (se necessario)
sudo usermod -a -G dialout $USER
```

### macOS

```bash
ls /dev/tty.usbserial-*
```

## 🔍 Configurazione Progetto

### Menuconfig

Accedi alle opzioni di build:

```bash
idf.py menuconfig
```

**Opzioni utili:**

- `Component config` → `ESP32-specific`
  - CPU frequency
  - Core count
  - Memory config

- `Serial flasher config`
  - Flash baud rate
  - Flash mode/size

### .vscode/settings.json

Configurazione VSCode per il progetto:

```json
{
  "idf.currentSetup": "C:\\esp\\v6.0.1\\esp-idf",
  "idf.projectPath": "${workspaceFolder}/firmware/ESP32_DRONE_ID",
  "idf.customExtraVars": {
    "IDF_TARGET": "esp32"
  }
}
```

## 📊 Output Build

Dopo un build riuscito:

```
firmware/ESP32_DRONE_ID/build/
├── esp32-remote-id-scanner.bin      ← Firmware (per flashing)
├── esp32-remote-id-scanner.elf      ← Executable (debug)
├── bootloader.bin
├── partition-table.bin
├── compile_commands.json
└── ...
```

### File Importante

**`esp32-remote-id-scanner.bin`** è il file da flashare.

## 🐛 Risoluzione Problemi

### "command not found: idf.py"

**Windows:**
```powershell
# Usa il batch file
%IDF_PATH%\tools\idf.py build
```

**Linux/macOS:**
```bash
# Source l'environment
source $IDF_PATH/export.sh
idf.py build
```

### "Error: CMake 3.22 required"

Aggiorna CMake:
```bash
# Tramite ESP-IDF installer
# O manualmente da cmake.org
```

### "Port COM3 not found"

1. Controlla che ESP32 sia collegato
2. Verifica in Device Manager / Serial ports
3. Prova di nuovo dopo riavvio
4. Controlla driver FTDI/CH340 se necessario

### "Failed to open /dev/ttyUSB0"

Linux/macOS:
```bash
sudo chmod 666 /dev/ttyUSB0
# O aggiungi l'utente al gruppo
sudo usermod -a -G dialout $USER
```

### "Brownout detector triggered"

Il device riavvia durante il boot:
- Alimentazione insufficiente (usa cavo USB di qualità)
- Prova a disabilitare nel menuconfig
- Usa PSU esterna

### Memory errors "Out of memory"

Aumenta heap:
```bash
idf.py menuconfig
# Component config → Heap → Maximum heap size
```

## 📝 Log di Build

Salva il log:

```bash
idf.py build 2>&1 | tee build.log
```

## 🔐 Sicurezza Flash

### Signature & Encryption

Per abilitare secure boot (avanzato):

```bash
idf.py menuconfig
# Security features
```

### Protezione Lettura

```bash
idf.py secure-padding-block read_protection enable
```

## 📚 Documentazione

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [ESP-IDF Components](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system-components.html)
- [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)

## 🚨 Note Importanti

1. **Sempre** collega ESP32 DOPO aver aperto il terminale
2. **Non** spegnere il device durante il flash
3. Usa **cavi USB di qualità** per flashing stabile
4. Tieni **serial monitor aperto** per debug

## ✨ VSCode Integration

Con l'extension Espressif IDF:

1. Command Palette: `ESP-IDF: Build`
2. Command Palette: `ESP-IDF: Flash`
3. Command Palette: `ESP-IDF: Monitor`

---

**Problemi?** Vedi ENV.md per setup completo.
