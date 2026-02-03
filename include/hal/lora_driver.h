/**
 * TrekLink LoRa E32 Driver
 * HAL Layer - Ebyte E32-433T20D UART LoRa Module
 * 
 * Hardware: E32-433T20D (UART interface, 433MHz, 20dBm)
 * Interface: UART2 (TX:17, RX:16), M0:18, M1:19, AUX:27
 * 
 * CRITICAL: Use 0xC2 command prefix for RAM-based channel changes
 *           (NOT 0xC0 which wears out EEPROM)
 */

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <Arduino.h>

/**
 * E32 Operating Modes (controlled via M0/M1 pins)
 */
enum class LoRaMode {
    NORMAL,         // M0=0, M1=0 - Normal TX/RX mode
    WAKEUP,         // M0=1, M1=0 - Wake-up mode (250ms preamble)
    POWER_SAVING,   // M0=0, M1=1 - Ultra-low power sleep
    CONFIG          // M0=1, M1=1 - Configuration mode
};

/**
 * Result codes for LoRa operations
 */
enum class Result {
    OK,
    ERROR_TIMEOUT,
    ERROR_UART,
    ERROR_AUX,
    ERROR_BUSY,
    ERROR_INVALID_PARAM
};

/**
 * LoRa E32 Driver Class
 * 
 * Provides non-blocking interface to E32-433T20D module
 * Supports channel hopping, CSMA-CA, and Wake-on-Radio
 */
class LoRaDriver {
public:
    /**
     * Initialize LoRa module
     * Sets up UART, GPIO pins, and default configuration
     */
    void begin();

    // ========== Mode Control ==========
    
    /**
     * Set operating mode
     * @param mode Target mode (NORMAL, WAKEUP, POWER_SAVING, CONFIG)
     * @return Result::OK or error code
     */
    Result setMode(LoRaMode mode);

    /**
     * Get current operating mode
     * @return Current LoRaMode
     */
    LoRaMode getMode() const;

    // ========== Channel Control ==========
    
    /**
     * Set LoRa channel (0-31 for 433MHz band)
     * Uses 0xC2 RAM write command (no EEPROM wear)
     * 
     * @param channel Channel number (0-31)
     * @return Result::OK or error code
     * 
     * CRITICAL: Includes 40ms settling time after channel switch
     */
    Result setChannel(uint8_t channel);

    /**
     * Get current channel
     * @return Channel number (0-31)
     */
    uint8_t getChannel() const;

    // ========== Transmission ==========
    
    /**
     * Transmit packet (non-blocking with CSMA-CA)
     * 
     * @param data Packet data buffer
     * @param len Data length (max 512 bytes for E32)
     * @return Result::OK or error code
     * 
     * Implements CSMA collision avoidance with random backoff
     */
    Result transmit(const uint8_t* data, size_t len);

    /**
     * Check if channel is busy (for CSMA)
     * @return true if channel occupied
     */
    bool isChannelBusy();

    // ========== Reception ==========
    
    /**
     * Check if data available (AUX pin status)
     * @return true if packet waiting in buffer
     */
    bool isAvailable();

    /**
     * Receive packet (non-blocking)
     * 
     * @param buffer Destination buffer
     * @param maxLen Maximum bytes to read
     * @return Number of bytes read, or 0 if no data
     */
    size_t receive(uint8_t* buffer, size_t maxLen);

    // ========== Link Quality ==========
    
    /**
     * Get RSSI of last received packet
     * @return RSSI in dBm (-120 to -30 typical)
     */
    int8_t getRSSI() const;

    /**
     * Get SNR of last received packet
     * @return SNR in dB (-20 to +10 typical)
     */
    int8_t getSNR() const;

    // ========== Diagnostic Functions ==========
    
    #ifdef DEBUG
    /**
     * Print LoRa status to Serial
     * For debugging and verification
     */
    void printStatus();
    #endif

private:
    // Current state
    LoRaMode currentMode;
    uint8_t currentChannel;
    int8_t lastRSSI;
    int8_t lastSNR;
    
    // Timing for non-blocking operations
    unsigned long lastChannelSwitch;
    unsigned long lastTxAttempt;
    
    // Configuration
    static constexpr unsigned long CHANNEL_SETTLING_TIME = 40;  // ms
    static constexpr unsigned long CSMA_BACKOFF_MIN = 10;        // ms
    static constexpr unsigned long CSMA_BACKOFF_MAX = 50;        // ms
    static constexpr unsigned long AUX_TIMEOUT = 1000;           // ms
    
    /**
     * Wait for AUX pin to go HIGH (module ready)
     * @param timeout_ms Maximum wait time
     * @return true if ready, false if timeout
     */
    bool waitForAUX(unsigned long timeout_ms);
    
    /**
     * Send configuration command to E32
     * @param cmd Command buffer
     * @param len Command length
     * @return Result::OK or error code
     */
    Result sendCommand(const uint8_t* cmd, size_t len);
    
    /**
     * Read RSSI/SNR from E32 status registers
     * Updates lastRSSI and lastSNR
     */
    void updateLinkQuality();
};

#endif // LORA_DRIVER_H
