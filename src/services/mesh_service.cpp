/**
 * TrekLink Mesh Service Implementation
 * Service Layer - Managed Flooding Protocol
 */

#include "services/mesh_service.h"
#include "core/service_locator.h"
#include "services/prfh_service.h"
#include "services/security_service.h"
#include "hal/lora_driver.h"
#include <string.h>

// Link quality threshold (RSSI in dBm)
constexpr int8_t RSSI_THRESHOLD = -120;

// Rebroadcast cooldown range (ms)
constexpr unsigned long REBROADCAST_COOLDOWN_MIN = 500;
constexpr unsigned long REBROADCAST_COOLDOWN_MAX = 2000;

void MeshService::begin(uint16_t deviceId, RebroadcastMode mode) {
    localDeviceId = deviceId;
    rebroadcastMode = mode;
    seenBufferIndex = 0;
    neighborCount = 0;
    packetLogger = nullptr;
    
    // Initialize seen buffer
    for (size_t i = 0; i < SEEN_BUFFER_SIZE; i++) {
        seenBuffer[i].key = 0;
        seenBuffer[i].timestamp = 0;
    }
    
    // Initialize pending rebroadcasts
    for (size_t i = 0; i < 4; i++) {
        pendingRebroadcasts[i].pending = false;
    }
}

// ========== Packet Processing ==========

MeshResult MeshService::processIncoming(const uint8_t* rawData, size_t rawLen) {
    // TODO: FEC Decode step (Task 10)
    // For now, assume rawData is already FEC-decoded
    
    // Deserialize packet
    Packet pkt;
    if (!pkt.deserialize(rawData)) {
        return MeshResult::ERROR_CRC;  // CRC validation failed
    }
    
    // Check for duplicate (before decryption - saves CPU)
    if (isDuplicate(pkt.senderIdHash, pkt.msgId)) {
        return MeshResult::ERROR_DUPLICATE;
    }
    
    // Add to seen buffer immediately (prevent reprocessing)
    addToSeenBuffer(pkt.senderIdHash, pkt.msgId);
    
    // Decrypt payload (first 16 bytes = ciphertext, last 8 bytes = auth tag)
    uint8_t ciphertext[16];
    uint8_t tag[8];
    memcpy(ciphertext, pkt.payload, 16);
    memcpy(tag, pkt.payload + 16, 8);
    
    uint8_t plaintext[16];
    uint32_t timestamp = millis();  // Use current time for nonce (simplified for Phase 2)
    
    SecurityService* sec = ServiceLocator::getSecurity();
    if (sec) {
        SecResult decRes = sec->decrypt(ciphertext, 16, plaintext, tag, pkt.msgId, timestamp);
        if (decRes != SecResult::OK) {
            return MeshResult::ERROR_DECRYPT;  // Auth tag verification failed
        }
        
        // Copy decrypted plaintext back to packet
        memcpy(pkt.payload, plaintext, 16);
    }
    
    
    // Check link quality
    if (pkt.rssi < RSSI_THRESHOLD) {
        return MeshResult::ERROR_LINK_QUALITY;
    }
    
    // Log packet (if diagnostic hook registered)
    if (packetLogger) {
        packetLogger(pkt, true);  // true = RX
    }
    
    // Dispatch to message handler
    dispatchMessage(pkt);
    
    // Determine if rebroadcast needed
    if (shouldRebroadcast(pkt)) {
        rebroadcast(pkt);
    }
    
    return MeshResult::OK;
}

MeshResult MeshService::sendPacket(Packet& pkt) {
    // Fill packet metadata
    pkt.hopCount = 4;  // Max 4 hops
    
    // Obfuscate sender ID using PRFH index
    PRFHService* prfh = ServiceLocator::getPRFH();
    SecurityService* sec = ServiceLocator::getSecurity();
    if (prfh && sec) {
        uint16_t prfhIndex = prfh->getCurrentChannel();
        pkt.senderIdHash = sec->obfuscateId(localDeviceId, prfhIndex);
    }
    
    // Calculate CRC
    pkt.crc8 = pkt.calculateCRC8();
    
    // Serialize packet
    uint8_t buffer[PACKET_SIZE];
    pkt.serialize(buffer);
    
    // Encrypt payload  (16 bytes plaintext → 16 bytes ciphertext + 8-byte tag in payload)
    uint8_t plaintext[16];
    memcpy(plaintext, pkt.payload, 16);  // First 16 bytes are plaintext
    
    uint8_t ciphertext[16];
    uint8_t tag[8];
    uint32_t timestamp = millis();
    
    SecResult encRes = sec->encrypt(plaintext, 16, ciphertext, tag, pkt.msgId, timestamp);
    if (encRes != SecResult::OK) {
        return MeshResult::ERROR_DECRYPT;  // Encryption failed
    }
    
    // Pack ciphertext + tag into payload (16 + 8 = 24 bytes)
    memcpy(pkt.payload, ciphertext, 16);
    memcpy(pkt.payload + 16, tag, 8);
    
    // Re-serialize with encrypted payload
    pkt.serialize(buffer);
    
    // TODO: FEC Encode (Task 10)
    // uint8_t fecBuffer[PACKET_SIZE + 32];  // Add 32 parity bytes
    // fec->encode(buffer, PACKET_SIZE, fecBuffer);
    
    
    // Transmit via LoRa
    LoRaDriver* lora = ServiceLocator::getLora();
    if (lora) {
        Result res = lora->transmit(buffer, PACKET_SIZE);
        if (res != Result::OK) {
            return MeshResult::ERROR_LINK_QUALITY;  // TX failed
        }
    }
    
    // Log packet
    if (packetLogger) {
        packetLogger(pkt, false);  // false = TX
    }
    
    return MeshResult::OK;
}

