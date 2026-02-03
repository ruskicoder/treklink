/**
 * TrekLink LoRa E32 Driver Implementation
 * HAL Layer - Ebyte E32-433T20D UART LoRa Module
 */

#include "hal/lora_driver.h"
#include "hardware_config.h"

void LoRaDriver::begin() {
    // Initialize UART2 for E32 communication (9600 baud default)
    Serial2.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    
    // Initialize control pins
    pinMode(LORA_M0_PIN, OUTPUT);
    pinMode(LORA_M1_PIN, OUTPUT);
    pinMode(LORA_AUX_PIN, INPUT);
    
    // Start in NORMAL mode
    currentMode = LoRaMode::NORMAL;
    digitalWrite(LORA_M0_PIN, LOW);
    digitalWrite(LORA_M1_PIN, LOW);
    
    // Default channel
    currentChannel = 0;
    
    // Initialize state
    lastRSSI = -120;
    lastSNR = -20;
    lastChannelSwitch = 0;
    lastTxAttempt = 0;
    
    // Wait for module to stabilize (non-blocking in real implementation)
    // In production, setMode() will handle this via waitForAUX()
}

// ========== Mode Control ==========

Result LoRaDriver::setMode(LoRaMode mode) {
    // Set M0/M1 pins according to mode
    switch (mode) {
        case LoRaMode::NORMAL:
            digitalWrite(LORA_M0_PIN, LOW);
            digitalWrite(LORA_M1_PIN, LOW);
            break;
            
        case LoRaMode::WAKEUP:
            digitalWrite(LORA_M0_PIN, HIGH);
            digitalWrite(LORA_M1_PIN, LOW);
            break;
            
        case LoRaMode::POWER_SAVING:
            digitalWrite(LORA_M0_PIN, LOW);
            digitalWrite(LORA_M1_PIN, HIGH);
            break;
            
        case LoRaMode::CONFIG:
            digitalWrite(LORA_M0_PIN, HIGH);
            digitalWrite(LORA_M1_PIN, HIGH);
            break;
    }
    
    // Wait for AUX pin to go HIGH (module ready)
    if (!waitForAUX(AUX_TIMEOUT)) {
        return Result::ERROR_AUX;
    }
    
    currentMode = mode;
    return Result::OK;
}

LoRaMode LoRaDriver::getMode() const {
    return currentMode;
}

// ========== Channel Control ==========

Result LoRaDriver::setChannel(uint8_t channel) {
    if (channel > 31) {
        return Result::ERROR_INVALID_PARAM;
    }
    
    // Switch to CONFIG mode to change channel
    LoRaMode previousMode = currentMode;
    Result res = setMode(LoRaMode::CONFIG);
    if (res != Result::OK) {
        return res;
    }
    
    // Build 0xC2 command for RAM-based channel write
    // Format: 0xC2 + ADDH + ADDL + SPED + CHAN + OPTION
    // We only change CHAN, keep other params at defaults
    uint8_t cmd[] = {
        0xC2,           // RAM write command (NOT 0xC0 EEPROM!)
        0x00,           // ADDH (address high byte)
        0x00,           // ADDL (address low byte)
        0x1A,           // SPED (default: SF9, BW125, Air rate 2.4k)
        channel,        // CHAN (0-31 for 433MHz)
        0x44            // OPTION (default: 20dBm, no relay, RSSI enabled)
    };
    
    // Send command
    res = sendCommand(cmd, sizeof(cmd));
    if (res != Result::OK) {
        setMode(previousMode);  // Restore previous mode
        return res;
    }
    
    // Return to previous mode
    res = setMode(previousMode);
    if (res != Result::OK) {
        return res;
    }
    
    // Mark time for settling delay
    lastChannelSwitch = millis();
    currentChannel = channel;
    
    return Result::OK;
}

uint8_t LoRaDriver::getChannel() const {
    return currentChannel;
}

// ========== Transmission ==========

