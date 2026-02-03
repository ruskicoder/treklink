# TrekLink Implementation Tasks (Meshtastic Fork)

> **Project Code:** EXE101-G1-TREKLINK  
> **Version:** 2.0 (Meshtastic Architecture)  
> **Date:** February 4, 2026  
> **Status:** READY FOR IMPLEMENTATION

---

## Implementation Strategy

This document outlines the two-phase implementation strategy for migrating TrekLink from proprietary firmware to a Meshtastic v2.6.x fork with Ra-02 433MHz SPI radio.

**Phase Priority:**
- **Phase 1 (CRITICAL):** Working Meshtastic-compatible device (~16 hours)
- **Phase 2 (OPTIONAL):** Custom TrekLink features (~20 hours)

If Phase 2 is incomplete, the device remains a fully functional **generic Meshtastic node**.

---

## Phase 1: Meshtastic Base Implementation (PRIORITY)

**Goal:** Create a working Meshtastic device with TrekLink hardware configuration.

**Estimated Time:** 16 hours  
**Success Criteria:** Device boots with Meshtastic, Ra-02 transmits/receives, interoperates with other Meshtastic devices.

---

### 1. Meshtastic Repository Setup

- [ ] 1.1 Clone and configure Meshtastic firmware repository
  - Clone official Meshtastic repository: `git clone https://github.com/meshtastic/firmware.git`
  - Checkout stable v2.6.x branch: `git checkout v2.6.x`
  - Explore repository structure (`src/`, `variants/`, `platformio.ini`)
  - _Requirements: REQ-ENV-01, REQ-ENV-02_

- [ ] 1.2 Create TrekLink custom variant directory
  - Create directory: `variants/treklink_esp32/`
  - Copy template from `variants/heltec_v1/variant.h` as starting point
  - Study variant.h structure (pin definitions, I2C/SPI config, region settings)
  - _Requirements: REQ-HW-04_

---

### 2. GPIO Configuration & Variant Definition

- [ ] 2.1 Define Ra-02 SPI pin mappings
  - Edit `variants/treklink_esp32/variant.h`
  - Define LoRa SPI pins:
    ```cpp
    #define LORA_SCK    5
    #define LORA_MISO   19
    #define LORA_MOSI   27
    #define LORA_CS     18
    #define LORA_DIO0   26
    #define LORA_RESET  14
    ```
  - Configure SPI bus settings (VSPI, 4 MHz clock)
  - _Requirements: REQ-HW-04.1_

- [ ] 2.2 Define I2C peripheral pins
  - Add I2C pin definitions for shared OLED + MPU6050 bus:
    ```cpp
    #define I2C_SDA     21
    #define I2C_SCL     22
    ```
  - Configure I2C speed (100 kHz standard mode)
  - Add I2C device addresses: OLED = 0x3C, MPU6050 = 0x68
  - _Requirements: REQ-HW-04.2, REQ-HW-04.3_

- [ ] 2.3 Define GPS UART pins
  - Add GPS Neo-6M UART1 pin mappings:
    ```cpp
    #define GPS_RX_PIN  16  // ESP32 TX1
    #define GPS_TX_PIN  17  // ESP32 RX1
    ```
  - Configure UART baud rate (9600 for NMEA)
  - _Requirements: REQ-HW-04.2_

- [ ] 2.4 Define button and notification pins
  - Add button GPIO definitions:
    ```cpp
    #define BUTTON_PIN  25  // MENU (Meshtastic uses single button)
    // Phase 2 will add BTN_SOS=34, BTN_UP=32, BTN_DOWN=35
    ```
  - Add notification pins:
    ```cpp
    #define PIN_BUZZER    33
    #define PIN_VIBRATOR  4
    #define LED_PIN       2
    ```
  - _Requirements: REQ-HW-04.10_

- [ ] 2.5 Define power management pins
  - Add power gating control pins:
    ```cpp
    #define PIN_GPS_PWR_EN    13  // GPS P-MOSFET gate
    #define PIN_OLED_GND_EN   23  // OLED GND switch (Phase 2)
    #define BATTERY_PIN       36  // ADC1_CH0
    ```
  - Configure ADC attenuation for battery voltage sensing
  - _Requirements: REQ-HW-04.6, REQ-HW-04.7, REQ-HW-04.5_

