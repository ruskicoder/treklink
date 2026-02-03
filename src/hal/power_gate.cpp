/**
 * TrekLink Power Gating Driver Implementation
 * HAL Layer - Hardware power control
 */

#include "hal/power_gate.h"
#include "hardware_config.h"

void PowerGate::begin() {
    // Initialize GPS P-MOSFET gate control (via NPN driver)
    pinMode(GPS_POWER_GATE_PIN, OUTPUT);
    digitalWrite(GPS_POWER_GATE_PIN, GPS_POWER_OFF);  // Start with GPS OFF
    gpsEnabled = false;

    // Initialize OLED NPN GND control
    pinMode(OLED_GND_SWITCH_PIN, OUTPUT);
    digitalWrite(OLED_GND_SWITCH_PIN, OLED_POWER_ON);  // Start with OLED ON
    oledEnabled = true;

    Serial.println("[PowerGate] Initialized");
    Serial.print("  GPS Power: ");
    Serial.println(gpsEnabled ? "ON" : "OFF");
    Serial.print("  OLED Power: ");
    Serial.println(oledEnabled ? "ON" : "OFF");
}

// ========== GPS Power Control ==========

void PowerGate::enableGPS() {
    digitalWrite(GPS_POWER_GATE_PIN, GPS_POWER_ON);  // GPIO 13 = HIGH
    gpsEnabled = true;
    
    Serial.println("[PowerGate] GPS Enabled");
    // Allow GPS module to stabilize (typical: 1-100ms depending on hot start)
    delay(10);
}

void PowerGate::disableGPS() {
    digitalWrite(GPS_POWER_GATE_PIN, GPS_POWER_OFF);  // GPIO 13 = LOW
    gpsEnabled = false;
    
    Serial.println("[PowerGate] GPS Disabled");
}

bool PowerGate::isGPSEnabled() {
    return gpsEnabled;
}

// ========== OLED Power Control ==========

void PowerGate::enableOLED() {
    digitalWrite(OLED_GND_SWITCH_PIN, OLED_POWER_ON);  // GPIO 23 = HIGH
    oledEnabled = true;
    
    Serial.println("[PowerGate] OLED Enabled");
    // Allow OLED to stabilize
    delay(5);
}

void PowerGate::disableOLED() {
    digitalWrite(OLED_GND_SWITCH_PIN, OLED_POWER_OFF);  // GPIO 23 = LOW
    oledEnabled = false;
    
    Serial.println("[PowerGate] OLED Disabled (Silent Mode)");
}

bool PowerGate::isOLEDEnabled() {
    return oledEnabled;
}

// ========== Diagnostic Functions ==========

void PowerGate::printStatus() {
    Serial.println("\n=== Power Gate Status ===");
    Serial.print("GPS Power:  ");
    Serial.println(gpsEnabled ? "ENABLED" : "DISABLED");
    Serial.print("  GPIO 13:  ");
    Serial.println(digitalRead(GPS_POWER_GATE_PIN) ? "HIGH" : "LOW");
    
    Serial.print("OLED Power: ");
    Serial.println(oledEnabled ? "ENABLED" : "DISABLED");
    Serial.print("  GPIO 23:  ");
    Serial.println(digitalRead(OLED_GND_SWITCH_PIN) ? "HIGH" : "LOW");
    Serial.println("========================\n");
}