// ========== Message Handlers ==========

void MeshService::handleText(const Packet& pkt) {
    // TODO: Store to message log (Phase 6 - Storage Service)
    // For now, just count as neighbor activity
    neighborCount = 1;  // Simplified - real implementation tracks multiple
}

void MeshService::handlePing(const Packet& pkt) {
    // Send ACK response if packet addressed to us
    uint16_t targetDeviceId = ServiceLocator::getSecurity()->deobfuscateId(
        pkt.targetIdHash, 
        ServiceLocator::getPRFH()->getCurrentChannel()
    );
    
    if (targetDeviceId == localDeviceId) {
        // Build ACK packet
        Packet ackPkt;
        ackPkt.msgType = static_cast<uint8_t>(MessageType::ACK);
        ackPkt.msgId = millis() & 0xFFFF;  // Simple msgId generation
        ackPkt.targetIdHash = pkt.senderIdHash;  // Reply to sender
        ackPkt.latitude = pkt.latitude;  // Echo GPS coords
        ackPkt.longitude = pkt.longitude;
        
        sendPacket(ackPkt);
    }
}

void MeshService::handleSOS(const Packet& pkt) {
    // SOS has priority - TODO: Add to priority queue
    // For now, just handle like TEXT
    handleText(pkt);
}

void MeshService::handleACK(const Packet& pkt) {
    // TODO: Mark message as delivered in message log
    // Update neighbor list
    neighborCount = 1;
}

void MeshService::handleMatrix(const Packet& pkt) {
    // TODO: Update neighbor position table
    // For Phase 2, just count
    neighborCount = 1;
}

// ========== Rebroadcast Logic ==========

bool MeshService::shouldRebroadcast(const Packet& pkt) {
    // Check hop count
    if (pkt.hopCount == 0) {
        return false;
    }
    
    // Check rebroadcast mode
    if (rebroadcastMode == RebroadcastMode::LOCAL_ONLY) {
        // Only rebroadcast if addressed to us
        uint16_t targetId = ServiceLocator::getSecurity()->deobfuscateId(
            pkt.targetIdHash,
            ServiceLocator::getPRFH()->getCurrentChannel()
        );
        return (targetId == localDeviceId);
    }
    
    // ALL mode - rebroadcast everything (default mesh behavior)
    return true;
}

void MeshService::rebroadcast(Packet& pkt) {
    // Decrement hop count
    pkt.hopCount--;
    
    // Find empty slot in pending rebroadcasts
    for (size_t i = 0; i < 4; i++) {
        if (!pendingRebroadcasts[i].pending) {
            pendingRebroadcasts[i].packet = pkt;
            // Random cooldown 500ms-2s
            unsigned long cooldown = random(REBROADCAST_COOLDOWN_MIN, REBROADCAST_COOLDOWN_MAX);
            pendingRebroadcasts[i].rebroadcastTime = millis() + cooldown;
            pendingRebroadcasts[i].pending = true;
            return;
        }
    }
    
    // Queue full - drop packet (overflow protection)
}

void MeshService::processPendingRebroadcasts() {
    unsigned long now = millis();
    
    for (size_t i = 0; i < 4; i++) {
        if (pendingRebroadcasts[i].pending && now >= pendingRebroadcasts[i].rebroadcastTime) {
            sendPacket(pendingRebroadcasts[i].packet);
            pendingRebroadcasts[i].pending = false;
        }
    }
}

// ========== Diagnostic Hooks ==========

void MeshService::setPacketLogger(void (*callback)(const Packet&, bool)) {
    packetLogger = callback;
}

// ========== Private Methods ==========

bool MeshService::isDuplicate(uint16_t senderHash, uint16_t msgId) {
    // Combine into uint32_t key for fast comparison
    uint32_t key = ((uint32_t)senderHash << 16) | msgId;
    
    // Search seen buffer
    for (size_t i = 0; i < SEEN_BUFFER_SIZE; i++) {
        if (seenBuffer[i].key == key) {
            return true;  // Duplicate found
        }
    }
    
    return false;  // Not seen before
}

void MeshService::addToSeenBuffer(uint16_t senderHash, uint16_t msgId) {
    // Combine into uint32_t key
    uint32_t key = ((uint32_t)senderHash << 16) | msgId;
    
    // Add to circular buffer
    seenBuffer[seenBufferIndex].key = key;
    seenBuffer[seenBufferIndex].timestamp = millis();
    
    // Advance index (circular)
    seenBufferIndex = (seenBufferIndex + 1) % SEEN_BUFFER_SIZE;
}

void MeshService::dispatchMessage(const Packet& pkt) {
    // Dispatch based on message type
    switch (static_cast<MessageType>(pkt.msgType)) {
        case MessageType::TEXT:
            handleText(pkt);
            break;
            
        case MessageType::PING:
            handlePing(pkt);
            break;
            
        case MessageType::SOS:
            handleSOS(pkt);
            break;
            
        case MessageType::ACK:
            handleACK(pkt);
            break;
            
        case MessageType::MATRIX_REQ:
        case MessageType::MATRIX_RESP:
            handleMatrix(pkt);
            break;
            
        default:
            // Unknown message type - ignore
            break;
    }
}
