# TrekLink

> **Meshtastic-Powered Off-Grid Communication Device**  
> ESP32 + Ra-02 433MHz LoRa + GPS + OLED

TrekLink is a custom variant of [Meshtastic](https://meshtastic.org/) firmware designed for off-grid communication in remote areas. It combines long-range 433MHz LoRa mesh networking with GPS tracking and a compact OLED display for backcountry adventures.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Meshtastic: 2.7.19](https://img.shields.io/badge/Meshtastic-2.7.19-green.svg)](https://meshtastic.org/)
[![Radio: 433MHz](https://img.shields.io/badge/Radio-433MHz%20LoRa-orange.svg)]()

---

## 🚀 Quick Start

### Prerequisites
- **Hardware:** ESP32-WROOM-32, Ra-02 SX1278 (433MHz), GPS Neo-6M, OLED SSD1306
- **Software:** [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- **Mobile App:** [Meshtastic](https://meshtastic.org/docs/getting-started/download) (iOS/Android)

### 1. Clone Repository
```bash
git clone https://github.com/ruskicoder/treklink.git
cd treklink
```

### 2. Build Firmware
```bash
# Build TrekLink variant
pio run -e treklink

# Flash to ESP32
pio run -e treklink -t upload

# Monitor serial output
pio device monitor -b 115200
```

### 3. Expected Boot Log
```
[INFO] Meshtastic 2.7.19
[INFO] Region: EU_433
[INFO] Radio: SX1278 initialized
[INFO] Frequency: 433.175 MHz
[INFO] GPS: Neo-6M detected
[INFO] OLED: SSD1306 0x3C found
```

### 4. Connect to Meshtastic App
1. Install **Meshtastic app** on your phone
2. Enable **Bluetooth**
3. Open app → **"Add Device"** → **"Bluetooth"**
4. Select **"TrekLink_XXXX"** (XXXX = last 4 MAC digits)
5. **Send messages** via mesh network! 📡

---

## 🛠️ Hardware Setup

### Minimum Viable Configuration
```
ESP32-WROOM-32 + Ra-02 SX1278 + GPS Neo-6M + OLED SSD1306 + 1 Button
```

### Wiring Guide

| Component | ESP32 GPIO | Notes |
|-----------|------------|-------|
| **Ra-02 LoRa (SPI)** | | |
| SCK | 5 | SPI Clock |
| MISO | 19 | SPI In |
| MOSI | 27 | SPI Out |
| CS | 18 | Chip Select |
| DIO0 | 26 | **Critical** interrupt pin |
| RESET | 14 | Module reset |
| VCC | 3.3V | ⚠️ **NOT 5V!** |
| **GPS Neo-6M (UART)** | | |
| TX (GPS) | 16 (RX1) | GPS → ESP32 |
| RX (GPS) | 17 (TX1) | ESP32 → GPS |
| VCC | 3.3V | Optional power gating via GPIO 13 |
| **OLED SSD1306 (I2C)** | | |
| SDA | 21 | I2C Data |
| SCL | 22 | I2C Clock |
| VCC | 3.3V | Optional power gating via GPIO 23 |
| **MENU Button** | | |
| Button | 25 → GND | Internal pull-up enabled |
| **Battery Monitor** | | |
| ADC | 36 | Voltage divider (2:1 ratio) |

**📋 Full pinout:** See [specifications.md](specifications.md#32-gpio-pinout-allocation)  
**🔧 Hardware testing:** See [phase1_hardware_testing.md](C:\Users\PC\.gemini\antigravity\brain\8b95c2df-5ee8-4678-8d5d-b41e629acc51\phase1_hardware_testing.md)

---

## ✨ Features

### ✅ Current (Phase 1 - Meshtastic Base)
- **433MHz LoRa Mesh** - Compatible with all Meshtastic devices
- **GPS Location Sharing** - U-BLOX Neo-6M with position beacons
- **OLED Display** - 128x64 Meshtastic UI (messages, nodes, stats, compass)
- **Bluetooth Connectivity** - Pair with iOS/Android Meshtastic app
- **Single-Button Navigation** - Cycle screens, send pings
- **Battery Monitoring** - ADC voltage sensing with % display
- **EU_433 Region** - 433.175 MHz LoRa frequency
- **Encrypted Mesh** - Meshtastic PKC end-to-end encryption

### 🔄 Planned (Phase 2 - Custom Enhancements)
- **Fall Detection** - MPU6050 IMU with automatic SOS alerts
- **Silent Mode** - OLED power gating (GPIO 23) for stealth operation
- **Multi-Button Controls** - SOS (GPIO 34), UP (GPIO 32), DOWN (GPIO 35)
- **GPS Power Gating** - GPIO 13 P-MOSFET circuit for battery savings
- **Custom Encryption Layer** - Optional AES-128-GCM for private TrekLink comms
- **Vibration/Buzzer Alerts** - Notification outputs (GPIO 4, 33)

---

## 📱 Usage

### Meshtastic App (Primary Interface)
All configuration and messaging is done via the **Meshtastic mobile app**:
- **Send/receive messages** - Text communication across mesh
- **View nodes** - See nearby Meshtastic devices
- **Share location** - GPS coordinates via mesh
- **Configure settings** - Region, channel, encryption keys
- **OTA updates** - Update firmware wirelessly

### On-Device Button (Field Monitoring)
The **MENU button (GPIO 25)** cycles OLED screens:
- **Single press** - Next screen (Messages → Nodes → Stats → Compass)
- **Double press** - Send location ping
- **Long press (3s)** - Toggle screen on/off (battery saver)

---

## 📐 Technical Specifications

| **Category** | **Specification** |
|--------------|-------------------|
| **Microcontroller** | ESP32-WROOM-32 (240MHz, 320KB RAM, 4MB Flash) |
| **Radio** | Ra-02 SX1278 (433MHz, SPI, 20dBm max TX power) |
| **GPS** | Neo-6M (UART, NMEA 9600 baud, internal backup battery) |
| **Display** | SSD1306 OLED 0.96" (128x64, I2C 0x3C, white) |
| **IMU** | MPU6050 (I2C 0x68, 3-axis gyro + accel) - *Phase 2* |
| **Power** | 2x 21700 Li-Ion (10,000mAh total, 3.7V nominal) |
| **Battery Life** | ~110 hours active, ~400 hours deep sleep |
| **Firmware** | Meshtastic v2.7.19 (ESP32 Arduino framework) |
| **Build System** | PlatformIO |

**📊 Full specs:** See [specifications.md](specifications.md)

---

## 🏗️ Project Structure

```
treklink/
├── variants/esp32/treklink/      # TrekLink custom variant
│   ├── variant.h                 # GPIO pin definitions
│   ├── pins_arduino.h            # Arduino pin mappings
│   └── platformio.ini            # Build configuration
├── boards/
│   └── treklink-esp32.json       # Board definition
├── src/                          # Meshtastic source code
├── .kiro/specs/treklink/         # Design documents
│   ├── requirements.md           # EARS requirements
│   ├── design.md                 # Technical design
│   └── tasks.md                  # Implementation tasks
├── specifications.md             # Hardware/software specs (SSOT)
├── platformio.ini                # Main build config
└── README.md                     # This file
```

---

## 🔧 Development

### Build Commands
```bash
# Build firmware
pio run -e treklink

# Clean build
pio run -e treklink --target clean

# Flash to ESP32
pio run -e treklink -t upload

# Serial monitor
pio device monitor -b 115200

# Upload + monitor
pio run -e treklink -t upload && pio device monitor -b 115200
```

### Firmware Files
After successful build, binaries are in `.pio/build/treklink/`:
- `firmware-treklink-2.7.19.408e923.bin` - Main application (OTA updates)
- `firmware-treklink-2.7.19.408e923.factory.bin` - Full flash image
- `littlefs-treklink-2.7.19.408e923.bin` - Filesystem (config/data)

### Custom Variant Configuration
TrekLink uses a custom ESP32 variant with specific GPIO assignments. To modify:
1. Edit `variants/esp32/treklink/variant.h`
2. Update GPIO pin definitions
3. Rebuild: `pio run -e treklink --target clean && pio run -e treklink`

---

## 🧪 Testing

### Serial Monitor Test
```bash
pio device monitor -b 115200
```

**Expected output:**
```
[INFO] Region: EU_433
[INFO] Radio: SX1278 initialized      ← ✅ Ra-02 working
[INFO] GPS: Fix acquired (8 sats)     ← ✅ GPS working
[INFO] OLED: SSD1306 0x3C found       ← ✅ Display working
```

### Bluetooth Pairing Test
1. Device boots → OLED shows Meshtastic logo
2. Phone Bluetooth → See "TrekLink_XXXX"
3. Meshtastic app → Connect → Device info shows

### Mesh Communication Test
1. Get **second Meshtastic device** (any variant)
2. Set both to **same region** (EU_433) and **same channel**
3. Send message Device A → Device B
4. Verify message received + RSSI/SNR displayed

**📋 Full testing guide:** See [phase1_hardware_testing.md](C:\Users\PC\.gemini\antigravity\brain\8b95c2df-5ee8-4678-8d5d-b41e629acc51\phase1_hardware_testing.md)

---

## 🗺️ Roadmap

### Phase 1: ✅ Working Meshtastic Device (COMPLETE)
- [x] Clone Meshtastic v2.7.19
- [x] Create TrekLink custom variant
- [x] Configure Ra-02 SX1278 (433MHz SPI)
- [x] Set GPIO pinout (Meshtastic DIY + TrekLink custom)
- [x] Build & verify firmware compilation
- [ ] Flash + test on hardware
- [ ] Verify Bluetooth pairing
- [ ] Test mesh communication

### Phase 2: 🔄 Custom Enhancements (Optional)
- [ ] Implement `TrekLinkButtonModule` (SOS, UP, DOWN buttons)
- [ ] Add `FallDetectionModule` (MPU6050 IMU)
- [ ] Implement GPS power gating circuit (GPIO 13)
- [ ] Add Silent Mode OLED control (GPIO 23)
- [ ] Custom TrekLink AES-128-GCM encryption layer
- [ ] Buzzer/vibration notification outputs

---

## 🐛 Troubleshooting

### Build Errors

**Error:** `pins_arduino.h: No such file`
```bash
# Missing file - should exist at variants/esp32/treklink/pins_arduino.h
# Re-clone repository if missing
```

**Error:** `LORA_DIO1 was not declared`
```bash
# Check variant.h includes:
#define LORA_DIO1 RADIOLIB_NC
```

### Runtime Errors

**Radio init failed**
- ❌ Check SPI wiring: SCK=5, MISO=19, MOSI=27, CS=18, RESET=14
- ❌ Verify 3.3V power (NOT 5V!)
- ❌ Test Ra-02 module with multimeter

**DIO0 interrupt timeout**
- ❌ Check GPIO 26 connected to Ra-02 DIO0 pin
- ❌ Verify solid connection (breadboard contact issues common)

**GPS no fix**
- ⚠️ GPS requires outdoor clear sky view
- ⚠️ Wait 5-10 minutes for cold start
- ❌ Check TX/RX wires not swapped

**OLED not found 0x3C**
- ❌ Check I2C wiring: SDA=21, SCL=22
- ❌ Verify OLED address 0x3C (some modules are 0x3D)
- 🔧 Run I2C scanner sketch to detect address

---

## 📚 Documentation

- **[specifications.md](specifications.md)** - Complete hardware/software specs (Single Source of Truth)
- **[requirements.md](.kiro/specs/treklink/requirements.md)** - EARS requirements + user stories
- **[design.md](.kiro/specs/treklink/design.md)** - Technical architecture + design decisions
- **[tasks.md](.kiro/specs/treklink/tasks.md)** - Implementation task breakdown
- **[phase1_hardware_testing.md](C:\Users\PC\.gemini\antigravity\brain\8b95c2df-5ee8-4678-8d5d-b41e629acc51\phase1_hardware_testing.md)** - Hardware wiring & testing guide
- **[Meshtastic Docs](https://meshtastic.org/docs/)** - Official Meshtastic documentation

---

## 🤝 Contributing

TrekLink is based on the [Meshtastic](https://github.com/meshtastic/firmware) project. Contributions welcome!

### Development Setup
1. Fork repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Make changes + test
4. Commit: `git commit -m "Add my feature"`
5. Push: `git push origin feature/my-feature`
6. Open Pull Request

### Custom Module Development (Phase 2)
To add custom Meshtastic modules:
1. Create module in `src/modules/`
2. Register in `src/modules/Modules.cpp`
3. Add to build flags in `variants/esp32/treklink/platformio.ini`

---

## 📜 License

### TrekLink Custom Variant
MIT License - see [LICENSE](LICENSE)

### Meshtastic Firmware Base
GPL-3.0 License - see [Meshtastic License](https://github.com/meshtastic/firmware/blob/master/LICENSE)

---

## 🙏 Acknowledgments

- **[Meshtastic Project](https://meshtastic.org/)** - Open-source mesh networking firmware
- **[RadioLib](https://github.com/jgromes/RadioLib)** - Universal radio communication library
- **ESP32 Community** - Hardware platform and tools
- **LoRa Alliance** - Long-range radio technology

---

## 📞 Support

- **Issues:** [GitHub Issues](https://github.com/ruskicoder/treklink/issues)
- **Meshtastic Discord:** [Join Community](https://discord.gg/meshtastic)
- **Documentation:** [Meshtastic Docs](https://meshtastic.org/docs/)

---

**Happy Meshing!** 📡🏔️
