# TrekLink Design Document (Meshtastic Fork)

> **Project Code:** EXE101-G1-TREKLINK  
> **Version:** 2.0 (Meshtastic Architecture)  
> **Date:** February 4, 2026  
> **Status:** ENGINEERING APPROVED

---

## 1. Overview

TrekLink v2.0 is a **Meshtastic firmware v2.6.x fork** optimized for off-grid emergency communications. This design document details the technical architecture, hardware integration, and software components for migrating from proprietary E32 UART implementation to Ra-02 SPI with Meshtastic base firmware.

**Design Goals:**
- ✅ **Rapid Development:** Leverage Meshtastic's mature codebase (~16 hour Phase 1)
- ✅ **Universal Compatibility:** Interoperate with 100,000+ Meshtastic devices
- ✅ **Extensibility:** Add custom TrekLink features (fall detection, Silent Mode)
- ✅ **Risk Mitigation:** Device remains functional even if Phase 2 incomplete

**Key Architectural Changes from v1.0:**

| Aspect | v1.0 (Proprietary) | v2.0 (Meshtastic Fork) |
|--------|-------------------|------------------------|
| **Radio Module** | E32-433T20D (UART) | Ra-02 SX1278 (SPI) |
| **Firmware Base** | Custom ESP-IDF | Meshtastic v2.6.x |
| **Mesh Protocol** | Custom Managed Flooding | Meshtastic Routing |
| **Radio Driver** | Custom UART Commands | RadioLib SPI |
| **Encryption** | AES-128-GCM only | Meshtastic PKC + optional AES |
| **GPIO Allocation** | E32-specific (16,17,18,19,27) | SPI-specific (5,18,19,27,26,14) |
| **PRFH** | Implemented | **Deferred** (time sync complexity) |

---

## 2. System Architecture

### 2.1 High-Level Architecture

```mermaid
graph TB
    subgraph User_Layer["User Layer"]
        USER[User Input: Buttons]
        DISPLAY[OLED Display Output]
    end

    subgraph Application_Layer["Application Layer (Phase 1 + 2)"]
        MESHTASTIC_UI[Meshtastic BaseUI<br/>Screen.cpp]
        TREKLINK_BUTTONS[TrekLink Button Module<br/>Phase 2]
        TREKLINK_FALL[Fall Detection Module<br/>Phase 2]
    end

    subgraph Meshtastic_Core["Meshtastic Core Services"]
        ROUTER[Router.cpp<br/>Mesh Packet Routing]
        RADIO_IF[RadioInterface.cpp<br/>RadioLib Abstraction]
        CHANNELS[Channels.cpp<br/>Encryption Keys]
        NODEDB[NodeDB.cpp<br/>Mesh Directory]
        GPS_SVC[GPSStatus.cpp<br/>GPS Management]
        PWR_SVC[PowerStatus.cpp<br/>Battery Monitoring]
    end

    subgraph Radio_Layer["Radio Abstraction (RadioLib)"]
        RADIOLIB[RadioLib SX1278 Driver]
        CAD[CAD: Channel Activity Detection]
        CSMA[CSMA: Collision Avoidance]
    end

    subgraph Hardware_Layer["Hardware Layer (TrekLink Variant)"]
        RA02[Ra-02 SX1278<br/>433MHz SPI]
        GPS_HW[Neo-6M GPS<br/>UART1]
        MPU[MPU6050 IMU<br/>I2C]
        OLED_HW[SSD1306 OLED<br/>I2C]
        BUTTONS_HW[4x Buttons<br/>GPIO]
        POWER_GATES[Power MOSFETs<br/>GPIO 13, 23]
    end

    USER --> MESHTASTIC_UI
    USER --> TREKLINK_BUTTONS
    MESHTASTIC_UI --> DISPLAY
    TREKLINK_BUTTONS --> ROUTER
    TREKLINK_FALL --> ROUTER
    ROUTER --> RADIO_IF
    RADIO_IF --> CHANNELS
    RADIO_IF --> RADIOLIB
    GPS_SVC --> GPS_HW
    PWR_SVC --> POWER_GATES
    RADIOLIB --> CAD
    RADIOLIB --> CSMA
    RADIOLIB --> RA02
    MESHTASTIC_UI --> OLED_HW
    TREKLINK_BUTTONS --> BUTTONS_HW
    TREKLINK_FALL --> MPU
```

### 2.2 Meshtastic Firmware Stack