- [ ] 2.6 Set Meshtastic region configuration
  - Define frequency region:
    ```cpp
    #define LORA_REGION  Meshtastic_Region_EU_433
    ```
  - Verify EU 433MHz band compliance (433.05-434.79 MHz)
  - _Requirements: REQ-COM-01.1, Regulatory Compliance_

---

### 3. PlatformIO Environment Configuration

- [ ] 3.1 Add TrekLink environment to platformio.ini
  - Open `platformio.ini` in firmware root
  - Add custom environment after existing esp32 environments:
    ```ini
    [env:treklink-esp32]
    extends = esp32_base
    board = esp32dev
    build_flags =
        ${esp32_base.build_flags}
        -D PRIVATE_HW
        -I variants/treklink_esp32
        -D ARDUINO_LORA_REGION=Meshtastic_Region_EU_433
    lib_deps =
        ${esp32_base.lib_deps}
        jgromes/RadioLib@^6.0.0
    ```
  - Verify build flag syntax
  - _Requirements: REQ-ENV-02_

- [ ] 3.2 Verify dependency resolution
  - Check that `jgromes/RadioLib@^6.0.0` is included in lib_deps
  - Verify Adafruit libraries for OLED (SSD1306) and IMU (MPU6050) are in esp32_base deps
  - Run dependency check: `pio pkg list -e treklink-esp32`
  - _Requirements: REQ-ENV-01.3_

---

### 4. Compilation & Firmware Build

- [ ] 4.1 Build firmware for TrekLink variant
  - Run PlatformIO build command:
    ```bash
    pio run -e treklink-esp32
    ```
  - Resolve any compilation errors (pin conflicts, missing defines)
  - Verify successful build output (firmware.bin generated)
  - _Requirements: REQ-ENV-02.1_

- [ ] 4.2 Flash firmware to ESP32
  - Connect ESP32 via USB
  - Flash compiled firmware:
    ```bash
    pio run -e treklink-esp32 -t upload
    ```
  - Monitor serial output:
    ```bash
    pio device monitor -b 115200
    ```
  - Expected boot messages: Meshtastic version, region EU_433, radio init
  - _Requirements: REQ-ENV-02.1_

---

### 5. Hardware Integration Testing

- [ ] 5.1 Verify Ra-02 SPI initialization
  - Connect Ra-02 module to ESP32 according to GPIO pinout (Section 2.1)
  - Power on device, monitor serial output
  - Expected log: `[INFO] SX1278 init success | Freq: 433.175 MHz`
  - Test: Send test packet via Meshtastic CLI
  - _Requirements: REQ-HW-01.2, REQ-COM-01.1_

- [ ] 5.2 Test GPS Neo-6M UART communication
  - Connect GPS module to GPIO 16/17 (UART1)
  - Power on GPS (ensure GPIO 13 is HIGH for P-MOSFET)
  - Monitor NMEA sentences in serial log: `$GPGGA`, `$GPRMC`
  - Verify GPS fix acquisition (outdoor test, <60 seconds with internal backup battery)
  - _Requirements: REQ-HW-01.4, REQ-NAV-01_

- [ ] 5.3 Verify OLED display initialization
  - Connect SSD1306 OLED to I2C bus (GPIO 21/22)
  - Power on device
  - Expected: Meshtastic splash screen displays on OLED
  - Verify I2C address detection (0x3C)
  - Test: Navigate Meshtastic menus using BUTTON_PIN (GPIO 25)
  - _Requirements: REQ-HW-02.1, REQ-UI-01_

- [ ] 5.4 Test MPU6050 IMU I2C detection
  - Connect MPU6050 to I2C bus (shared with OLED)
  - Verify I2C address 0x68 detected in serial log
  - Read accelerometer/gyro test values
  - Note: Fall detection not implemented in Phase 1, only verify hardware
  - _Requirements: REQ-HW-01.5_

- [ ] 5.5 Verify button input detection
  - Connect MENU button to GPIO 25
  - Test: Press button, verify Meshtastic UI responds (menu navigation)
  - Monitor debouncing behavior (no spurious inputs)
  - _Requirements: REQ-UI-02_