Result LoRaDriver::transmit(const uint8_t* data, size_t len) {
    // Ensure we're in NORMAL mode
    if (currentMode != LoRaMode::NORMAL) {
        Result res = setMode(LoRaMode::NORMAL);
        if (res != Result::OK) {
            return res;
        }
    }
    
    // Check channel settling time (non-blocking guard interval)
    unsigned long now = millis();
    if (now - lastChannelSwitch < CHANNEL_SETTLING_TIME) {
        return Result::ERROR_BUSY;  // Channel not ready yet
    }
    
    // CSMA-CA: Check if channel is busy
    if (isChannelBusy()) {
        // Random backoff
        unsigned long backoff = random(CSMA_BACKOFF_MIN, CSMA_BACKOFF_MAX);
        if (now - lastTxAttempt < backoff) {
            return Result::ERROR_BUSY;
        }
    }
    
    // Wait for AUX to be HIGH (module ready to transmit)
    if (!waitForAUX(AUX_TIMEOUT)) {
        return Result::ERROR_AUX;
    }
    
    // Transmit data via UART
    size_t written = Serial2.write(data, len);
    if (written != len) {
        return Result::ERROR_UART;
    }
    
    lastTxAttempt = now;
    return Result::OK;
}

bool LoRaDriver::isChannelBusy() {
    // E32 AUX pin is LOW when receiving or transmitting
    // HIGH = channel idle
    return (digitalRead(LORA_AUX_PIN) == LOW);
}

// ========== Reception ==========

bool LoRaDriver::isAvailable() {
    // Check if data available in UART RX buffer
    return (Serial2.available() > 0);
}

size_t LoRaDriver::receive(uint8_t* buffer, size_t maxLen) {
    size_t bytesRead = 0;
    
    while (Serial2.available() > 0 && bytesRead < maxLen) {
        buffer[bytesRead++] = Serial2.read();
    }
    
    // Update link quality metrics if packet received
    if (bytesRead > 0) {
        updateLinkQuality();
    }
    
    return bytesRead;
}

// ========== Link Quality ==========

int8_t LoRaDriver::getRSSI() const {
    return lastRSSI;
}

int8_t LoRaDriver::getSNR() const {
    return lastSNR;
}

// ========== Private Helper Methods ==========

bool LoRaDriver::waitForAUX(unsigned long timeout_ms) {
    unsigned long start = millis();
    
    // Non-blocking wait for AUX to go HIGH
    while (digitalRead(LORA_AUX_PIN) == LOW) {
        if (millis() - start >= timeout_ms) {
            return false;  // Timeout
        }
        yield();  // Allow other tasks to run
    }
    
    return true;  // AUX is HIGH (module ready)
}

Result LoRaDriver::sendCommand(const uint8_t* cmd, size_t len) {
    // Clear UART RX buffer
    while (Serial2.available()) {
        Serial2.read();
    }
    
    // Send command
    size_t written = Serial2.write(cmd, len);
    if (written != len) {
        return Result::ERROR_UART;
    }
    
    // Wait for AUX to indicate command processed
    if (!waitForAUX(AUX_TIMEOUT)) {
        return Result::ERROR_AUX;
    }
    
    return Result::OK;
}

void LoRaDriver::updateLinkQuality() {
    // E32 doesn't provide direct RSSI/SNR via UART
    // These would be read from status registers or estimated
    // For now, placeholder implementation
    
    // In production:
    // 1. Send 0xC0 C1 C2 C3 command to read operating params
    // 2. Parse response for RSSI byte
    // 3. Convert to dBm: RSSI_dBm = -(256 - RSSI_raw)
    
    // Placeholder: Assume moderate signal
    lastRSSI = -70;  // Typical mid-range value
    lastSNR = 5;     // Typical good SNR
}

// ========== Diagnostic Functions ==========

#ifdef DEBUG
void LoRaDriver::printStatus() {
    Serial.println("\n=== LoRa E32 Status ===");
    
    Serial.print("Mode: ");
    switch (currentMode) {
        case LoRaMode::NORMAL:       Serial.println("NORMAL"); break;
        case LoRaMode::WAKEUP:       Serial.println("WAKEUP"); break;
        case LoRaMode::POWER_SAVING: Serial.println("POWER_SAVING"); break;
        case LoRaMode::CONFIG:       Serial.println("CONFIG"); break;
    }
    
    Serial.print("Channel: ");
    Serial.println(currentChannel);
    
    Serial.print("RSSI: ");
    Serial.print(lastRSSI);
    Serial.println(" dBm");
    
    Serial.print("SNR: ");
    Serial.print(lastSNR);
    Serial.println(" dB");
    
    Serial.print("AUX Pin: ");
    Serial.println(digitalRead(LORA_AUX_PIN) ? "HIGH (Ready)" : "LOW (Busy)");
    
    Serial.println("=======================\n");
}
#endif
