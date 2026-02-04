# TrekLink MVP - Development BOM (Breadboard/Perfboard Prototype)

> **Version:** 1.1 - **2 Devices for Mesh Testing**  
> **Date:** February 1, 2026  
> **Purpose:** Through-hole components for rapid prototyping and testing on breadboards/perfboards

---

## Core Modules (Pre-assembled Breakout Boards)

| Component | Part Number | Per Device | Total (x2) | Notes |
|-----------|-------------|------------|------------|-------|
| **ESP32 Development Board** | ESP32-WROOM-32 DevKit V1 (30-pin) | 1 | **2** | Through-hole pin headers, includes CP2102 USB-UART |
| **LoRa Module** | Ebyte E32-433T20D with breakout | 1 | **2** | 433MHz, 100mW, UART interface, with pin headers |
| **GPS Module** | Neo-6M with breakout board | 1 | **2** | Includes ceramic patch antenna, UART+I2C |
| **IMU Sensor** | MPU6050 GY-521 breakout | 1 | **2** | 6-axis accelerometer/gyroscope, I2C interface |
| **OLED Display** | 0.96" SSD1306 128x64 I2C module | 1 | **2** | Blue/White, 4-pin header (VCC, GND, SCL, SDA) |

> **Note:** ESP32 has built-in RTC that will be synchronized with GPS time - no external RTC module needed


---

## Power Management (Through-Hole Components)

| Component | Part Number/Spec | Per Device | Total (x2) | Notes |
|-----------|------------------|------------|------------|-------|
| **Battery Holder** | 2x 21700 parallel spring holder | 1 | **2** | Through-hole PCB mount or external wired |
| **Li-ion Cells** | 21700 5000mAh 3.7V (e.g., Samsung 50E) | 2 | **4** | Total 10,000mAh capacity per device |
| **Charger Module** | TP5100 2A dual-cell Li-ion charger | 1 | **2** | Breakout board with screw terminals |
| **Buck Converter** | Mini360 HM (DC-DC Step-Down) | 1 | **2** | Adjustable 3.3V output, sufficient for all modules |

---

## Power Gating & Switching (Through-Hole)

> **Note:** Power gating circuit will be tested on breadboard first (see design.md section 3.6.6 for wiring), then transferred to perfboard  
> **Configuration:** GPS uses P-MOSFET (high-side), OLED uses NPN transistor (low-side) for independent control in Silent mode

| Component | Part Number/Spec | Per Device | Total (x2 + spares) | Notes |
|-----------|------------------|------------|---------------------|-------|
| **P-Channel MOSFET** | **IRF9530N** (TO-220) | 1 | **3** | GPS power gating (2 devices + 1 spare) |
| **NPN Transistor (Gate Driver)** | S8050-D | 1 | **2** | For GPS MOSFET gate driver |
| **NPN Transistor (OLED Switch)** | S8050-D | 1 | **2** | For OLED low-side switching (Silent mode support) |
| **Pull-up Resistor** | 10kΩ (1/4W) | 1 | **3** | MOSFET gate pull-up (2 devices + 1 spare) |
| **Base Resistor** | 1kΩ (1/4W) | 2 | **5** | NPN base current limiting (GPS gate + OLED switch, +1 spare) |
| **Slide Switch** | SPDT 2-position slide (through-hole) | 1 | **2** | Power ON/OFF |

---

## User Interface Components (Through-Hole)

| Component | Part Number/Spec | Per Device | Total (x2) | Notes |
|-----------|------------------|------------|------------|-------|
| **Tactile Button** | 12x12mm through-hole momentary | 4 | **8** | MENU, SOS, UP, DOWN |
| **Passive Buzzer** | 9056-TS Buzzer 3.3V passive buzzer (12mm, through-hole) | 1 | **2** | Audio feedback - requires PWM (more flexible tones) |
| **Vibration Motor** | Coin vibration motor (10mm) with wires | 1 | **2** | Haptic feedback |
| **Status LED** | Blue LED 5mm (1.8-2V forward voltage) | 1 | **2** | Basic status indication |
| **LED Current Resistor** | 220Ω (1/4W) | 1 | **2** | For status LED current limiting |
| **Transistor (Motor Driver)** | S8050-D (TO-92) | 1 | **2** | For vibration motor switching (same as gate drivers) |
| **Flyback Diode** | 1N4001 | 1 | **2** | Vibration motor protection |

---

## Passive Components (Through-Hole)