- [ ] 5.6 Test battery ADC voltage sensing
  - Connect battery voltage divider to GPIO 36 (ADC1_CH0)
  - Measure raw ADC value, convert to voltage (formula: `V_batt = ADC_value * 2 * 3.3 / 4095`)
  - Verify Meshtastic PowerStatus displays battery percentage
  - Expected range: 6.4V (full, 2S @ 3.2V/cell) to 8.4V (charged, 2S @ 4.2V/cell)
  - _Requirements: REQ-PWR-02_

---

### 6. Meshtastic Interoperability Testing

- [ ] 6.1 Two-device mesh packet exchange
  - **Setup:** TrekLink device + standard Meshtastic T-Beam (or similar)
  - **Procedure:**
    1. Set both devices to same channel (default PSK or custom)
    2. Send text message from T-Beam → TrekLink via Meshtastic app
    3. Observe TrekLink OLED display received message
  - **Expected:** Packet received within 5 seconds, RSSI/SNR displayed
  - _Requirements: REQ-COM-01, REQ-COM-02, REQ-COM-03_

- [ ] 6.2 Position broadcast (SOS button placeholder)
  - **Setup:** TrekLink with GPS fix + Meshtastic Android/iOS app
  - **Procedure:**
    1. Click MENU button to access Meshtastic menu
    2. Select "Send Position" or trigger via serial command
    3. Verify position appears on Meshtastic app map
  - **Expected:** GPS coordinates broadcast as POSITION_APP packet
  - _Requirements: REQ-NAV-01, REQ-SAF-01_

- [ ] 6.3 Multi-hop routing test (3+ devices)
  - **Setup:** 3 Meshtastic devices (including TrekLink) arranged in line: A ←→ B ←→ C
  - **Procedure:**
    1. Device A and C out of direct range (B is relay)
    2. Send message from A → C
    3. Observe B rebroadcasts packet (check RSSI logs)
  - **Expected:** Message reaches C via B relay, hop count decrements
  - _Requirements: REQ-COM-01.2_

---

### Phase 1 Completion Checklist

✅ **Phase 1 SUCCESS CRITERIA:**
- [x] Device boots with Meshtastic firmware (serial log shows version)
- [x] Ra-02 433MHz SPI radio transmits and receives packets
- [x] GPS acquires fix outdoors and shares position
- [x] OLED displays Meshtastic UI (dashboard, messages)
- [x] MENU button navigates Meshtastic menus
- [x] Battery voltage displayed correctly
- [x] Device interoperates with other Meshtastic nodes (text messages, position)

**If all criteria met:** Phase 1 complete, device is functional Meshtastic node.  
**Next:** Optionally proceed to Phase 2 for custom TrekLink features.

---

## Phase 2: TrekLink Custom Features (OPTIONAL)

**Goal:** Add TrekLink differentiators (fall detection, multi-button, Silent Mode, hybrid encryption).

**Estimated Time:** 20 hours  
**Success Criteria:** Fall detection auto-SOS, Silent Mode power gating, TrekLink Private encryption working.

**IMPORTANT:** Phase 2 is **OPTIONAL**. If time constraints prevent completion, Phase 1 device remains fully functional.

---

### 7. Multi-Button Support Module

- [ ] 7.1 Create TrekLinkButtonModule class
  - **File:** `src/modules/TrekLinkButtonModule.cpp` + `.h`
  - Extend Meshtastic `SinglePortModule` or `ProtobufModule`
  - Add member variables for button states (MENU, SOS, UP, DOWN)
  - Implement debouncing logic (50ms threshold)
  - _Requirements: REQ-UI-02_

- [ ] 7.2 Implement button event detection
  - Detect click, double-click, hold events (separate logic for each button)
  - Add interrupt handler for SOS button (GPIO 34) using `attachInterrupt()`
  - Use `millis()` for timing (no `delay()` allowed)
  - _Requirements: REQ-UI-02_

- [ ] 7.3 Map button actions to Meshtastic functions
  - **MENU button:**
    - Click: Navigate Meshtastic menu
    - Hold 1s: Toggle Silent Mode (call `toggleSilentMode()`)
  - **SOS button:**
    - Click: Broadcast position (call Meshtastic `sendPosition()`)
    - Hold 3s: Trigger SOS (create CRITICAL priority packet)
    - Double-click: Matrix request (broadcast NodeInfo query)
  - **UP/DOWN buttons:**
    - Navigate menu options, scroll messages
  - _Requirements: REQ-SAF-01, REQ-UI-02_