```
┌─────────────────────────────────────────────────────────────┐
│              Phase 2: TrekLink Custom Modules               │
│  - TrekLinkButtonModule (multi-button, Silent Mode)        │
│  - FallDetectionModule (MPU6050 monitoring, auto-SOS)      │
│  - TrekLinkProtocol (hybrid AES-128-GCM encryption)        │
├─────────────────────────────────────────────────────────────┤
│               Phase 1: Meshtastic Base Firmware             │
│  - BaseUI (Screen.cpp, graphics.cpp)                        │
│  - Router (packet routing, hop management)                  │
│  - RadioInterface (LoRa abstraction)                        │
│  - GPSStatus (TinyGPSPlus integration)                      │
│  - PowerStatus (battery ADC, power management)              │
│  - Channels (PKC encryption, key management)                │
│  - NodeDB (mesh node database, seen packets)                │
├─────────────────────────────────────────────────────────────┤
│                   RadioLib (SPI Driver)                     │
│  - SX1278 class (Ra-02 hardware interface)                  │
│  - CAD (Channel Activity Detection for low-power RX)        │
│  - CSMA (Carrier Sense Multiple Access)                     │
│  - Packet queue management                                  │
├─────────────────────────────────────────────────────────────┤
│            TrekLink Hardware Variant (variant.h)            │
│  - GPIO pin definitions (LORA_*, I2C_*, GPS_*, etc.)        │
│  - I2C/SPI bus configuration                                │
│  - Button/LED/Power gate mappings                           │
│  - ADC channels for battery sensing                         │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 ESP32 Dual-Core Task Allocation

Meshtastic firmware uses FreeRTOS with dual-core task distribution:

| Core | Tasks | Priority | Purpose |
|------|-------|----------|---------|
| **Core 0** | RadioTask | 2 (Highest) | LoRa TX/RX, CAD listening, packet processing |
| | MeshRoutingTask | 1 | Packet routing, rebroadcast logic, seen buffer |
| **Core 1** | UITask | 0 (Lowest) | Screen rendering, button polling |
| | GPSTask | 0 | NMEA parsing, position updates |
| | [Phase 2] FallDetectionTask | 0 | MPU6050 monitoring (if implemented) |

**Core 0 Real-Time Constraints:**
- RadioTask must respond to DIO0 interrupts within 10ms
- CAD polling occurs every 100-500ms (configurable)
- No blocking operations allowed (delay() prohibited)

**Inter-Core Communication:**
- FreeRTOS Task Notifications (lightweight event signaling)
- Thread-safe queues for packet relay (Core 0 → Core 1 for UI updates)

---

## 3. Hardware Design

### 3.1 Hardware Block Diagram (Updated for Ra-02)

```mermaid
graph TB
    subgraph Power["Power Subsystem"]
        BATT[2x 21700 Li-Ion<br/>10000mAh Parallel<br/>3.7V Nominal]
        TP5100[TP5100 Charger<br/>USB-C Input<br/>2S Li-Ion]
        MINI360[Mini360 Buck<br/>3.3V + 5V Rails]
        BATT --> TP5100
        TP5100 -- CHRG Status --> GPIO35[GPIO 35]
        BATT --> MINI360
    end

    subgraph Core["Core Processing"]
        ESP32[ESP32-WROOM-32<br/>Dual-Core 240MHz<br/>520KB RAM]
        MINI360 -- 3.3V --> ESP32
    end

    subgraph Radio["Radio Subsystem (SPI)"]
        RA02[Ra-02 SX1278<br/>433MHz 100mW<br/>SPI Interface]
        ANT433[433MHz Antenna<br/>17.5cm Whip SMA]
        ESP32 -- "SCK (5)" --> RA02
        ESP32 -- "MISO (19)" --> RA02
        ESP32 -- "MOSI (27)" --> RA02
        ESP32 -- "CS (18)" --> RA02
        ESP32 -- "DIO0 (26)" --> RA02
        ESP32 -- "RESET (14)" --> RA02
        RA02 --> ANT433
    end

    subgraph Navigation["Navigation Subsystem"]
        GPS[Neo-6M GPS<br/>UART 9600 baud<br/>Internal Backup Batt]
        ANT_GPS[GPS Ceramic Antenna<br/>Internal]
        MOSFET_GPS[P-MOSFET IRF9530N<br/>GPS Power Gate]
        NPN_GPS[S8050-D NPN<br/>Gate Driver]
        ESP32 -- "GPIO 13" --> NPN_GPS
        NPN_GPS --> MOSFET_GPS
        MINI360 -- 3.3V --> MOSFET_GPS
        MOSFET_GPS --> GPS
        ESP32 -- "TX1 (16)" --> GPS
        ESP32 -- "RX1 (17)" --> GPS
        GPS --> ANT_GPS
    end

    subgraph Sensors["Sensor Subsystem"]
        MPU[MPU6050 IMU<br/>I2C 0x68<br/>Fall Detection]
        ESP32 -- "I2C SDA (21)" --> MPU
        ESP32 -- "I2C SCL (22)" --> MPU
        ESP32 -- "INT (34)" --> MPU
    end

    subgraph Display["Display Subsystem"]
        OLED[SSD1306 OLED<br/>0.96" 128x64<br/>I2C 0x3C]
        NPN_OLED[S8050-D NPN<br/>GND Switch]
        ESP32 -- "I2C SDA (21)" --> OLED
        ESP32 -- "I2C SCL (22)" --> OLED
        ESP32 -- "GPIO 23" --> NPN_OLED
        NPN_OLED -- GND Control --> OLED
    end

    subgraph UI["User Interface"]
        BTN_MENU[MENU Button<br/>GPIO 25]
        BTN_SOS[SOS Button<br/>GPIO 34]
        BTN_UP[UP Button<br/>GPIO 32]
        BTN_DOWN[DOWN Button<br/>GPIO 35]
        BUZZ[Passive Buzzer<br/>GPIO 33 PWM]
        VIB[Vibrator Motor<br/>GPIO 4 via NPN]
        LED[Status LED<br/>GPIO 2 Built-in]
        ESP32 --> BTN_MENU
        ESP32 --> BTN_SOS
        ESP32 --> BTN_UP
        ESP32 --> BTN_DOWN
        ESP32 --> BUZZ
        ESP32 --> VIB
        ESP32 --> LED
    end

    subgraph Battery_Sense["Battery Monitoring"]
        VOLTAGE_DIV[Voltage Divider<br/>10kΩ + 10kΩ]
        BATT --> VOLTAGE_DIV
        VOLTAGE_DIV -- ADC Input --> GPIO36[GPIO 36<br/>ADC1_CH0]
    end
```

### 3.2 GPIO Pinout Table (TrekLink Final Configuration)

#### LoRa Ra-02 (SPI Bus - VSPI)

| Signal | GPIO | Direction | Pull | Notes |
|--------|------|-----------|------|-------|
| **LORA_SCK** | 5 | Output | - | SPI Clock (Meshtastic DIY default) |
| **LORA_MISO** | 19 | Input | - | Master In Slave Out |
| **LORA_MOSI** | 27 | Output | - | Master Out Slave In |
| **LORA_CS** | 18 | Output | Pull-up | Chip Select (active LOW) |
| **LORA_DIO0** | 26 | Input | - | Interrupt: RX/TX done |
| **LORA_RESET** | 14 | Output | Pull-up | Module reset (active LOW) |

**SPI Configuration:**
- Bus: VSPI (hardware SPI)
- Clock: 4 MHz (SX1278 max: 10 MHz)
- Mode: SPI_MODE0 (CPOL=0, CPHA=0)

#### I2C Bus (Shared OLED + MPU6050)

| Signal | GPIO | Devices | Pull | Notes |
|--------|------|---------|------|-------|
| **I2C_SDA** | 21 | OLED (0x3C) + MPU (0x68) | 4.7kΩ external | Bidirectional data |
| **I2C_SCL** | 22 | OLED + MPU | 4.7kΩ external | Clock output |

**I2C Configuration:**
- Speed: 100 kHz (standard mode)
- Pull-ups: 4.7kΩ to 3.3V (external, required for I2C spec)

#### GPS Neo-6M (UART1)

| Signal | GPIO | Direction | ESP32 Function | Notes |
|--------|------|-----------|----------------|-------|
| **GPS_RX** | 16 | Output | TX1 | ESP32 transmits to GPS |
| **GPS_TX** | 17 | Input | RX1 | GPS transmits to ESP32 |

**UART Configuration:**
- Baud: 9600 (NMEA default)
- Format: 8N1 (8 data bits, no parity, 1 stop bit)
- Protocol: NMEA 0183 ($GPGGA, $GPRMC sentences)

#### User Interface Buttons

| Button | GPIO | Type | Pull | Function |
|--------|------|------|------|----------|
| **BTN_MENU** | 25 | Input | Internal pull-down | Menu navigation, Silent Mode (hold 1s) |
| **BTN_UP** | 32 | Input | Internal pull-down | Scroll up, increment |
| **BTN_DOWN** | 35 | Input Only | External pull-up | Scroll down, decrement |
| **BTN_SOS** | 34 | Input Only | External pull-up | Ping (1 click), SOS (3s hold) |

**Input-Only Constraint:**
- GPIO 34, 35 cannot output (no internal pull-ups available)
- External 10kΩ pull-up resistors required

#### Notifications & Indicators

| Output | GPIO | Type | Notes |
|--------|------|------|-------|
| **PIN_BUZZER** | 33 | PWM | Passive buzzer, 2.7kHz resonant freq |
| **PIN_VIBRATOR** | 4 | Digital | Coin vibrator via NPN transistor |
| **LED_PIN** | 2 | Digital | Built-in blue LED (also strapping pin, LOW at boot) |

#### Power Management

| Control | GPIO | Type | Function |
|---------|------|------|----------|
| **PIN_GPS_PWR_EN** | 13 | Output | P-MOSFET gate (via S8050-D NPN driver) |
| **PIN_OLED_GND_EN** | 23 | Output | OLED GND switch (Silent Mode) |
| **BATTERY_PIN** | 36 | ADC Input Only | Voltage divider (2x 10kΩ, measures BAT/2) |

---

## 4. Software Architecture (Meshtastic Integration)

### 4.1 Meshtastic Core Components

#### 4.1.1 Router (src/mesh/Router.cpp)

**Responsibility:** Packet routing, hop management, seen buffer deduplication.

**Key Functions:**
```cpp
class Router {
    // Receive packet from radio, process routing logic
    void handleFromRadio(MeshPacket *packet);
    
    // Send packet to radio with hop count initialization
    void sendPacket(MeshPacket *packet);
    
    // Check if packet already seen (prevent broadcast storm)
    bool wasSeenRecently(PacketId id);
    
    // Rebroadcast logic based on hop count and channel
    void rebroadcastPacket(MeshPacket *packet);
};
```

**TrekLink Modifications (Phase 2):**
- Inject custom TrekLink packet types (PortNum_PRIVATE_APP)
- Override rebroadcast logic for TrekLink Private mode filtering

#### 4.1.2 RadioInterface (src/mesh/RadioInterface.cpp)

**Responsibility:** Abstract radio hardware using RadioLib.

**Key Functions:**
```cpp
class RadioInterface {
    // Initialize SX1278 with region-specific config
    bool init();
    
    // Transmit packet with CSMA collision avoidance
    bool sendPacket(MeshPacket *packet);
    
    // Check for incoming packets using CAD
    void loop(); // Called continuously from RadioTask
    
    // Handle DIO0 interrupt (RX/TX done)
    void handleInterrupt();
};
```

**TrekLink Configuration:**
```cpp
// In variants/treklink_esp32/variant.h
#define LORA_REGION Meshtastic_Region_EU_433
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_DIO0 26
#define LORA_RESET 14
```

#### 4.1.3 Channels (src/mesh/Channels.cpp)

**Responsibility:** Encryption key management, channel switching.

**Key Functions:**
```cpp
class Channels {
    // Derive AES256 key from PSK
    void initDefaultChannel(const uint8_t *psk, size_t pskLen);
    
    // Encrypt payload with Meshtastic PKC
    bool encryptPacket(MeshPacket *packet);
    
    // Decrypt and authenticate received packet
    bool decryptPacket(MeshPacket *packet);
};
```

**TrekLink Enhancement (Phase 2):**
- Add secondary encryption layer (AES-128-GCM) using Channel ID as seed
- Mode switching: Meshtastic PKC only (public) vs. PKC + AES-GCM (private)

#### 4.1.4 GPSStatus (src/gps/GPSStatus.cpp)

**Responsibility:** GPS management, position tracking.

**Key Functions:**
```cpp
class GPSStatus {
    // Parse NMEA sentences from Neo-6M
    void loop();
    
    // Get last valid position
    Position getPosition();
    
    // Check if GPS has fix
    bool hasValidFix();
};
```

**TrekLink Power Gating (Phase 2):**
```cpp
void enableGPS() {
    digitalWrite(PIN_GPS_PWR_EN, HIGH); // Turn on P-MOSFET
    delay(100); // Wait for GPS boot
}

void disableGPS() {
    digitalWrite(PIN_GPS_PWR_EN, LOW); // Cut GPS power
}
```

---

### 4.2 TrekLink Custom Modules (Phase 2 Optional)

#### 4.2.1 TrekLinkButtonModule

**File:** `src/modules/TrekLinkButtonModule.cpp`

**Responsibility:** Multi-button support, Silent Mode, custom button actions.

```cpp
class TrekLinkButtonModule : public SinglePortModule {
private:
    enum ButtonState {
        IDLE,
        PRESS_DETECTED,
        HOLD_DETECTED,
        DOUBLE_CLICK_WAITING
    };
    
    ButtonState menuState, sosState, upState, downState;
    unsigned long menuPressTime, sosPressTime;
    
public:
    void setup() override {
        pinMode(BTN_MENU, INPUT_PULLDOWN);
        pinMode(BTN_SOS, INPUT);
        pinMode(BTN_UP, INPUT_PULLDOWN);
        pinMode(BTN_DOWN, INPUT);
        
        // Attach interrupt for SOS (critical button)
        attachInterrupt(digitalPinToInterrupt(BTN_SOS), onSOSPressed, FALLING);
    }
    
    int32_t runOnce() override {
        // Poll buttons every 50ms
        handleMenuButton();
        handleSOSButton();
        handleUpDownButtons();
        return 50; // Run again in 50ms
    }
    
private:
    void handleMenuButton() {
        if (digitalRead(BTN_MENU) == HIGH) {
            if (menuState == IDLE) {
                menuPressTime = millis();
                menuState = PRESS_DETECTED;
            } else if (menuState == PRESS_DETECTED && (millis() - menuPressTime > 1000)) {
                toggleSilentMode();
                menuState = HOLD_DETECTED;
            }
        } else {
            if (menuState == PRESS_DETECTED && (millis() - menuPressTime < 1000)) {
                // Short press: Navigate menu
                navigateMenu();
            }
            menuState = IDLE;
        }
    }
    
    void handleSOSButton() {
        if (digitalRead(BTN_SOS) == HIGH) {
            if (sosState == IDLE) {
                sosPressTime = millis();
                sosState = PRESS_DETECTED;
            } else if (sosState == PRESS_DETECTED && (millis() - sosPressTime > 3000)) {
                triggerSOS();
                sosState = HOLD_DETECTED;
            }
        } else {
            if (sosState == PRESS_DETECTED && (millis() - sosPressTime < 1000)) {
                // Single click: Ping location
                broadcastPosition();
            }
            sosState = IDLE;
        }
    }
    
    void toggleSilentMode() {
        static bool silentMode = false;
        silentMode = !silentMode;
        
        // Hardware power gate OLED
        digitalWrite(PIN_OLED_GND_EN, silentMode ? LOW : HIGH);
        
        // Vibrate confirmation
        digitalWrite(PIN_VIBRATOR, HIGH);
        delay(200);
        digitalWrite(PIN_VIBRATOR, LOW);
    }
    
    void triggerSOS() {
        // Create high-priority SOS packet
        MeshPacket packet;
        packet.channel = 0; // Primary channel (broadcast)
        packet.priority = MeshPacket_Priority_CRITICAL;
        packet.want_ack = false; // No ACK needed for broadcast
        
        // Populate position data
        Position pos = service.gps->getPosition();
        packet.decoded.position = pos;
        
        // Send via Meshtastic router
        service.sendPacket(&packet);
        
        // Activate local SOS indicators
        activateSOSAlarms();
    }
    
    void activateSOSAlarms() {
        // Buzzer: Continuous alarm
        ledcAttachPin(PIN_BUZZER, 0);
        ledcSetup(0, 2700, 8); // 2.7kHz, 8-bit resolution
        ledcWrite(0, 128); // 50% duty cycle
        
        // LED: Strobe pattern
        // Vibrator: Pulse pattern
        // (Handled by separate task)
    }
};
```

#### 4.2.2 FallDetectionModule

**File:** `src/modules/FallDetectionModule.cpp`

**Responsibility:** MPU6050 monitoring, fall signature detection, auto-SOS trigger.

```cpp
class FallDetectionModule : public ProtobufModule<MeshPacket> {
private:
    enum FallState {
        MONITORING,
        FREEFALL_DETECTED,
        IMPACT_DETECTED,
        INACTIVITY_DETECTED,
        PRE_ALARM,
        SOS_TRIGGERED
    };
    
    FallState state;
    Adafruit_MPU6050 mpu;
    unsigned long freefallStartTime, impactTime, inactivityStartTime, prealarmStartTime;
    
    const float FREEFALL_THRESHOLD = 0.3; // g (< 0.3g indicates freefall)
    const float IMPACT_THRESHOLD = 3.0;   // g (> 3g indicates impact)
    const int FREEFALL_MIN_DURATION = 500; // ms
    const int INACTIVITY_DURATION = 10000; // 10 seconds
    const int PREALARM_TIMEOUT = 30000;    // 30 seconds
    
public:
    void setup() override {
        Wire.begin(I2C_SDA, I2C_SCL);
        
        if (!mpu.begin(0x68)) {
            LOG_ERROR("MPU6050 not found!");
            return;
        }
        
        // Configure MPU6050
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        
        // Enable motion interrupt on INT pin (GPIO 34)
        pinMode(34, INPUT);
        
        state = MONITORING;
    }
    
    int32_t runOnce() override {
        sensors_event_t accel, gyro, temp;
        mpu.getEvent(&accel, &gyro, &temp);
        
        float totalAccel = sqrt(sq(accel.acceleration.x) + 
                                sq(accel.acceleration.y) + 
                                sq(accel.acceleration.z));
        
        switch (state) {
            case MONITORING:
                // Detect freefall (total acceleration < 0.3g)
                if (totalAccel < FREEFALL_THRESHOLD) {
                    freefallStartTime = millis();
                    state = FREEFALL_DETECTED;
                }
                break;
                
            case FREEFALL_DETECTED:
                if (totalAccel > FREEFALL_THRESHOLD) {
                    // Check if freefall lasted long enough
                    if (millis() - freefallStartTime > FREEFALL_MIN_DURATION) {
                        // Freefall ended, now detect impact
                        state = IMPACT_DETECTED;
                        impactTime = millis();
                    } else {
                        // False alarm, return to monitoring
                        state = MONITORING;
                    }
                }
                break;
                
            case IMPACT_DETECTED:
                // Look for high-G impact within 2 seconds of freefall
                if (totalAccel > IMPACT_THRESHOLD) {
                    inactivityStartTime = millis();
                    state = INACTIVITY_DETECTED;
                } else if (millis() - impactTime > 2000) {
                    // No impact detected, false alarm
                    state = MONITORING;
                }
                break;
                
            case INACTIVITY_DETECTED:
                // Check for movement (total accel deviates from 1g)
                if (abs(totalAccel - 1.0) > 0.2) {
                    // User is moving, cancel fall detection
                    state = MONITORING;
                } else if (millis() - inactivityStartTime > INACTIVITY_DURATION) {
                    // Inactivity threshold reached, enter pre-alarm
                    state = PRE_ALARM;
                    prealarmStartTime = millis();
                    activatePreAlarm();
                }
                break;
                
            case PRE_ALARM:
                // Wait for user to cancel (SOS double-click handled by ButtonModule)
                if (millis() - prealarmStartTime > PREALARM_TIMEOUT) {
                    // Timeout reached, trigger auto-SOS
                    state = SOS_TRIGGERED;
                    triggerAutoSOS();
                }
                // Check if user cancelled (external flag set by ButtonModule)
                if (fallCancelledByUser) {
                    state = MONITORING;
                    deactivatePreAlarm();
                }
                break;
                
            case SOS_TRIGGERED:
                // SOS active indefinitely until user cancels
                break;
        }
        
        return 100; // Check every 100ms
    }
    
private:
    void activatePreAlarm() {
        // Display countdown on OLED
        screen->setStatusMessage("FALL DETECTED! 30s to cancel");
        
        // Rapid vibration pattern
        // Beeping alarm
        // (Implementation similar to TrekLinkButtonModule)
    }
    
    void triggerAutoSOS() {
        // Same as manual SOS trigger in ButtonModule
        MeshPacket packet;
        packet.channel = 0;
        packet.priority = MeshPacket_Priority_CRITICAL;
        packet.decoded.position = service.gps->getPosition();
        service.sendPacket(&packet);
        
        // Activate full SOS indicators
    }
};
```

---

## 5. Data Models & Interfaces

### 5.1 Meshtastic Packet Structure (Protobuf)

**MeshPacket (Standard Meshtastic):**
```protobuf
message MeshPacket {
    uint32 id = 1;           // Unique packet ID (prevents duplicates)
    fixed32 from = 2;        // Sending node ID
    fixed32 to = 3;          // Target node ID (0xFFFFFFFF = broadcast)
    uint32 channel = 4;      // Channel index (0 = primary)
    Data decoded = 5;        // Decrypted payload
    bytes encrypted = 6;     // Encrypted payload (if not decoded)
    uint32 rx_time = 7;      // Receive timestamp
    int32 rx_snr = 8;        // Signal-to-Noise Ratio
    int32 rx_rssi = 9;       // Received Signal Strength
    uint32 hop_limit = 10;   // Remaining hops
    bool want_ack = 11;      // Request acknowledgment
    Priority priority = 12;  // UNSET, MIN, BACKGROUND, DEFAULT, RELIABLE, CRITICAL (SOS)
}

message Data {
    PortNum portnum = 1;     // Type: TEXT_MESSAGE_APP, POSITION_APP, NODEINFO_APP, PRIVATE_APP
    bytes payload = 2;       // Actual message data
}
```

**TrekLink Private Packet (Phase 2):**
```cpp
// Encapsulated within PortNum_PRIVATE_APP
struct TrekLinkPrivatePayload {
    uint8_t version = 0x01;
    uint8_t channelId;              // Custom Channel ID for AES-GCM key derivation
    uint8_t encryptedData[64];      // AES-128-GCM encrypted
    uint8_t authTag[16];            // GCM authentication tag
    uint8_t nonce[12];              // Nonce for GCM (concat timestamp + random)
};
```

### 5.2 GPS Data Model

**TinyGPSPlus Integration (Meshtastic Default):**
```cpp
struct Position {
    double latitude;   // Decimal degrees
    double longitude;  // Decimal degrees
    int32_t altitude;  // Meters above MSL
    uint32_t time;     // Unix timestamp
    uint8_t sats;      // Satellites in view
    uint32_t precision_bits; // PDOP precision indicator
};
```

**Power Gating Strategy:**
```cpp
// GPS polling with power gating
void GPSTask() {
    while (true) {
        enableGPS();           // Turn on via GPIO 13
        delay(5000);           // Wait for GPS acquisition (hot start: <2s)
        Position pos = gps->getPosition();
        disableGPS();          // Cut power
        
        if (pos.isValid()) {
            // Share position with mesh
            broadcastPosition(pos);
        }
        
        vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_INTERVAL)); // Default: 60s
    }
}
```

---

## 6. Error Handling & Resilience

### 6.1 Radio Failures

**CAD Timeout:**
```cpp
// If no packet heard for 5 minutes, assume radio failure
if (millis() - lastPacketTime > 300000) {
    LOG_WARN("Radio silence detected, reinitializing SX1278...");
    radio.reset();
    radio.begin(433.0, 125.0, 9, 7, 20);
}
```

**TX Failure Recovery:**
```cpp
int retryCount = 0;
while (radio.transmit(packet) != RADIOLIB_ERR_NONE && retryCount < 3) {
    LOG_ERROR("TX failed, retrying... (%d/3)", retryCount);
    delay(100);
    retryCount++;
}
```

### 6.2 GPS Failures

**Fallback Strategy (Phase 2):**
1. **No GPS fix:** Display last known position + "GPS searching..."
2. **Extended no-fix (>10 min):** Attempt RSSI triangulation using mesh nodes
3. **No mesh nodes:** Use dead reckoning (IMU-based position estimation)

### 6.3 Power Failures

**Low Battery Protocol:**
```cpp
if (batteryVoltage < 3.2) { // Per-cell voltage (6.4V total for 2S)
    // Disable non-critical peripherals
    disableGPS();
    digitalWrite(PIN_OLED_GND_EN, LOW); // Turn off OLED
    
    // Enter deep sleep, wake every 60s for emergency RX
    esp_sleep_enable_timer_wakeup(60 * 1000000); // 60 seconds in μs
    esp_deep_sleep_start();
}
```

---

## 7. Testing Strategy

### 7.1 Unit Tests (Isolated Components)

**Test 1: Ra-02 SPI Communication**
```cpp
void test_ra02_init() {
    RadioLib SX1278 radio = new Module(18, 26, 14, 19);
    int state = radio.begin(433.0, 125.0, 9, 7, 20);
    assert(state == RADIOLIB_ERR_NONE);
    printf("✓ Ra-02 initialized successfully\n");
}
```

**Test 2: GPS NMEA Parsing**
```cpp
void test_gps_parsing() {
    const char *nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    TinyGPSPlus gps;
    for (const char *c = nmea; *c; c++) {
        gps.encode(*c);
    }
    assert(gps.location.isValid());
    assert(abs(gps.location.lat() - 48.1173) < 0.01);
    printf("✓ GPS parsing correct\n");
}
```

**Test 3: MPU6050 I2C Detection**
```cpp
void test_mpu6050() {
    Wire.begin(21, 22);
    Wire.beginTransmission(0x68);
    int error = Wire.endTransmission();
    assert(error == 0);
    printf("✓ MPU6050 detected on I2C\n");
}
```

### 7.2 Integration Tests (Multi-Device)

**Test 1: Mesh Packet RX/TX (2 devices)**
- **Setup:** TrekLink + Standard Meshtastic T-Beam (both on EU_433)
- **Procedure:** Send text message from T-Beam → TrekLink
- **Expected:** TrekLink receives packet within 5s, displays on OLED

**Test 2: Position Broadcast (1 device + app)**
- **Setup:** TrekLink with GPS fix + Meshtastic Android app
- **Procedure:** Click SOS button on TrekLink
- **Expected:** Android app shows TrekLink position on map

### 7.3 Phase 2 Tests (Custom Features)

**Test 1: Fall Detection**
- **Setup:** TrekLink with Phase 2 firmware
- **Procedure:** Drop device from 1.5m onto soft surface
- **Expected:** Pre-alarm triggers (30s countdown), auto-SOS if not cancelled

**Test 2: Silent Mode**
- **Setup:** TrekLink with Phase 2 firmware
- **Procedure:** Hold MENU button for 1 second
- **Expected:** OLED turns off (GPIO 23 → LOW), vibrator confirms

**Test 3: Hybrid Encryption**
- **Setup:** 2x TrekLink (Phase 2) + 1x Standard Meshtastic
- **Procedure:** Send TrekLink Private message
- **Expected:** Only TrekLink devices decrypt, standard device cannot

---

## 8. Design Decisions & Trade-offs

### 8.1 Why Meshtastic Fork?

**Advantages:**
✅ Mature RadioLib SPI driver (proven SX1278 support)  
✅ 100,000+ compatible devices (instant ecosystem)  
✅ Active development (OTA updates, bug fixes)  
✅ Proven power management (light sleep, CAD optimization)  
✅ Strong encryption (PKC with per-channel keys)  

**Disadvantages:**
❌ Loss of PRFH anti-jamming (deferred)  
❌ Custom UI requires forking BaseUI  
❌ Learning curve for Meshtastic codebase  
❌ Dependency on upstream changes  

**Verdict:** Advantages outweigh disadvantages for MVP timeline.

### 8.2 Why Remove PRFH?

**Technical Challenge:**
- PRFH requires **nanosecond-accurate time synchronization** across all mesh nodes
- GPS time sync works, but nodes boot at different times (desync up to hours)
- Implementing "join protocol" for new nodes to catch hopping sequence adds 10-15 hours

**Alternative:**
- Use Meshtastic's static frequency per region (EU_433 = 433.175 MHz default)
- Rely on AES-256 PKC encryption for security (industry standard)
- Phase 2 may revisit PRFH if time allows

### 8.3 Why SPI Instead of UART?

**Ra-02 SX1278 Advantages:**
✅ Direct register access (faster configuration changes)  
✅ RadioLib compatibility (mature, well-tested library)  
✅ Lower latency (SPI @ 4 MHz vs UART @ 115200 baud)  
✅ No AT command parsing overhead  

**E32 UART Disadvantages:**
❌ Proprietary AT commands (vendor lock-in)  
❌ Channel switching delay (80ms M0/M1 settling)  
❌ Internal MCU black box (cannot debug RF issues)  

---

## 9. Future Enhancements

### Post-MVP Features (Beyond Phase 2)

🔮 **PRFH Anti-Jamming:** Implement if GPS time sync + join protocol solved  
🔮 **Custom Android App:** TrekLink-specific UI for preset messages, dot matrix map  
🔮 **Wi-Fi AP Configuration:** Web interface for device settings (no phone app needed)  
🔮 **Dead Reckoning:** IMU-based position estimation when GPS unavailable  
🔮 **RSSI Triangulation:** Mesh-based positioning using neighbor RSSI measurements  
🔮 **Offline Map Tiles:** Store OpenStreetMap tiles in SPIFFS for true offline navigation  
🔮 **Voice Codec:** Low-bitrate voice transmission (e.g., Codec2 @ 1.2 kbps)  

---

## 10. References & Resources

**Meshtastic Documentation:**
- Official Docs: https://meshtastic.org/docs/introduction
- Firmware GitHub: https://github.com/meshtastic/firmware
- Hardware Variants: https://meshtastic.org/docs/hardware

**RadioLib (SX1278 Driver):**
- GitHub: https://github.com/jgromes/RadioLib
- SX1278 Datasheet: https://www.semtech.com/products/wireless-rf/lora-core/sx1278

**TrekLink Project Resources:**
- Implementation Plan: `implementation_plan.md`
- GPIO Pinout: `gpio_pinout_comparison.md`
- Requirements: `.kiro/specs/treklink/requirements.md`
- System Spec: `specifications.md`

---

**Document Control:**
- **Author:** AI Development Team
- **Reviewed By:** [Pending User Approval]
- **Next Review Date:** Upon Phase 1 completion
- **Change Log:**
  - v2.0 (2026-02-04): Complete redesign for Meshtastic fork with Ra-02 SPI
  - v1.2 (2026-01-28): Original proprietary E32 UART design
