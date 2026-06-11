# ESP32 Remote ID - Build & Flash Guide

Complete guide to compile and flash the ESP-IDF project.

## 🚀 Quick Start

```bash
# Enter the main project directory
cd ESP32_DRONE_ID

# Set ESP32 target
idf.py set-target esp32

# Build
idf.py build

# Flash and monitor (replace COM3 with your port)
idf.py -p COM3 flash monitor
```

## 📋 Prerequisites

1. **ESP-IDF installed** (v5.0+)
   ```bash
   # Verify
   idf.py --version
   ```

2. **ESP32 connected via USB**
   - USB drivers installed
   - COM port available

3. **Environment configured**
   - Open "ESP-IDF CMD Prompt" from Start Menu (Windows)
   - Or run `. ./export.sh` on Linux/macOS

## 🛠️ Frequent Commands

### Build

```bash
# Standard build
idf.py build

# Build with verbose output
idf.py build -v

# Rebuilding (clean first)
idf.py fullclean
idf.py build
```

### Flashing

```bash
# Auto-detect port
idf.py flash

# Specific port
idf.py -p COM3 flash

# Custom baud rate
idf.py -p COM3 -b 460800 flash

# Erase flash before flashing
idf.py erase-flash
idf.py -p COM3 flash
```

### Monitoring

```bash
# Standard monitor
idf.py monitor

# With specific port
idf.py -p COM3 monitor

# With custom baud rate
idf.py -p COM3 -b 115200 monitor

# Exit monitor: Ctrl+]
```

### Combined

```bash
# Build + Flash + Monitor (3 in 1)
idf.py -p COM3 build flash monitor

# Same but with speed
idf.py -p COM3 -b 921600 build flash monitor
```

## 🔌 Port Discovery

### Windows (PowerShell)

```powershell
# List COM ports
Get-WmiObject Win32_SerialPort | Select-Object Name, Description

# Or via Device Manager:
# Devices > COM Ports
```

### Linux

```bash
# List ports
ls /dev/ttyUSB*
ls /dev/ttyACM*

# Permissions (if needed)
sudo usermod -a -G dialout $USER
```

### macOS

```bash
ls /dev/tty.usbserial-*
```

## 🔍 Project Configuration

### Menuconfig

Access build options:

```bash
idf.py menuconfig
```

**Useful options:**

- `Component config` → `ESP32-specific`
  - CPU frequency
  - Core count
  - Memory config

- `Serial flasher config`
  - Flash baud rate
  - Flash mode/size

### .vscode/settings.json

VSCode configuration for the project:

```json
{
  "idf.currentSetup": "C:\\esp\\v6.0.1\\esp-idf",
  "idf.projectPath": "${workspaceFolder}",
  "idf.customExtraVars": {
    "IDF_TARGET": "esp32"
  }
}
```

## 📊 Build Output

After a successful build:

```
build/
├── esp32-remote-id-scanner.bin      ← Firmware (for flashing)
├── esp32-remote-id-scanner.elf      ← Executable (debug)
├── bootloader.bin
├── partition-table.bin
├── compile_commands.json
└── ...
```

### Important File

**`esp32-remote-id-scanner.bin`** is the file to flash.

## 🐛 Troubleshooting

### "command not found: idf.py"

**Windows:**
```powershell
# Use the batch file
%IDF_PATH%\tools\idf.py build
```

**Linux/macOS:**
```bash
# Source the environment
source $IDF_PATH/export.sh
idf.py build
```

### "Error: CMake 3.22 required"

Update CMake:
```bash
# Via ESP-IDF installer
# Or manually from cmake.org
```

### "Port COM3 not found"

1. Check that ESP32 is connected
2. Verify in Device Manager / Serial ports
3. Try again after reboot
4. Check FTDI/CH340 driver if needed

### "Failed to open /dev/ttyUSB0"

Linux/macOS:
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add user to group
sudo usermod -a -G dialout $USER
```

### "Brownout detector triggered"

Device reboots during boot:
- Insufficient power (use a quality USB cable)
- Try disabling it in menuconfig
- Use an external PSU

### Memory errors "Out of memory"

Increase heap:
```bash
idf.py menuconfig
# Component config → Heap → Maximum heap size
```

## 📝 Build Log

Save the log:

```bash
idf.py build 2>&1 | tee build.log
```

## 🔐 Flash Security

### Signature & Encryption

To enable secure boot (advanced):

```bash
idf.py menuconfig
# Security features
```

### Read Protection

```bash
idf.py secure-padding-block read_protection enable
```

## 📚 Documentation

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [ESP-IDF Components](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system-components.html)
- [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)

## 🚨 Important Notes

1. **Always** connect ESP32 AFTER opening the terminal
2. **Do not** power off the device during flash
3. Use **quality USB cables** for stable flashing
4. Keep **serial monitor open** for debugging

## ✨ VSCode Integration

With the Espressif IDF extension:

1. Command Palette: `ESP-IDF: Build`
2. Command Palette: `ESP-IDF: Flash`
3. Command Palette: `ESP-IDF: Monitor`

---

**Problems?** See ENV.md for full setup.
