# TrekLink MVP Requirements Specification

## Introduction
TrekLink is an off-grid, decentralized communication device designed for hikers, skiers, and outdoor enthusiasts. It creates a local mesh network using LoRa technology to enable text messaging, location tracking, and SOS alerts without reliance on cellular infrastructure. This document defines the comprehensive functional and non-functional requirements for the TrekLink MVP, serving as the Single Source of Truth (SSOT) for development.

## Requirements

### 1. Communication & Networking (REQ-COM)

#### REQ-COM-01: Mesh Network Topology
**User Story:** As a user, I want my messages to reach their destination even if I don't have a direct line of sight, so that I can communicate reliably in complex terrain.
**Acceptance Criteria:**
1. **WHEN** a packet is received for another node, **THEN** the system **SHALL** re-broadcast it immediately if the TTL (Hop Count) is > 0.
2. **IF** a packet has already been seen (based on Msg ID), **THEN** the system **SHALL** drop the packet to prevent broadcast storms.
3. **WHILE** the device is on, **THEN** it **SHALL** listen for incoming LoRa packets according to the defined wake cycle.

#### REQ-COM-02: Packet Handling & Protocol
**User Story:** As a developer, I want an optimized packet structure, so that airtime is minimized and battery life is maximized.
**Acceptance Criteria:**
1. **WHEN** transmitting a message, **THEN** the system **SHALL** perform Forward Error Correction (FEC) using Reed-Solomon encoding to handle packet corruption.
2. **IF** the payload exceeds the packet limit, **THEN** the system **SHALL** reject the message (MVP does not support fragmentation).
3. **WHEN** a packet is received with `RSSI + (SNR * Factor)` below the defined threshold, **THEN** the system **SHALL** drop the packet (Link Quality Routing).

#### REQ-COM-03: Message Types
**User Story:** As a user, I want different ways to communicate, so that I can send quick updates or urgent alerts.
**Acceptance Criteria:**
1. **WHEN** the user selects a predefined message, **THEN** the system **SHALL** broadcast it to the target ID.
2. **WHEN** an SOS is triggered, **THEN** the system **SHALL** broadcast a high-priority packet containing GPS coordinates to `0xFFFF` (Broadcast Address).
3. **WHEN** a "Matrix Check" is requested (double-click SOS), **THEN** the system **SHALL** broadcast a ping and record responses to build a neighbor list.

### 2. Navigation & Location (REQ-NAV)

#### REQ-NAV-01: GPS Position Tracking
**User Story:** As a user, I want to know where I am and where my friends are, so that we don't get lost.
**Acceptance Criteria:**
1. **WHILE** in Active Mode, **THEN** the system **SHALL** power the Neo-6M GPS module and attempt to acquire a fix.
2. **IF** a fix is acquired, **THEN** the system **SHALL** store the Latitude and Longitude.
3. **WHEN** sending a heartbeat or message, **THEN** the system **SHALL** include the latest valid GPS coordinates.

#### REQ-NAV-02: Distance & Direction Display
**User Story:** As a user, I want to see how far away my group members are, so that I can gauge travel time.
**Acceptance Criteria:**
1. **WHEN** viewing the "Map" screen, **THEN** the system **SHALL** display relative positions of other nodes.
2. **IF** a target node is selected, **THEN** the system **SHALL** calculate and display the distance (in meters/km) using the Haversine formula.

### 3. Safety & Emergency (REQ-SAF)

#### REQ-SAF-01: Fall Detection
**User Story:** As a user, I want the device to call for help if I fall and become unresponsive, so that I can be rescued even if incapacitated.
**Acceptance Criteria:**
1. **WHEN** the MPU6050 detects freefall followed by a high-G impact and user inactivity, **THEN** the system **SHALL** enter a "Pre-Alarm" state.
2. **WHILE** in "Pre-Alarm" state, **THEN** the system **SHALL** sound a local alarm and vibrate for 30 seconds.
3. **IF** the user does not cancel the alarm within 30 seconds (Double Click), **THEN** the system **SHALL** automatically trigger an SOS broadcast.
4. **WHEN** the SOS logic is active (Auto or Manual), the user **MUST** Hold the SOS button for 5 seconds to Cancel the emergency state.

#### REQ-SAF-02: SOS Activation
**User Story:** As a user, I want a quick way to signal distress, so that I can get help immediately.
**Acceptance Criteria:**
1. **WHEN** the user holds the SOS button for > 3 seconds, **THEN** the system **SHALL** activate SOS mode and broadcast distress signals.
2. **WHILE** in SOS mode, **THEN** the system **SHALL** transmit continuously for 1 minute, then switch to a pulsed interval (every 30s) indefinitely until cancelled or battery depletion.

### 4. Security & Stealth (REQ-SEC)

#### REQ-SEC-01: Encryption
**User Story:** As a user, I want my communications to be private, so that eavesdroppers cannot read my messages or track my location.
**Acceptance Criteria:**
1. **WHEN** forming a packet, **THEN** the system **SHALL** encrypt the payload using AES-128-GCM with a pre-shared key.
2. **IF** a received packet fails decryption, **THEN** the system **SHALL** discard it silently.

