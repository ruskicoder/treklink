# TrekLink

> **ESP32 LoRa Mesh Communication Device for Off-Grid Adventures**

TrekLink is a decentralized, battery-powered mesh communication device designed for hiking, trekking, and remote area exploration where cellular networks are unavailable. It combines LoRa long-range radio, GPS tracking, and fall detection to keep adventurers connected and safe.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)

---

## 🚀 Quick Start (5 Minutes)

> **Choose Your IDE:** This project works with both **PlatformIO** (VS Code) and **pioarduino-ide** (OpenVSX/Antigravity).

### Option 1: PlatformIO (VS Code - Official)

**Prerequisites:**
- [VS Code](https://code.visualstudio.com/) + [PlatformIO Extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- ESP32 DevKit V1 + USB cable

**Steps:**
```bash
# 1. Install PlatformIO extension in VS Code
# 2. Clone repository
git clone https://github.com/[your-org]/treklink.git
cd treklink

# 3. Open in VS Code
code .

# 4. Build & Upload (PlatformIO toolbar or CLI)
pio run -t upload

# 5. Monitor serial output
pio device monitor
```

### Option 2: pioarduino-IDE (OpenVSX - Antigravity/VSCodium)

**Prerequisites:**
- VS Code with OpenVSX (Antigravity, VSCodium, etc.) + [pioarduino-ide Extension](https://open-vsx.org/extension/pioarduino/pioarduino-ide)
- ESP32 DevKit V1 + USB cable

**Steps:**
```bash
# 1. Install pioarduino-ide from OpenVSX
# 2. Clone repository
git clone https://github.com/[your-org]/treklink.git
cd treklink

# 3. Open in your IDE
# 4. Build & Upload (same commands as PlatformIO)
pio run -t upload

# 5. Monitor serial output
pio device monitor
```

> **Note:** pioarduino-ide is a **community fork** of PlatformIO with better ESP32 Arduino core v3 support. It uses the **same** `platformio.ini` config and commands. Projects are **100% compatible** between both tools.

**Expected Output:**
```
=================================
TrekLink ESP32 LoRa Mesh Device
Version: 1.0 MVP
=================================

[PowerGate] Initialized
  GPS Power: OFF
  OLED Power: ON
[ButtonHandler] Initialized
System initialized successfully
```

**That's it!** 🎉 Your ESP32 is now running TrekLink firmware.

---

## ✨ Features

### Current Implementation (Phase 1 - Complete)
- ✅ **PlatformIO Project Structure** - ESP32 development environment
- ✅ **GPIO Pin Definitions** - Complete hardware configuration
- ✅ **Power Gating Driver** - GPS & OLED power control for battery saving
- ✅ **Button Handler** - Debouncing, click, double-click, hold detection
- ✅ **Wokwi Simulation** - Test UI logic without hardware

### Planned Features (MVP Roadmap)
- 🔄 **LoRa Mesh Networking** - Managed flooding protocol
- 🔄 **GPS Tracking** - Location sharing with mesh nodes
- 🔄 **Fall Detection** - Automatic SOS alerts using MPU6050
- 🔄 **OLED Display** - Dashboard, map, message viewer
- 🔄 **Preset Messages** - Quick communication templates
- 🔄 **Silent Mode** - OLED/buzzer off, vibration-only alerts
- 🔄 **AES-128-GCM Encryption** - Secure mesh communication

---

## 🔧 Hardware Requirements

### Core Components (Per Device)

| Component | Part Number | Qty | Purpose |
|-----------|-------------|-----|---------|
| **MCU** | ESP32-WROOM-32 DevKit V1 (30-pin) | 1 | Main processor |
| **LoRa** | Ebyte E32-433T20D (433MHz, 100mW) | 1 | Long-range radio |
| **GPS** | Neo-6M with antenna | 1 | Location tracking |
| **IMU** | MPU6050 GY-521 breakout | 1 | Fall detection |
| **Display** | SSD1306 128x64 OLED (I2C) | 1 | User interface |
| **Battery** | 2x 21700 Li-ion (5000mAh, 3.7V) | 2 | ~10Ah capacity |
| **Charger** | TP5100 2A dual-cell charger | 1 | Battery charging |
| **Buck Converter** | Mini360 DC-DC (3.3V output) | 1 | Power regulation |

### User Interface

| Component | Qty | Notes |
|-----------|-----|-------|
| Tactile buttons (12x12mm) | 4 | MENU, SOS, UP, DOWN |
| Passive buzzer (3.3V, PWM) | 1 | Audio feedback |
| Coin vibration motor (10mm) | 1 | Haptic feedback |
| Slide switch (SPDT) | 1 | Power ON/OFF |
| Status LED (5mm blue) | 1 | Visual indicator |

### Power Gating Components

| Component | Qty | Purpose |
|-----------|-----|---------|
| IRF9530N P-MOSFET (TO-220) | 1 | GPS high-side switch |
| S8050-D NPN transistors (TO-92) | 2 | Gate driver + OLED switch |
| 10kΩ resistors (1/4W) | 5 | Pull-ups & pull-downs |
| 1kΩ resistors (1/4W) | 2 | Base current limiting |
| 4.7kΩ resistors (1/4W) | 2 | I2C pull-ups |

**📦 Full BOM:** See [BOM_Development.md](BOM_Development.md) for complete parts list with suppliers.

---

## 📌 Pin Connections

### ESP32 DevKit V1 Pinout

**⚠️ IMPORTANT:** ESP32 operates at **3.3V logic**. All connections shown below are 3.3V compatible.

| Module | ESP32 Pin | GPIO | Function | Notes |
|--------|-----------|------|----------|-------|
| **LoRa E32-433T20D** ||||
| E32 RXD | TX2 (R) | GPIO 17 | UART TX | ESP32 → LoRa |
| E32 TXD | RX2 (R) | GPIO 16 | UART RX | LoRa → ESP32 |
| E32 AUX | D27 (L) | GPIO 27 | Interrupt | Wake-on-Radio |
| E32 M0 | D18 (R) | GPIO 18 | Mode | LoRa mode control |
| E32 M1 | D19 (R) | GPIO 19 | Mode | LoRa mode control |
| E32 VCC | 3V3 | - | Power | **3.3V ONLY** |
| E32 GND | GND | - | Ground | Common GND |
| **GPS Neo-6M** ||||
| GPS TX | D14 (L) | GPIO 14 | RX | NMEA input |
| GPS VCC | - | - | Power | Via P-MOSFET (GPIO 13 control) |
| GPS GND | GND | - | Ground | Common GND |
| **I2C Bus (OLED + MPU6050)** ||||
| SDA | D21 (R) | GPIO 21 | I2C Data | 4.7kΩ pull-up required |
| SCL | D22 (R) | GPIO 22 | I2C Clock | 4.7kΩ pull-up required |
| **MPU6050 IMU** ||||
| MPU INT | D34 (L) | GPIO 34 | Interrupt | Fall detection wake |
| MPU VCC | 3V3 | - | Power | Always-on |
| MPU GND | GND | - | Ground | Common GND |
| **OLED Display** ||||
| OLED SDA | D21 (R) | GPIO 21 | I2C Data | Shared with MPU6050 |
| OLED SCL | D22 (R) | GPIO 22 | I2C Clock | Shared with MPU6050 |
| OLED VCC | 3V3 | - | Power | Always-on |
| OLED GND | D23 (R) | GPIO 23 | GND Switch | NPN low-side (Silent Mode) |
| **Buttons** ||||
| MENU | D25 (L) | GPIO 25 | Input | Active LOW + pull-down |
| SOS | D26 (L) | GPIO 26 | Input | Active LOW + pull-down |
| UP | D32 (L) | GPIO 32 | Input | Active LOW + pull-down |
| DOWN | D33 (L) | GPIO 33 | Input | Active LOW + pull-down |
| **Power Control** ||||
| Slide Switch | D4 (R) | GPIO 4 | Input | Power ON/OFF detect |
| GPS Power Gate | D13 (L) | GPIO 13 | Output | P-MOSFET gate control |
| OLED GND Switch | D23 (R) | GPIO 23 | Output | NPN base (Silent Mode) |
| CHRG Status | D35 (L) | GPIO 35 | Input | TP5100 charging indicator |
| **Audio/Haptic** ||||
| Buzzer | D12 (L) | GPIO 12 | PWM Output | Passive buzzer tone generation |
| Vibrator | D15 (R) | GPIO 15 | Output | Motor via NPN transistor |

**Legend:** (R) = Right side of board, (L) = Left side of board

**🔌 Wiring Diagrams:** See [design.md Section 3.6](/.kiro/specs/treklink/design.md#36-circuit-wiring-and-connections) for detailed breadboard wiring.

---

## 💻 Software Setup

### Choose Your Development Environment

**Option 1: PlatformIO (VS Code - Official)** - Recommended for VS Code users

**Option 2: pioarduino-IDE (OpenVSX)** - For Antigravity, VSCodium, Cursor, or other OpenVSX-based editors

Both use the **same project structure** and `platformio.ini` configuration file. All commands are identical.

---

### Install PlatformIO (VS Code)

1. Install [VS Code](https://code.visualstudio.com/)
2. Open Extensions (Ctrl+Shift+X)
3. Search "PlatformIO IDE"
4. Click Install
5. Reload VS Code

**CLI Alternative:**
```bash
python -m pip install platformio
```

---

### Install pioarduino-IDE (OpenVSX)

**For Antigravity, VSCodium, Cursor, etc.:**

1. Open Extensions
2. Search "pioarduino-ide" in OpenVSX
3. Click Install
4. Reload editor

**Direct Link:** https://open-vsx.org/extension/pioarduino/pioarduino-ide

> **Why pioarduino-ide?**  
> - ✅ Available on OpenVSX (official PlatformIO is not)
> - ✅ Better ESP32 Arduino Core v3 support
> - ✅ Same commands, same `platformio.ini`, 100% compatible
> - ✅ Community-maintained fork of PlatformIO

---

### Project Structure

```
treklink/
├── src/
│   ├── main.cpp              # Entry point
│   └── hal/                  # Hardware Abstraction Layer
│       ├── power_gate.cpp    # GPS/OLED power control
│       └── button_handler.cpp # Button debouncing & events
├── include/
│   ├── hardware_config.h     # GPIO pin definitions
│   └── hal/                  # HAL headers
├── lib/                      # Custom libraries (future)
├── test/                     # Unit tests (future)
├── platformio.ini            # PlatformIO configuration
├── wokwi.toml               # Wokwi simulator config
└── diagram.json             # Wokwi virtual circuit

Documentation:
├── .kiro/specs/treklink/
│   ├── requirements.md       # EARS-format requirements
│   ├── design.md            # Technical design & architecture
│   └── tasks.md             # Implementation task list
├── BOM_Development.md       # Hardware bill of materials
└── specifications.md        # Full system specifications
```

### Build & Upload

```bash
# Build firmware
pio run

# Upload to ESP32 (auto-detects port)
pio run -t upload

# Upload + Monitor serial output
pio run -t upload && pio device monitor

# Clean build
pio run -t clean
```

### Serial Monitor

```bash
# Start monitor (115200 baud)
pio device monitor

# Exit: Ctrl+C
```

**Expected Output:**
```
=================================
TrekLink ESP32 LoRa Mesh Device
Version: 1.0 MVP
=================================

[PowerGate] Initialized
  GPS Power: OFF
  OLED Power: ON
[ButtonHandler] Initialized
  Button config: Active LOW with pull-down
  Debounce: 50ms
  Double-click window: 300ms
  Hold threshold: 1000ms

System initialized successfully
```

---

## 🧪 Testing & Debugging

### Phase 1 Testing (Current)

**1. USB Power Test (No Battery Needed)**
```bash
# Connect ESP32 via USB
# Upload firmware
pio run -t upload && pio device monitor

# Verify startup messages appear
```

**2. OLED I2C Test**
```bash
# Connect OLED to D21 (SDA) and D22 (SCL)
# Add 4.7kΩ pull-up resistors to SDA/SCL
# Run I2C scanner (future task)
```

**3. Button Test**
```bash
# Connect buttons to GPIO 25, 26, 32, 33
# Press buttons and check serial output
# Verify: click, double-click, hold events
```

**4. Wokwi Simulation**
```bash
# Build firmware
pio run

# Open Wokwi simulator in VS Code
# Test buttons and OLED without hardware
```

### Power Gating Test (Requires Battery)

**⚠️ Not testable yet:** Power gating requires battery and full circuit assembly.

### Debug Tools

**Enable Verbose Logs:**
Edit `platformio.ini`:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  # Change from 3 to 5
```

**Check Compilation:**
```bash
pio run -v  # Verbose build output
```

**Firmware Size:**
```bash
pio run -t size
```

---

## 🐛 Troubleshooting

### Upload Fails

**Problem:** `Failed to connect to ESP32`

**Solutions:**
1. Hold BOOT button while uploading
2. Check USB cable (must support data, not just power)
3. Install CP2102 driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
4. Try different USB port
5. Check COM port: `pio device list`

### I2C Not Working

**Problem:** OLED not detected

**Solutions:**
1. Verify 4.7kΩ pull-up resistors on SDA (GPIO 21) and SCL (GPIO 22)
2. Check OLED I2C address: Default 0x3C (some use 0x3D)
3. Multiple I2C devices? Ensure no address conflicts
4. Run I2C scanner to detect devices

### Button Not Responding

**Problem:** Button presses not detected

**Solutions:**
1. Check button wiring: One side to GPIO, other to GND
2. Verify GPIO pin numbers in `hardware_config.h`
3. Buttons are **active LOW** (pressed = GND)
4. Internal pull-down enabled in code (no external resistor needed)

### Compilation Errors

**Problem:** `fatal error: Arduino.h: No such file or directory`

**Solution:**
```bash
# Reinstall ESP32 platform
pio platform install espressif32 --force
```

**Problem:** Library not found

**Solution:**
```bash
# Install libraries manually
pio lib install "Adafruit SSD1306"
pio lib install "Adafruit GFX Library"
```

### Serial Monitor Garbage

**Problem:** Unreadable characters

**Solutions:**
1. Set baud rate: `pio device monitor -b 115200`
2. ESP32 boot message at 74880 baud is normal (ignore)
3. Check COM port settings

---

## 📚 Documentation

- **[Requirements](/.kiro/specs/treklink/requirements.md)** - Functional requirements (EARS format)
- **[Design Document](/.kiro/specs/treklink/design.md)** - Architecture, circuits, wiring
- **[Task List](/.kiro/specs/treklink/tasks.md)** - Implementation checklist
- **[Specifications](specifications.md)** - Complete system specifications
- **[BOM](BOM_Development.md)** - Hardware bill of materials

---

## 🛣️ Development Roadmap

### Phase 1: Setup & HAL ✅ (Complete)
- [x] PlatformIO project structure
- [x] Wokwi simulation config
- [x] GPIO pin definitions
- [x] Power gating driver
- [x] Button handler

### Phase 2: LoRa Communication (In Progress)
- [ ] LoRa E32 driver
- [ ] Packet structure
- [ ] AES-128-GCM encryption
- [ ] PRFH routing protocol
- [ ] Managed flooding mesh
- [ ] Reed-Solomon FEC

### Phase 3: Sensors & Services
- [ ] GPS service (TinyGPSPlus)
- [ ] IMU service (fall detection)
- [ ] Storage service (NVS)

### Phase 4: Power Management
- [ ] Deep sleep modes
- [ ] Silent Mode
- [ ] Battery monitoring

### Phase 5-7: UI & Features
- [ ] OLED display driver
- [ ] Dashboard, map, menu
- [ ] Preset messages
- [ ] SOS logic

---

## 🤝 Contributing

This is a university project (EXE101-G1-TREKLINK). Contributions are welcome!

1. Fork the repository
2. Create feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open Pull Request

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 👥 Team

**EXE101-G1-TREKLINK**  
FPT University - Embedded Systems Project

---

## 🙏 Acknowledgments

- **Ebyte** - E32 LoRa module
- **Espressif** - ESP32 microcontroller
- **PlatformIO** - Development platform
- **Adafruit** - Display libraries
- **TinyGPS++** - GPS parsing library

---

**📡 Stay Connected. Stay Safe. Trek On!** 🏔️
