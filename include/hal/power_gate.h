/**
 * TrekLink Power Gating Driver
 * HAL Layer - Hardware power control
 * 
 * Manages GPS P-MOSFET high-side switching and OLED NPN low-side switching
 */

#ifndef POWER_GATE_H
#define POWER_GATE_H

#include <Arduino.h>

/**
 * Power Gating Driver for GPS and OLED modules
 * 
 * GPS: High-side P-MOSFET control via NPN gate driver (GPIO 13)
 * OLED: Low-side NPN GND control (GPIO 23) for Silent Mode
 */
class PowerGate {
public:
    /**
     * Initialize power gating GPIOs
     * Sets up GPS and OLED control pins
     */
    void begin();

    // ========== GPS Power Control (P-MOSFET High-Side) ==========
    
    /**
     * Enable GPS module power
     * GPIO 13 = HIGH → NPN ON → Gate LOW → P-MOSFET ON → GPS powered
     */
    void enableGPS();

    /**
     * Disable GPS module power
     * GPIO 13 = LOW → NPN OFF → Gate HIGH (10kΩ) → P-MOSFET OFF → GPS unpowered
     */
    void disableGPS();

    /**
     * Get GPS power state
     * @return true if GPS is powered, false otherwise
     */
    bool isGPSEnabled();

    // ========== OLED Power Control (NPN Low-Side GND) ==========
    
    /**
     * Enable OLED display
     * GPIO 23 = HIGH → NPN ON → OLED GND connected → Display ON
     */
    void enableOLED();

    /**
     * Disable OLED display (Silent Mode)
     * GPIO 23 = LOW → NPN OFF → OLED GND disconnected → Display OFF
     */
    void disableOLED();

    /**
     * Get OLED power state
     * @return true if OLED is enabled, false otherwise
     */
    bool isOLEDEnabled();

    // ========== Diagnostic Functions ==========
    
    #ifdef DEBUG
    /**
     * Print power states to Serial
     * For debugging and verification
     */
    void printStatus();
    #endif

private:
    bool gpsEnabled;    // Track GPS power state
    bool oledEnabled;   // Track OLED power state
};

#endif // POWER_GATE_H