#### REQ-SEC-02: Anti-Jamming (PRFH)
**User Story:** As a tactical user, I want communication to persist even if someone tries to jam the frequency.
**Acceptance Criteria:**
1. **WHILE** operating, **THEN** the system **SHALL** follow a Pseudo-Random Frequency Hopping (PRFH) schedule.
2. **WHEN** the hop interval expires, **THEN** the system **SHALL** retune the LoRa module to the next frequency channel determined by the LCG algorithm seeded by the Shared Key.
3. **IF** the RTC timestamp drifts, **THEN** the system **SHALL** attempt to re-sync with the network timeleader (future enhancement, MVP uses manual sync or loose tolerance).

#### REQ-SEC-03: Stealth / Silent Mode
**User Story:** As a user, I want to operate the device without making noise or light, so that I remain undetected.
**Acceptance Criteria:**
1. **WHEN** the user initiates "Silent Mode" (via Menu hold or Switch), **THEN** the system **SHALL** disable the OLED display, Status LED, and Buzzer.
2. **WHILE** in Silent Mode, **THEN** the system **SHALL** use haptic vibration for all alerts and confirmations.
3. **IF** adaptive power control is active, **THEN** the system **SHALL** output the minimum RF power required to reach the next hop.

### 5. Power Management (REQ-PWR)

#### REQ-PWR-01: Smart Sleep & Duty Cycle
**User Story:** As a user, I want the battery to last for several days, so that I don't need to charge it during a trip.
**Acceptance Criteria:**
1. **WHILE** idle, **THEN** the ESP32 **SHALL** enter Deep Sleep mode.
2. **WHEN** the LoRa module (in Wake-Up mode) detects a preamble, **THEN** it **SHALL** wake the ESP32 via the AUX pin.
3. **IF** no activity occurs for a defined timeout, **THEN** the system **SHALL** return to sleep.

#### REQ-PWR-02: Power Gating
**User Story:** As a developer, I want to cut power to unused sensors, so that leakage current is eliminated.
**Acceptance Criteria:**
1. **WHEN** the GPS is not required (e.g., inside sleep cycle), **THEN** the system **SHALL** disable the GPS MOSFET.
2. **WHEN** the OLED is off, **THEN** the system **SHALL** disable the OLED MOSFET.
3. **WHEN** the device is switched OFF via the physical slide switch, **THEN** the system **SHALL** enter a hard-off state (< 10uA consumption).

### 6. User Interface (REQ-UI)

#### REQ-UI-01: Dashboard & Information
**User Story:** As a user, I want to see the system status at a glance, so that I know my connectivity and battery level.
**Acceptance Criteria:**
1. **WHEN** the display is on, **THEN** the Main Dashboard **SHALL** show: Signal Strength (RSSI), Active Nodes Count, Battery Level, and Current Time.
2. **IF** valid GPS data is available, **THEN** the system **SHALL** display an indicator or coordinate snippet.

#### REQ-UI-02: Controls
**User Story:** As a user, I want simple physical controls, so that I can operate the device with gloves.
**Acceptance Criteria:**
1. **WHEN** the "Menu" button is pressed, **THEN** the system **SHALL** cycle through available screens (Dashboard, Map, Messages, Settings).
2. **WHEN** "Up" or "Down" buttons are pressed, **THEN** the system **SHALL** scroll through lists or adjust zoom on the map.
3. **WHEN** the "SOS" button is single-clicked, **THEN** the system **SHALL** send a standard "Ping".

### 7. Hardware Specifications (REQ-HW)

#### REQ-HW-01: Components & Layout
**User Story:** As a hardware engineer, I want a defined BOM, so that the device works as designed.
**Acceptance Criteria:**
1. **The system SHALL** utilize the ESP32-WROOM-32 as the main MCU.
2. **The system SHALL** utilize the Ebyte E32-433T20D for LoRa communication.
3. **The system SHALL** use 2x 21700 Li-ion cells in parallel for power.
4. **The system SHALL** include a Neo-6M GPS and MPU6050 Accelerometer.
5. **The system SHALL** use an AO3401 P-Channel MOSFET for high-side switching of peripherals.

#### REQ-HW-02: Physical Enclosure
**User Story:** As a user, I want a rugged device, so that it survives outdoor conditions.
**Acceptance Criteria:**
1. **The enclosure SHALL** be rated for IP67 (dust tight, immersion up to 1m).
2. **The enclosure SHALL** have dimensions approximately 125x80x32mm.
3. **The design SHALL** place both the GPS and LoRa antennas on the top face of the enclosure, separated to opposite corners to minimize near-field interference.

## Conclusion
These requirements encompass the full scope of the TrekLink MVP as defined in the technical specifications. Adherence to these criteria ensures the delivery of a robust, secure, and energy-efficient off-grid communication device.

### 8. Development Environment (REQ-ENV)

#### REQ-ENV-01: Framework & Language
**User Story:** As a developer, I want to use standard tools, so that the code is maintainable and compatible with libraries.
**Acceptance Criteria:**
1. **The system SHALL** be developed in C++ (C++17 standard where supported).
2. **The system SHALL** utilize the Arduino Framework for ESP32 (latest stable release).

#### REQ-ENV-02: Build System
**User Story:** As a developer, I want a reproducible build environment, so that anyone can compile the project.
**Acceptance Criteria:**
1. **The system SHALL** use PlatformIO Core as the build system.
2. **The project config SHALL** be defined completely in `platformio.ini`.

#### REQ-ENV-03: Simulation
**User Story:** As a developer, I want to verify logic without hardware, so that I can iterate quickly.
**Acceptance Criteria:**
1. **The system SHALL** be verifiable using the Wokwi simulator.
2. **The simulation config SHALL** be maintained in `diagram.json` to match the hardware pinout.