| Component | Value | Per Device | Total (x2) | Notes |
|-----------|-------|------------|------------|-------|
| **Decoupling Capacitor** | 100nF (0.1µF) ceramic | 10 | **20** | One per module/IC (most modules include onboard, buy extras) |
| **Bulk Capacitor** | 100µF electrolytic (25V) Samwha | 3 | **6** | Power rail stabilization |
| **Bulk Capacitor** | **1000µF electrolytic (16V)** | 2 | **4** | Battery input, Mini360 output |
| **Pull-up Resistor** | 4.7kΩ (1/4W) | 2 | **4** | I2C bus (SDA, SCL) |
| **Button Pull-down** | 10kΩ (1/4W) | 4 | **8** | Button GPIO inputs |

---

## Antennas

| Component | Spec | Per Device | Total (x2) | Notes |
|-----------|------|------------|------------|-------|
| **LoRa Antenna** | 433MHz SMA male whip antenna | 1 | **2** | ~17cm length, 2dBi gain |
| **GPS Antenna** | Ceramic patch (built into Neo-6M module) | 1 | **2** | Included with breakout board |

---

## Connectors & Cables (Through-Hole)

| Component | Spec | Per Device | Total (x2) | Notes |
|-----------|------|------------|------------|-------|
| **Pin Headers** | 2.54mm male (40-pin strip) | 5 strips | **10** | For connecting modules to breadboard |
| **Female Headers** | 2.54mm female (40-pin strip) | 3 strips | **6** | Socket for modules |
| **Jumper Wires** | Male-to-Male (pack of 65+) | 1 pack | **2 packs** | Breadboard connections |
| **Jumper Wires** | Male-to-Female (pack of 40+) | 1 pack | **2 packs** | Module to breadboard |
| **USB-C Breakout** | USB Type-C female breakout board | 1 | **2** | For charging (if not using TP5100 micro-USB) |
| **Screw Terminals** | 2-pin 5mm pitch | 5 | **10** | Battery, power distribution |

---

## Breadboard & Prototyping

| Component | Spec | Per Device | Total (x2) | Notes |
|-----------|------|------------|------------|-------|
| **Breadboard** | 830-point solderless breadboard | 2 | **4** | Main + auxiliary for power distribution (2 per device) |
| **Perfboard** | 10x15cm single-sided FR4 | 2 | **4** | For semi-permanent prototype |
| **Standoffs** | M3 x 10mm nylon | 8 | **16** | Module mounting |
| **Screws** | M3 x 6mm | 8 | **16** | Module mounting |

---

## Tools & Consumables

| Item | Quantity | Notes |
|------|----------|-------|
| **Soldering Iron** | 1 | Temperature controlled, 300-400°C |
| **Solder Wire** | 1 roll | 60/40 or 63/37 tin-lead, 0.8mm diameter |
| **Solder Wick** | 1 | For desoldering |
| **Flux Pen** | 1 | Optional, for better solder joints |
| **Wire Stripper** | 1 | 22-30 AWG |
| **Multimeter** | 1 | For voltage/continuity checks |
| **Helping Hands** | 1 | PCB holder with magnifier |

---

## Estimated Cost (USD)

| Category | Unit Cost (1 device) | Total Cost (x2 devices) |
|----------|----------------------|------------------------|
| **Core Modules** | $30 - $40 | **$60 - $80** |
| **Power Components** | $20 - $25 | **$40 - $50** |
| **Power Gating & UI** | $8 - $12 | **$16 - $24** |
| **Passive Components** | $5 - $8 | **$10 - $16** |
| **Connectors & Proto** | $15 - $20 | **$30 - $40** |
| **Tools (one-time)** | $25 - $35 | **$25 - $35** |
| **Total** | ~$103 - $140 | **~$181 - $245** |

> Cost for 2 complete devices plus spares for mesh connectivity testing

---

## Assembly Notes

1. **Breadboard Phase:**
   - Assemble Device 1 completely
   - Test all peripherals (GPS, OLED, buttons, buzzer, vibration)
   - Assemble Device 2
   - Test mesh connectivity between devices

2. **Power Gating Testing:**
   - Test GPS power gating on Device 1
   - Test OLED switching for Silent mode
   - Verify battery life improvements

3. **Perfboard Phase:**
   - Transfer working circuits to perfboard
   - Compact layout for enclosure fitting

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.1 | 2026-02-01 | Updated for 2 devices (mesh testing), Silent mode support with independent GPS/OLED control |
| 1.0 | 2026-02-01 | Initial BOM |