- [ ] 7.4 Register module with Meshtastic module system
  - Add TrekLinkButtonModule to `src/modules/Modules.cpp` registered modules list:
    ```cpp
    #ifdef TREKLINK_VARIANT
    registerModule(&trekLinkButtonModule);
    #endif
    ```
  - Define `TREKLINK_VARIANT` in variant.h
  - Test: Verify module `setup()` and `runOnce()` are called during boot
  - _Requirements: REQ-UI-02_

---

### 8. Silent Mode Power Gating

- [ ] 8.1 Implement hardware power gate control
  - Add function to TrekLinkButtonModule:
    ```cpp
    void toggleSilentMode() {
        static bool silentMode = false;
        silentMode = !silentMode;
        digitalWrite(PIN_OLED_GND_EN, silentMode ? LOW : HIGH);
        // Vibrate confirmation
        digitalWrite(PIN_VIBRATOR, HIGH);
        delay(200);
        digitalWrite(PIN_VIBRATOR, LOW);
    }
    ```
  - Connect OLED GND to NPN transistor (S8050-D) controlled by GPIO 23
  - _Requirements: REQ-PWR-01.3, REQ-SEC-04_

- [ ] 8.2 Test Silent Mode functionality
  - **Setup:** TrekLink device with Phase 2 firmware
  - **Procedure:**
    1. Hold MENU button for 1 second
    2. Observe OLED turns off (screen goes black)
    3. Device vibrates confirmation
    4. Test: Send message to device, verify vibration-only alert (no screen)
  - **Expected:** OLED power cut via GPIO 23, device still functional
  - _Requirements: REQ-SEC-04_

---

### 9. Fall Detection Module

- [ ] 9.1 Create FallDetectionModule class
  - **File:** `src/modules/FallDetectionModule.cpp` + `.h`
  - Extend Meshtastic `ProtobufModule<MeshPacket>`
  - Add state machine: `MONITORING → FREEFALL → IMPACT → INACTIVITY → PRE_ALARM → SOS_TRIGGERED`
  - Initialize Adafruit_MPU6050 library in `setup()`
  - _Requirements: REQ-SAF-02_

- [ ] 9.2 Implement fall signature detection logic
  - **Freefall Detection:**
    - Monitor total acceleration: `sqrt(ax² + ay² + az²)`
    - If < 0.3g for >500ms → FREEFALL_DETECTED
  - **Impact Detection:**
    - After freefall, look for total accel > 3g → IMPACT_DETECTED
  - **Inactivity Detection:**
    - After impact, monitor for no movement (accel ≈ 1g ± 0.2g) for 10s → INACTIVITY_DETECTED
  - _Requirements: REQ-SAF-02.1_

- [ ] 9.3 Implement Pre-Alarm state
  - Enter PRE_ALARM after inactivity detection
  - Display countdown on OLED: "FALL DETECTED! 30s to cancel"
  - Sound rapid beeping alarm (buzzer)
  - Vibrate continuously
  - Monitor for cancellation (SOS double-click from ButtonModule)
  - If 30 seconds elapsed without cancel → trigger auto-SOS
  - _Requirements: REQ-SAF-02.2, REQ-SAF-02.3, REQ-SAF-02.4_

- [ ] 9.4 Integrate with Meshtastic SOS trigger
  - Create Meshtastic packet with CRITICAL priority:
    ```cpp
    MeshPacket packet;
    packet.channel = 0; // Primary channel
    packet.priority = MeshPacket_Priority_CRITICAL;
    packet.to = NODENUM_BROADCAST; // 0xFFFFFFFF
    packet.decoded.portnum = PortNum_POSITION_APP;
    packet.decoded.position = service.gps->getPosition();
    service.sendPacket(&packet);
    ```
  - Activate local SOS indicators (buzzer, LED strobe)
  - _Requirements: REQ-SAF-02.5_

