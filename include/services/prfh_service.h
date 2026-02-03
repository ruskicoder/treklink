/**
 * TrekLink PRFH Service
 * Service Layer - Pseudo-Random Frequency Hopping
 * 
 * Implements LCG-based channel hopping synchronized across mesh network
 * Provides anti-jamming and anti-tracking capabilities
 * 
 * Algorithm: Linear Congruential Generator (LCG)
 *   nextVal = (A * prevVal + C) mod M
 *   channel = nextVal % 32  (E32 supports 32 channels)
 * 
 * Parameters:
 *   A = 1664525 (Numerical Recipes)
 *   C = 1013904223
 *   M = 2^32 (implicit overflow of uint32_t)
 *   Seed = PSK XOR timestamp
 */

#ifndef PRFH_SERVICE_H
#define PRFH_SERVICE_H

#include <Arduino.h>

/**
 * PRFH Configuration Constants
 */
constexpr unsigned long PRFH_HOP_INTERVAL_MS = 10000;  // 10 seconds per channel
constexpr unsigned long PRFH_GUARD_INTERVAL_MS = 50;   // 50ms guard for E32 latency + drift
constexpr uint8_t PRFH_CHANNEL_COUNT = 32;             // E32 supports 0-31

/**
 * LCG Parameters (Numerical Recipes)
 */
constexpr uint32_t LCG_A = 1664525;
constexpr uint32_t LCG_C = 1013904223;
// LCG_M = 2^32 (implicit via uint32_t overflow)

/**
 * PRFH Service Class
 * 
 * Manages pseu do-random channel hopping for anti-jamming
 * and network privacy
 */
class PRFHService {
public:
    /**
     * Initialize PRFH with Pre-Shared Key
     * @param psk Pre-shared key (used to seed LCG)
     */
    void begin(uint32_t psk);

    /**
     * Calculate channel for given timestamp
     * @param timestamp Unix timestamp or network time (ms)
     * @return Channel number (0-31)
     * 
     * Deterministic: Same timestamp + PSK → Same channel
     * Used for synchronized hopping across network
     */
    uint8_t calculateCurrentChannel(uint32_t timestamp);

    /**
     * Hop to next channel in sequence
     * @param currentTime Current network time (ms)
     * @return New channel number
     * 
     * Automatically calculates next hop based on time
     */
    uint8_t hopToNextChannel(unsigned long currentTime);

    /**
     * Enter Search Mode (lost sync recovery)
     * Cycles through all 32 channels sequentially
     * Used when node has lost network sync
     * 
     * @param channelDwellMs Time to listen on each channel (default 1000ms)
     * @return Next channel in search sequence
     */
    uint8_t enterSearchMode(unsigned long channelDwellMs = 1000);

    /**
     * Get current channel without hopping
     * @return Current channel number
     */
    uint8_t getCurrentChannel() const;

    /**
     * Check if hop is due (time-based)
     * @param currentTime Current millis() or network time
     * @return true if time to hop to next channel
     */
    bool isHopDue(unsigned long currentTime) const;

    /**
     * Get network time from Sync Master packet
     * @param masterUptime Uptime from heartbeat master (ms)
     * @param localUptime Local millis()
     * @return Synchronized network time offset
     * 
     * PHASE 2 ONLY: Sync Master logic for testing without GPS
     * Phase 3 will use GPS-synchronized Unix timestamps
     */
    uint32_t calculateNetworkTime(uint32_t masterUptime, unsigned long localUptime) const;

    /**
     * Update network time offset (for drift correction)
     * @param networkTime Calculated network time from master
     */
    void updateNetworkTime(uint32_t networkTime);

private:
    uint32_t pskSeed;               // Pre-shared key for LCG seed
    uint32_t lcgState;              // Current LCG state
    uint8_t currentChannel;         // Current channel (0-31)
    unsigned long lastHopTime;      // Timestamp of last hop (millis())
    int32_t networkTimeOffset;      // Offset for Sync Master sync (Phase 2)
    
    // Search mode variables
    bool searchMode;
    uint8_t searchChannelIndex;
    unsigned long searchLastHop;
    
    /**
     * Advance LCG to next state
     * @return New LCG value
     * 
     * LCG formula: next = (A * current + C) mod M
     * Using uint32_t overflow for automatic mod 2^32
     */
    uint32_t nextLCG();
    
    /**
     * Seed LCG with PSK and timestamp
     * @param timestamp Seed timestamp
     */
    void seedLCG(uint32_t timestamp);
};

#endif // PRFH_SERVICE_H
