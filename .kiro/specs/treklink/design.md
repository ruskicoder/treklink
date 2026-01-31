# TrekLink MVP Design Document

## 1. Overview
TrekLink is a decentralized, off-grid communication device powered by an **ESP32** (host) and **Ebyte E32** (LoRa). It creates a mesh network for short-text messaging, GPS tracking, and SOS alerts. The design prioritizes redundancy, power efficiency (Power Gated Architecture), and security (AES-128 + PRFH).

## 2. System Architecture

The system follows a **Layered Architecture**:
1.  **Hardware Layer**: Physical modules (ESP32, LoRa, GPS, IMU, OLED) & Power/Battery Management.
2.  **HAL (Hardware Abstraction Layer)**: Drivers for I2C (OLED, MPU6050), UART (GPS, LoRa), and GPIO (Buttons, MOSFETs).
3.  **Service Layer**:
    *   **Mesh Service**: Handles "Blind Flooding" managed routing, packet deduplication, and channel management.
    *   **Sensor Service**: Manages GPS fixes (Hot Start) and Fall Detection (Low Power Wake).
    *   **Security Service**: Handles AES-128-CTR encryption and PRFH seeding.
    *   **Power Service**: State machine for Deep Sleep, Wake-on-Radio, and Power Gating.
    *   **Storage Service**: Ring Buffer logging to Internal Flash (NVS) to minimize wear.
4.  **Application/UI Layer**: Main loop handling User Input (3-Button Nav) and Output (OLED/Buzzer).

### 2.1 Block Diagram
```mermaid
graph TD
    subgraph Power_Subsystem
        BAT[2x 21700 Li-Ion] --> TP5100[Charger]
        TP5100 -- STATUS --> ESP32_GPIO[Charge Detect]
        BAT --> BUCK[Mini360 3.3V]
        BUCK --> ESP32_VCC
        BAT --> RAIL_LORA[LoRa VCC (Always On)]
        BAT --> RAIL_GPS_BCKP[GPS Backup VCC (Always On)]
        BAT --> RAIL_IMU[MPU6050 VCC (Always On - Low Power Mode)]
        BAT --> RAIL_PERIPH[P-MOSFET High-Side Switch]
        RAIL_PERIPH --> GPS_MAIN_VCC
        RAIL_PERIPH --> OLED_VCC
    end

    subgraph MCU_ESP32
        ESP32[ESP32-WROOM-32]
        ESP32 -- UART2 (RX/TX) --> GPS[Neo-6M]
        ESP32 -- UART1 (RX/TX) --> LORA[E32-433T20D]
        ESP32 -- I2C (SDA/SCL) --> OLED[0.96 OLED]
        ESP32 -- I2C (SDA/SCL) --> IMU[MPU6050]
        ESP32 -- GPIO --> BTN[Buttons x4]
        ESP32 -- GPIO --> SW[Slide Switch]
        ESP32 -- GPIO --> MOSFET_GATE[Power Gating Ctrl]
        ESP32 -- RTC_GPIO (27) <-- LORA_AUX[Wake Interrupt]
        ESP32 -- RTC_GPIO (35) <-- IMU_INT[Wake Detection]
    end
```

## 3. Hardware Design

### 3.1 Wiring Map (Pinout Strategy)
*   **LoRa (E32)**:
    *   TX -> GPIO 17
    *   RX -> GPIO 16
    *   **AUX -> GPIO 27** (RTC Pin, Safe from Bootstrapping)
    *   M0 -> GPIO 18
    *   M1 -> GPIO 19
*   **GPS (Neo-6M)**:
    *   TX -> GPIO 14 (ESP32 RX)
    *   **V_BCKP -> V_BAT** (Direct Connection for Hot Start)
*   **Sensors**:
    *   I2C SDA -> GPIO 21
    *   I2C SCL -> GPIO 22
    *   **MPU INT -> GPIO 34** (Input Only, Wake)
*   **Power & Status**:
    *   **TP5100 CHRG -> GPIO 35** (Input Only)
    *   MOSFET Gate -> GPIO 13
    *   Buzzer -> GPIO 12
    *   Vibrate -> GPIO 15
*   **Controls**:
    *   BTN_MENU -> GPIO 25
    *   BTN_SOS -> GPIO 26
    *   BTN_UP -> GPIO 32
    *   BTN_DOWN -> GPIO 33
    *   SW_SLIDE -> GPIO 4

### 3.2 Power/Sleep Logic
*   **GPS Strategy**: Main power cut via MOSFET during sleep. Backup Battery pin always connected to ensure hot starts (<1s fix).
*   **IMU Strategy**: MPU6050 connected to Always-On rail. Configured to "Low Power Accelerometer Mode" (Cycle Mode, ~10-20uA) to allow Wake-on-Motion even when "Off".
*   **Charging**: GPIO 35 reads TP5100 status. Low = Charging.

## 4. Software Design

### 4.1 Requirements Traceability
*   **Logging**: Internal Flash (NVS/SPIFFS) with RAM Ring Buffer. Only writes to flash on Buffer Full or Critical Event (SOS) to save wear.
*   **UI**: **Presets Only**. No custom text input for MVP to ensure simplicity.
*   **Routing**: **Blind Flooding**. Packets are rebroadcast if `hopCount > 0` and `msgId` is new, regardless of target (unless Local-Only mode set).

### 4.2 Key State Machines

#### A. SOS Logic (Infinite Pulse)
1.  **Trigger**: Button Hold (3s) or Fall Detected (Timeout).
2.  **Phase 1 (Immediate)**: Transmit SOS Packet continuously (CSMA disabled/aggressive) for **1 Minute**.
3.  **Phase 2 (Beacon)**: Transmit SOS Packet once every **30 Seconds** (Pulse) indefinitely.
4.  **Cancellation**: User must **HOLD SOS Button for 5 Seconds**. System confirms with Long Vibrate.

#### B. Software Interference Mitigation
*   **Logic**: During LoRa Transmission (TX Active), the system **SHALL** ignore/discard GPS NMEA data.
*   **Reason**: Top-mounted antennas (~7cm separation) may cause LNA saturation during the 100-500ms TX burst. GPS reading resumes immediately after TX completes.

## 5. Development Phases
1.  **Phase 1: HAL & Kernel**: Drivers for new wiring (Safe AUX), Power Gating logic, Sleep/Wake.
2.  **Phase 2: Mesh Core**: Blind Flooding, Packet Structure, Encryption.
3.  **Phase 3: Sensor Fusion**: GPS Hot Start verification, Fall Detection Interrupts.
4.  **Phase 4: UI & Application**: Menu system, Presets, SOS State Machine.

## 6. Conclusion
This design is finalized for the MVP. It mitigates power risks (GPS backup), data loss risks (Flash wear), and security risks (Blind routing).

## 7. Configuration & Environment

### 7.1 Development Environment
*   **Platform**: PlatformIO Core (CLI).
*   **Framework**: Arduino Framework (ESP32).
*   **Language**: C++17.

### 7.2 Simulation Strategy (Wokwi)
To ensure rapid iteration without constant hardware flashing, the following components must be verifiable in Wokwi:
1.  **Mesh Logic**: Simulated by connecting multiple Wokwi instances via logic analyzer or UDP gatway (Advanced) or simply mocking the UART interface to simulate packet reception.
2.  **UI Flows**: OLED display logic and button de-bouncing.
3.  **Sensor Protocol**: I2C mocks for MPU6050 and UART mocks for NMEA GPS streams.