- [ ] 9.5 Test fall detection end-to-end
  - **Setup:** TrekLink with Phase 2 firmware, outdoor with GPS fix
  - **Procedure:**
    1. Drop device from 1.5m onto soft surface (mattress/cushion)
    2. Observe fall signature detection (freefall → impact → inactivity)
    3. Pre-alarm countdown starts (30s)
    4. **Test A:** Double-click SOS to cancel, verify SOS not sent
    5. **Test B:** Wait 30s without cancel, verify auto-SOS broadcast
  - **Expected:** Nearby Meshtastic devices receive SOS with GPS coordinates
  - _Requirements: REQ-SAF-02_

---

### 10. Hybrid Encryption (TrekLink Private Mode)

- [ ] 10.1 Create TrekLinkProtocol class
  - **File:** `src/mesh/TrekLinkProtocol.cpp` + `.h`
  - Add AES-128-GCM encryption functions (use ESP32 mbedTLS library)
  - Define custom packet payload structure:
    ```cpp
    struct TrekLinkPrivatePayload {
        uint8_t version;
        uint8_t channelId;
        uint8_t encryptedData[64];
        uint8_t authTag[16];
        uint8_t nonce[12];
    };
    ```
  - _Requirements: REQ-SEC-01.2, REQ-SEC-01.4_

- [ ] 10.2 Implement Channel ID-based key derivation
  - Derive AES-128-GCM key from Channel ID:
    ```cpp
    void deriveKey(uint8_t channelId, uint8_t *key) {
        uint8_t seed[32];
        memset(seed, channelId, 32); // Simple seed (production: use HMAC-SHA256)
        memcpy(key, seed, 16); // Use first 16 bytes as AES-128 key
    }
    ```
  - Generate unique nonce per packet (timestamp + random)
  - _Requirements: REQ-SEC-01.4_

- [ ] 10.3 Integrate with Meshtastic Router
  - Hook into `Router::sendPacket()` to intercept outgoing packets
  - If in TrekLink Private mode:
    1. Encrypt payload with AES-128-GCM
    2. Encapsulate in Meshtastic PortNum_PRIVATE_APP packet
    3. Pass to Router for standard Meshtastic PKC encryption (double-layer)
  - Hook into `Router::handleFromRadio()` to decrypt incoming packets
  - _Requirements: REQ-SEC-01.2, REQ-SEC-01.3_

- [ ] 10.4 Implement mode switching (Public vs Private)
  - Add menu option: "TrekLink Mode: Public / Private"
  - **Public Mode:** Use only Meshtastic PKC (universal compatibility)
  - **Private Mode:** Use Meshtastic PKC + AES-128-GCM (TrekLink devices only)
  - **SOS Mode:** Always use Public Mode (override Private setting for SOS)
  - _Requirements: REQ-COM-04, REQ-SEC-01.3_

- [ ] 10.5 Test hybrid encryption
  - **Setup:** 2x TrekLink (Phase 2) + 1x standard Meshtastic device
  - **Procedure:**
    1. Set both TrekLink devices to "Private Mode" with same Channel ID
    2. Send text message from TrekLink A → TrekLink B
    3. Observe standard Meshtastic device CANNOT decrypt message
    4. Switch to "Public Mode", send message
    5. Observe standard Meshtastic device CAN decrypt message
  - **Expected:** Private messages encrypted with AES-GCM, public messages use PKC only
  - _Requirements: REQ-SEC-01_

---

### 11. GPS Power Gating (Phase 2 Enhancement)

- [ ] 11.1 Implement GPS power control in GPSStatus
  - Add power gating functions to Meshtastic GPSStatus class:
    ```cpp
    void enableGPS() {
        digitalWrite(PIN_GPS_PWR_EN, HIGH); // Turn on P-MOSFET
        delay(100); // Wait for GPS boot
    }
    void disableGPS() {
        digitalWrite(PIN_GPS_PWR_EN, LOW); // Cut power
    }
    ```
  - Modify GPS polling loop to power-gate between updates
  - _Requirements: REQ-PWR-01.2, REQ-PWR-03.1_

- [ ] 11.2 Test GPS power gating
  - Measure current consumption with multimeter:
    - GPS enabled: ~45mA
    - GPS disabled: ~5mA (ESP32 + Ra-02 standby)
  - Verify GPS hot start works after re-enabling (internal backup battery retains almanac)
  - _Requirements: REQ-PWR-03.1_

---

### 12. Custom OTA Update Service (Phase 2 Optional)

