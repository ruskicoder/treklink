/**
 * TrekLink PRFH Service Implementation
 * Service Layer - Pseudo-Random Frequency Hopping
 */

#include "services/prfh_service.h"

void PRFHService::begin(uint32_t psk) {
    pskSeed = psk;
    currentChannel = 0;
    lastHopTime = 0;
    networkTimeOffset = 0;
    searchMode = false;
    searchChannelIndex = 0;
    searchLastHop = 0;
    
    // Initialize LCG with PSK
    seedLCG(pskSeed);
}

// ========== Channel Calculation ==========

uint8_t PRFHService::calculateCurrentChannel(uint32_t timestamp) {
    // Reseed LCG with timestamp XOR PSK
    seedLCG(timestamp ^ pskSeed);
    
    // Generate channel from LCG state
    uint8_t channel = lcgState % PRFH_CHANNEL_COUNT;
    return channel;
}

uint8_t PRFHService::hopToNextChannel(unsigned long currentTime) {
    // Check if hop is due
    if (!isHopDue(currentTime)) {
        return currentChannel;  // No hop needed
    }
    
    // Calculate time slot (10-second intervals)
    uint32_t timeSlot = (currentTime + networkTimeOffset) / PRFH_HOP_INTERVAL_MS;
    
    // Calculate new channel for this time slot
    currentChannel = calculateCurrentChannel(timeSlot);
    lastHopTime = currentTime;
    
    return currentChannel;
}

uint8_t PRFHService::enterSearchMode(unsigned long channelDwellMs) {
    searchMode = true;
    
    // Check if time to cycle to next channel
    unsigned long now = millis();
    if (now - searchLastHop >= channelDwellMs) {
        searchChannelIndex = (searchChannelIndex + 1) % PRFH_CHANNEL_COUNT;
        searchLastHop = now;
        currentChannel = searchChannelIndex;
    }
    
    return currentChannel;
}

uint8_t PRFHService::getCurrentChannel() const {
    return currentChannel;
}

bool PRFHService::isHopDue(unsigned long currentTime) const {
    // Include guard interval - hop if within 50ms of hop window
    return (currentTime - lastHopTime) >= (PRFH_HOP_INTERVAL_MS - PRFH_GUARD_INTERVAL_MS);
}

// ========== Sync Master Network Time (Phase 2) ==========

uint32_t PRFHService::calculateNetworkTime(uint32_t masterUptime, unsigned long localUptime) const {
    // Calculate delta between master and local time
    // masterUptime is the "network time" reference
    int32_t delta = masterUptime - localUptime;
    return delta; // This is the offset to apply
}

void PRFHService::updateNetworkTime(uint32_t networkTime) {
    networkTimeOffset = networkTime;
}

// ========== Private LCG Implementation ==========

uint32_t PRFHService::nextLCG() {
    // LCG formula: next = (A * current + C) mod M
    // Using uint32_t overflow for automatic mod 2^32
    lcgState = (LCG_A * lcgState + LCG_C);  // Overflow is intentional!
    return lcgState;
}

void PRFHService::seedLCG(uint32_t timestamp) {
    // Seed = timestamp XOR PSK
    lcgState = timestamp ^ pskSeed;
    
    // Advance LCG a few steps to improve distribution
    for (int i = 0; i < 3; i++) {
        nextLCG();
    }
}
