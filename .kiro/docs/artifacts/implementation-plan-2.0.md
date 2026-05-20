# TrekLink v2.0 — Hardware Specification (Revised)

> **Revision:** 2.0 (incorporating user answers)
> **Date:** 2026-05-14
> **Budget Ref:** `actual-available-funds.md` — 49,803,700₫ total project
> **PCB Service Cap:** 13,000,000₫ (~$520 USD) for design service
> **Batch:** 7 units, 1 revision cycle
> **Status:** AWAITING USER CONFIRMATION ON 6 DECISIONS

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    TrekLink v2.0 PCB                        │
│                                                             │
│  ESP32-S3-WROOM-1 (N8R8)    ←SPI→   E22-400M22S (SX1262)  │
│          │                              │                   │
│          ├─UART1→ NEO-M9N GPS          │← SMA 433MHz       │
│          ├─I2C→ ICM-20948 IMU          │                   │
│          ├─I2C→ SSD1306 OLED           │                   │
│          ├─GPIO→ 4× Buttons            │                   │
│          ├─PWM→ Buzzer                 │                   │
│          ├─GPIO→ Vibrator (via NPN)    │                   │
│          ├─GPIO→ Status LED            │                   │
│          └─ADC→ Battery divider        │                   │
│                                                             │
│  BQ24074 ←USB-C→ TPS63802 → 3.3V rail → all modules       │
│      │                                                      │
│  DW01A+FS8205A → 21700 battery (1S2P, 10Ah)               │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Component Selection (Final)

### 2.1 MCU — ESP32-S3-WROOM-1-N8R8

| Spec | Value |
|------|-------|
| **Core** | Xtensa LX7, dual-core 240MHz |
| **Flash/PSRAM** | 8MB / 8MB |
| **Bluetooth** | BT 5.0 LE |
| **USB** | Native USB-OTG (no CP2102 needed) |
| **Package** | Castellated SMD (18×25.5×3.2mm) |
| **LCSC** | C2913202 (Extended) |
| **JLCPCB PCBA** | ✅ Supported |
| **Price** | ~80,000₫/unit |

**Firmware migration:** Create `variants/esp32s3/treklink_v2_0/` based on EBYTE_ESP32-S3 variant template. Change from `esp32_base` to `esp32s3_base`. All pin mappings via `variant.h`.

---

### 2.2 LoRa Radio — CDEBYTE E22-400M22S (SX1262, SPI)