- [ ] 12.1 Fork Meshtastic OTAModule
  - Copy `src/modules/OTAModule.cpp` → `TrekLinkOTAModule.cpp`
  - Modify firmware URL to point to TrekLink repository
  - Support dual-mode: Meshtastic base updates + TrekLink feature updates
  - _Requirements: REQ-ENV-02.4 (from answers.md Q24)_

- [ ] 12.2 Test OTA update
  - Deploy test firmware to GitHub Releases
  - Trigger OTA update via Meshtastic app
  - Verify ESP32 receives firmware, reboots with new version
  - _Requirements: REQ-ENV-02.4_

---

### Phase 2 Completion Checklist

✅ **Phase 2 SUCCESS CRITERIA:**
- [x] Multi-button support (MENU, SOS, UP, DOWN) functional
- [x] Silent Mode physically cuts OLED power via GPIO 23
- [x] Fall detection triggers auto-SOS after 30s pre-alarm
- [x] TrekLink Private mode encrypts messages with AES-128-GCM
- [x] Meshtastic Public mode works for SOS (universal interoperability)
- [x] GPS power gating reduces idle current consumption
- [x] Custom OTA updates TrekLink firmware

**If all criteria met:** Full TrekLink feature set operational.  
**If incomplete:** Phase 1 device still functional as Meshtastic node.

---

## Testing & Verification

### Integration Tests (Phase 1 + Phase 2)

- [ ] **Test 1:** Basic Meshtastic Mesh (Phase 1)
  - 2 devices, text message exchange, verify delivery

- [ ] **Test 2:** Position Broadcast (Phase 1)
  - Outdoor GPS fix, broadcast position, verify on Meshtastic app

- [ ] **Test 3:** Multi-Hop Routing (Phase 1)
  - 3+ devices in line, verify relay behavior

- [ ] **Test 4:** Fall Detection Auto-SOS (Phase 2)
  - Drop device, verify pre-alarm, cancel test, auto-trigger test

- [ ] **Test 5:** Silent Mode Stealth (Phase 2)
  - Toggle Silent Mode, verify OLED off, vibration-only alerts

- [ ] **Test 6:** TrekLink Private Encryption (Phase 2)
  - 2x TrekLink + 1x standard Meshtastic, verify encryption isolation

- [ ] **Test 7:** SOS Universal Broadcast (Phase 1 + 2)
  - Trigger SOS from TrekLink, verify ALL Meshtastic devices receive (public mode)

---

## Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Ra-02 module DOA** | Phase 1 blocked | Have spare modules, test with known-good LoRa device first |
| **GPIO conflicts** | Hardware damage | Triple-check pinout before wiring, use multimeter to verify |
| **Meshtastic compilation errors** | Phase 1 delays | Use stable v2.6.x branch, follow official build guide |
| **Phase 2 timeframe overrun** | Custom features missing | **Accept Phase 1 as deliverable**, prioritize working hardware |
| **Fall detection false positives** | User annoyance | Implement tunable sensitivity, allow user to disable feature |

---

## Dependency Tree

```
Phase 1 Tasks:
├── 1. Meshtastic Repo Setup (1.1, 1.2)
├── 2. GPIO Configuration (2.1-2.6) → depends on 1.2
├── 3. PlatformIO Config (3.1, 3.2) → depends on 2
├── 4. Compilation (4.1, 4.2) → depends on 3
├── 5. Hardware Testing (5.1-5.6) → depends on 4
└── 6. Interoperability (6.1-6.3) → depends on 5

Phase 2 Tasks (Optional):
├── 7. Multi-Button Module (7.1-7.4) → depends on Phase 1 complete
├── 8. Silent Mode (8.1-8.2) → depends on 7
├── 9. Fall Detection (9.1-9.5) → depends on 7
├── 10. Hybrid Encryption (10.1-10.5) → depends on Phase 1
├── 11. GPS Power Gating (11.1-11.2) → depends on Phase 1
└── 12. Custom OTA (12.1-12.2) → depends on all Phase 2
```

---

## Document Control

- **Author:** AI Development Team
- **Reviewed By:** [Pending User Approval]
- **Next Review Date:** Upon Phase 1 completion
- **Change Log:**
  - v2.0 (2026-02-04): Complete rewrite for Meshtastic fork with two-phase strategy
  - v1.3 (2026-02-02): Original proprietary firmware task list