> [!IMPORTANT]
> **NOT the E22-400T22D** (that's UART). The M22S variant is the **SPI** version required by Meshtastic/RadioLib.

| Spec | Value |
|------|-------|
| **Chip** | SX1262 (Semtech) |
| **Interface** | SPI (NSS, SCK, MISO, MOSI, DIO1, BUSY, NRST) |
| **Frequency** | 410–493 MHz (covers VN_433) |
| **TX Power** | +22 dBm (158mW) |
| **RX Current** | ~5mA (vs 12mA on SX1278) |
| **Package** | Castellated SMD (16×16mm) |
| **TCXO** | Internal, controlled via DIO3 (1.8V) |
| **RF Switch** | DIO2-controlled (SX126X_DIO2_AS_RF_SWITCH) |

**Why E22-400M22S specifically:**
- Direct SPI access to SX1262 registers (not UART AT-command)
- Same form factor & pinout as E22-900M22S already supported in Meshtastic codebase (EBYTE_ESP32-S3 variant)
- Castellated = PCBA-compatible
- CDEBYTE is the same company, well-documented

**SPI Pin Mapping (E22-400M22S → ESP32-S3):**

| E22 Pin | Function | ESP32-S3 GPIO (Proposed) |
|---------|----------|--------------------------|
| NSS | Chip Select | GPIO 14 |
| SCK | SPI Clock | GPIO 21 |
| MOSI | Data In | GPIO 38 |
| MISO | Data Out | GPIO 39 |
| NRST | Reset | GPIO 40 |
| BUSY | Status | GPIO 41 |
| DIO1 | IRQ | GPIO 42 |
| DIO2 | RF Switch (auto) | — (internal) |
| DIO3 | TCXO control | — (internal, 1.8V) |

**variant.h excerpt:**
```cpp
#define USE_SX1262
#define SX126X_CS 14
#define LORA_SCK 21
#define LORA_MOSI 38
#define LORA_MISO 39
#define SX126X_RESET 40
#define SX126X_BUSY 41
#define SX126X_DIO1 42
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define SX126X_MAX_POWER 22
#define LORA_CS SX126X_CS
#define LORA_DIO1 SX126X_DIO1
```

---

### 2.3 GPS — u-blox NEO-M9N (Module Breakout)

| Spec | Value |
|------|-------|
| **Constellations** | GPS + GLONASS + BeiDou + Galileo (4 concurrent) |
| **Accuracy** | 1.5m CEP |
| **Sensitivity** | -167 dBm |
| **Anti-Jamming** | Yes |
| **Interface** | UART 9600/115200 baud |
| **Backup Battery** | Coin cell on breakout for hot start |
| **Price** | ~350,000₫/unit (genuine u-blox module) |

**ESP32-S3 UART mapping:**

| GPS Pin | ESP32-S3 GPIO |
|---------|---------------|
| TX | GPIO 16 (ESP32-S3 RX) |
| RX | GPIO 17 (ESP32-S3 TX) |
| EN | GPIO 15 (power enable) |

No firmware change needed — same NMEA protocol, same `GPS_UBLOX` define.

---

### 2.4 IMU — ICM-20948 (GY-ICM20948 Breakout for v2.0)

| Spec | Value |
|------|-------|
| **Axes** | 9 (Accel + Gyro + Magnetometer AK09916) |
| **Power** | ~1.2mA (vs 3.9mA MPU6050) |
| **I2C Address** | 0x68 (same as MPU6050, no bus conflict with OLED 0x3C) |
| **DMP** | Advanced (offloads sensor fusion) |
| **Step Counter** | Hardware |
| **Price** | ~70,000₫/unit |

**Firmware:** Meshtastic already includes `SparkFun 9DoF IMU Breakout - ICM 20948 - Arduino Library@1.3.2` in `platformio.ini`. Support is built-in.

**Benefit for TrekLink:** Magnetometer enables **compass heading** for the REQ-NAV-03 direction/distance display → users see which direction group members are relative to their facing.

**I2C mapping:**

| Signal | ESP32-S3 GPIO |
|--------|---------------|
| SDA | GPIO 5 |
| SCL | GPIO 6 |

---

### 2.5 Display — SSD1306 0.96" I2C OLED (Keep)

Same as v1.0. On I2C bus at 0x3C. Connected via 4-pin header for replaceability.

> [!NOTE]
> For v3.0/v4.0, consider SH1106 1.3" or color TFT. For v2.0 the priority is **firmware compatibility, not display size**.

---

### 2.6 Power System (Custom PCB, Full PCBA)

#### TPS63802 Buck-Boost Regulator

| Spec | Value |
|------|-------|
| **IC** | TPS63802DLAT (TI) |
| **Package** | VSON-10 (3×2mm) |
| **Input** | 1.3–5.5V (full Li-Ion range) |
| **Output** | 3.3V (set via feedback divider) |
| **Current** | 2A max |
| **Efficiency** | 90–96% |
| **Quiescent** | 11µA |
| **LCSC** | Available (Extended) |

**External components:** 0.47µH inductor, 22µF output cap, feedback resistor divider (per TI WEBENCH).

#### BQ24074 Battery Charger + Power Path

| Spec | Value |
|------|-------|
| **IC** | BQ24074RGTR (TI) |
| **Package** | QFN-16 (3.5×3.5mm) |
| **Input** | USB-C 5V (via CC resistors) |
| **Charge Current** | Programmable via ISET resistor |
| **Power Path** | ✅ Load + charge simultaneously |
| **LCSC** | C54313 |

**USB-C connector:** USB4105-GF-A (GCT) or equivalent. Must include 2× 5.1kΩ CC pull-downs.

#### Battery Protection — DW01A + FS8205A

Standard Li-Ion protection combo. Over-discharge, over-charge, short circuit protection. Both available on LCSC as basic parts.

---

## 3. Physical Stack-Up Analysis

> [!WARNING]
> **Critical constraint: 21700 battery diameter = 21mm.** This is the tallest component.

### Height Budget (30mm max case thickness)

```
┌─── Top Shell ─────────────────────────── 1.5mm (wall)
│    ← OLED + button clearance             3.0mm
│    ← PCB thickness                       1.6mm (4-layer)
│    ← Component clearance (bottom side)   2.0mm (SMD passives)
│    ← Air gap / tolerance                 0.9mm
│    ← 21700 battery                      21.0mm (diameter)
│    ← Battery holder spring clearance     0.5mm (thin contacts)
└─── Bottom Shell ──────────────────────── 1.5mm (wall)
                                    TOTAL: 32.0mm ← OVER BUDGET
```

> [!IMPORTANT]
> **Problem:** Stacking PCB on top of 21700 battery exceeds 30mm.

### Solutions (pick one):

**Option A: Side-by-Side Layout (Recommended)**
- Battery placed **beside** the PCB, not underneath
- PCB + battery share the same horizontal plane
- Case profile: ~23mm thick (21mm battery + 2mm shell)
- **Tradeoff:** Wider device (~80mm), but thinner
- Form factor: 115×80×23mm

```
┌──────────────────────────────────────┐
│  ┌───────────┐  ┌──────────────────┐ │
│  │ 21700 ×2  │  │      PCB         │ │ ← 23mm thick
│  │ (parallel)│  │  ESP32-S3        │ │
│  │           │  │  SX1262          │ │
│  └───────────┘  │  GPS, IMU, etc.  │ │
│                 └──────────────────┘ │
└──────────────────────────────────────┘
        115mm × 80mm × 23mm
```

**Option B: Stacked with Cutout**
- Battery sits in a **recessed pocket** in the bottom shell
- PCB sits on a shelf above the battery
- Internal clearance ~28mm (21mm battery + 1.6mm PCB + 5mm components + tolerance)
- Case: 100×75×30mm (maxes out budget)

**Option C: Battery Rotated 90°**
- Battery oriented with length along the case width (70mm battery in 80mm case width)
- PCB stacks on top of battery at 90°
- Needs ~27mm clearance
- Case: 100×80×28mm

---

## 4. Budget Analysis (VND)

### Per-Unit Component Cost (×7 units)

| Component | Unit Price | ×7 Total | Source |
|-----------|-----------|----------|--------|
| ESP32-S3-WROOM-1-N8R8 | 80,000₫ | 560,000₫ | LCSC |
| E22-400M22S (SX1262 SPI) | 95,000₫ | 665,000₫ | CDEBYTE/Thegioiic |
| NEO-M9N GPS module | 350,000₫ | 2,450,000₫ | Thegioiic/AliExpress |
| ICM-20948 (GY breakout) | 70,000₫ | 490,000₫ | Thegioiic |
| SSD1306 0.96" OLED | 44,000₫ | 308,000₫ | Shopee |
| BQ24074RGTR | 55,000₫ | 385,000₫ | LCSC |
| TPS63802DLAT | 45,000₫ | 315,000₫ | LCSC |
| DW01A + FS8205A | 5,000₫ | 35,000₫ | LCSC |
| USB-C connector | 8,000₫ | 56,000₫ | LCSC |
| Samsung 50E 21700 (×2/unit) | 104,000₫ | 728,000₫ | Shopee |
| 21700 holder (2P) | 43,000₫ | 301,000₫ | Shopee |
| 433MHz SMA antenna | 42,000₫ | 294,000₫ | Thegioiic |
| Buttons (4×12mm) + switch | 10,000₫ | 70,000₫ | Thegioiic |
| Buzzer + vibrator | 12,000₫ | 84,000₫ | Thegioiic |
| SMD passives (resistors, caps, diodes) | ~20,000₫ | 140,000₫ | LCSC (PCBA) |
| **Subtotal Components** | | **6,881,000₫** | |

### PCB + PCBA Cost (JLCPCB, 7 boards)

| Item | Cost |
|------|------|
| PCB fabrication (4-layer, 100×80mm, 7pcs) | ~750,000₫ |
| PCBA assembly (SMD passives + ICs) | ~1,400,000₫ |
| Component sourcing (LCSC basic+extended) | Included in component cost |
| Shipping (to Vietnam) | ~500,000₫ |
| **Subtotal PCB+PCBA** | **~2,650,000₫** |

### 3D Print Enclosure (Vietnam local)

| Item | Cost |
|------|------|
| PETG enclosure (7 units, 2 parts each) | ~1,050,000₫ |
| **Subtotal Enclosure** | **~1,050,000₫** |

### Total v2.0 Hardware Budget

| Category | Amount |
|----------|--------|
| Components (7 units) | 6,881,000₫ |
| PCB + PCBA | 2,650,000₫ |
| 3D Print enclosure | 1,050,000₫ |
| Shipping & consumables | 1,485,780₫ |
| Contingency (15%) | 1,810,017₫ |
| **TOTAL HARDWARE** | **~13,877,000₫** |

Against funds allocation:
- Core Module: 8,573,950₫ ✅
- Power: 3,760,260₫ ✅
- PCB Fab: 2,058,000₫ ✅
- Enclosure: 2,656,500₫ ✅
- PCB Design Service: 13,000,000₫ (separate line item)

**Budget status:** Within allocated funds. The NEO-M9N is the most expensive single component — if budget is tight, can fall back to **NEO-M8N** (~180,000₫) saving ~1,190,000₫ across 7 units.

---

## 5. Firmware Variant Migration

### File Structure

```
variants/esp32s3/treklink_v2_0/
├── variant.h           ← Pin definitions (NEW)
├── platformio.ini      ← Build config (NEW)
└── pins_arduino.h      ← Arduino pin mappings (NEW)
```

### Key `variant.h` Defines

```cpp
// TrekLink v2.0 ESP32-S3 + SX1262 Custom Variant
#define TREKLINK_VARIANT
#define TREKLINK_V2

// Radio: SX1262 via E22-400M22S
#define USE_SX1262
#define SX126X_CS 14
#define LORA_SCK 21
#define LORA_MOSI 38
#define LORA_MISO 39
#define SX126X_RESET 40
#define SX126X_BUSY 41
#define SX126X_DIO1 42
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define SX126X_MAX_POWER 22
#define LORA_CS SX126X_CS
#define LORA_DIO1 SX126X_DIO1

// I2C (OLED 0x3C + ICM-20948 0x68)
#define I2C_SDA 5
#define I2C_SCL 6

// GPS (UART)
#define HAS_GPS 1
#define GPS_UBLOX
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define PIN_GPS_EN 15
#define GPS_EN_ACTIVE 1

// Display
#define HAS_SCREEN 1
#define USE_SSD1306

// Buttons
#define BUTTON_PIN 0      // MENU (BOOT button w/ pull-down)
#define BUTTON_PIN_SOS 4  // SOS
#define BUTTON_PIN_UP 7   // UP
#define BUTTON_PIN_DOWN 8 // DOWN

// Notifications
#define PIN_BUZZER 11
#define PIN_VIBRATOR 12
#define PIN_VIBRATION PIN_VIBRATOR
#define LED_PIN 2

// Battery
#define BATTERY_PIN 1
#define ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define ADC_MULTIPLIER 2.0

// Region
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433
#define DEFAULT_TIMEZONE "ICT-7"
```

### `platformio.ini`

```ini
[env:treklink_v2]
extends = esp32s3_base
board = esp32-s3-devkitc-1    ; or custom board JSON for N8R8
board_level = extra
build_flags =
  ${esp32s3_base.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V2
  -I variants/esp32s3/treklink_v2_0
  -D HAS_SCREEN=1
  -D USE_CANNED_MESSAGE_MODULE=1
  -D MESHTASTIC_EXCLUDE_POWER_TELEMETRY=1
  -D MESHTASTIC_EXCLUDE_MQTT=1
lib_deps =
  ${esp32s3_base.lib_deps}
  ${networking_base.lib_deps}
  ${radiolib_base.lib_deps}
  ${environmental_base.lib_deps}
```

---

## 6. PCB Design Service Deliverables

For JLCPCB/PCBWay or Flux.ai to produce the PCB, you need to provide:

### Schematic (EasyEDA Pro or KiCad export)
- All component connections as specified in this document
- Power tree (USB-C → BQ24074 → Battery → TPS63802 → 3.3V rail)
- SPI bus (ESP32-S3 ↔ E22-400M22S)
- UART (ESP32-S3 ↔ NEO-M9N)
- I2C bus with pull-ups (OLED + ICM-20948)
- Button matrix with pull-ups/downs
- Notification outputs (buzzer PWM, vibrator NPN driver, LED)
- Battery ADC voltage divider
- ESD protection on USB-C
- Decoupling caps on every IC VCC pin

### PCB Layout Requirements
- **Layers:** 4 (Signal-GND-Power-Signal)
- **Size:** ≤100×70mm (to fit in enclosure)
- **Impedance:** 50Ω controlled for SMA-to-SX1262 RF trace
- **Ground plane:** Solid on layer 2 (critical for RF and EMI)
- **Keep-out zones:** GPS antenna area, LoRa antenna area
- **Module footprints:** Castellated pads for ESP32-S3 and E22-400M22S
- **Through-hole footprints:** GPS module, IMU module, OLED header, buttons, SMA, battery connector
- **Mounting holes:** 4× M3, aligned to enclosure standoffs
- **Silkscreen:** Component labels, TrekLink branding, version marking

---

## Decisions Required

> [!IMPORTANT]
> These 6 decisions determine the final PCB layout and enclosure design.

### D1: Physical Layout — Side-by-Side vs Stacked?
Given the 21mm battery diameter and 30mm max thickness:
- **Option A (Recommended):** Side-by-side → 115×80×23mm (thinner, wider)
- **Option B:** Stacked with cutout → 100×75×30mm (compact, maxes height)
- **Option C:** Battery rotated 90° → 100×80×28mm (compromise)

### D2: GPS Module — NEO-M9N vs NEO-M8N?
- NEO-M9N: 350,000₫/unit, 4-constellation, anti-jamming (premium tier)
- NEO-M8N: 180,000₫/unit, 3-constellation, no anti-jamming (saves 1.19M₫ total)
- Your answers say "premium" → **NEO-M9N confirmed?**

### D3: Full PCBA Scope
You said "prioritize full PCBA." This means JLCPCB assembles:
- ✅ All SMD passives (resistors, caps, diodes)
- ✅ TPS63802 + BQ24074 + DW01A + FS8205A (power ICs)
- ✅ NPN transistors (SOT-23)
- ✅ USB-C connector
- ✅ ESP32-S3-WROOM-1 (castellated)
- ✅ E22-400M22S (castellated)
- ❓ **Should GPS, IMU, OLED, buttons also be PCBA?** These are typically through-hole modules that need to be hand-soldered. Unless we use castellated/SMD variants.

### D4: PCB Design Tool — EasyEDA Pro or Flux.ai?
- EasyEDA Pro: Free, JLCPCB-integrated, faster ordering
- Flux.ai: AI-assisted, more modern, but requires JLCPCB Gerber export
- **Which will you use for v2.0?** (affects how I prepare the schematic spec)

### D5: Antenna Strategy
- **SMA external whip** (17.5cm, current v1.0 approach) — best range, bulky
- **IPEX → SMA pigtail** — module uses IPEX, pigtail to SMA on case
- **PCB trace antenna** — saves cost, shorter range, complex design
- **Recommendation:** IPEX from E22 → SMA pigtail through case wall. Confirm?

### D6: V1.0 Backward Compatibility
TrekLink v2.0 uses different radio chip (SX1262 vs SX1278) and different MCU (ESP32-S3 vs ESP32). However:
- Both devices use **Meshtastic firmware** → they **CAN communicate** via LoRa mesh (same protocol, same encryption, same frequency)
- They just can't share the same firmware binary
- **Is inter-device communication between v1.0 and v2.0 required for testing?** (it should work out-of-the-box if both are on VN_433)

Q7: OLED on top face or front edge?
Q8: SOS button differentiation (color/guard)?
Q9: 2× 21700 (10Ah, thicker) vs 1× 21700 (5Ah, slimmer)?
Q10: Charging current limit (500mA / 1A / 1.5A)?


Answer:

Q1: Side-by-side vertical meaning: battery lower compartment, sit horizontally, pcb is single, sit higher (top down look). Please reference the concept-layout.png, it only sample, not actual individual component position

Q2: NEO-M9N

Q3: GPS, IMU, OLED, Buttons is PCBA

Q4: EasyEDA Pro & KiCad. I will hire freelancers

Q5: IPEX -> SMA pigtail. this ensures any force to the antenna will not damage the PCB

Q6: Firmware not need to be exactly the same variant, but they must be able to communicate to each other. (yes defaults are still on 433 band)

Q7: OLED on top face (like a phone display)

Q8: SOS button recessed design, with guard

Q9: 2x 21700 (10Ah)

Q10: any is fine, recommend 1A charging.