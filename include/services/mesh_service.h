/**
 * TrekLink Mesh Service
 * Service Layer - Managed Flooding with Duplicate Detection
 * 
 * Implements mesh network routing, packet processing, and rebroadcast logic.
 * Uses circular buffer + uint32_t keys for efficient deduplication.
 */

#ifndef MESH_SERVICE_H
#define MESH_SERVICE_H

#include <Arduino.h>
#include "protocol/packet.h"

/**
 * Rebroadcast Modes
 */
enum class RebroadcastMode {
    ALL,         // Rebroadcast all packets (default mesh mode)
    LOCAL_ONLY   // Only rebroadcast packets addressed to this device
};

/**
 * Mesh Service Result Codes
 */
enum class MeshResult {
    OK,
    ERROR_DUPLICATE,
    ERROR_CRC,
    ERROR_DECRYPT,
    ERROR_HOP_LIMIT,
    ERROR_LINK_QUALITY
};

/**
 * Circular Buffer for Seen Messages (Deduplication)
 * Stores (SenderHash + MsgID) as uint32_t key
 */
struct SeenMessage {
    uint32_t key;               // (SenderHash << 16) | MsgID
    unsigned long timestamp;    // When packet was seen (for timeout)
};

/**
 * Mesh Service Class
 *  
 * Handles incoming/outgoing packet routing, duplicate detection,
 * rebroadcast logic, and message type dispatching.
 */
class MeshService {
public:
    /**
     * Initialize mesh service
     * @param deviceId Local device ID
     * @param rebroadcastMode Rebroadcast policy
     */
    void begin(uint16_t deviceId, RebroadcastMode mode = RebroadcastMode::ALL);

    // ========== Packet Processing ==========
    
    /**
     * Process incoming packet from LoRa
     * @param rawData Raw packet data from LoRa RX
     * @param rawLen Length of raw data
     * @return MeshResult indicating processing status
     * 
     * Flow: FEC Decode → Decrypt → Check Seen → Route/Rebroadcast
     */
    MeshResult processIncoming(const uint8_t* rawData, size_t rawLen);

    /**
     * Send outgoing packet
     * @param pkt Packet to send
     * @return MeshResult::OK or error
     * 
     * Flow: Fill packet → Encrypt → FEC Encode → LoRa TX
     */
    MeshResult sendPacket(Packet& pkt);

    // ========== Message Handlers ==========
    
    /**
     * Handle TEXT message
     * @param pkt Received packet
     */
    void handleText(const Packet& pkt);

    /**
     * Handle PING message
     * @param pkt Received packet
     */
    void handlePing(const Packet& pkt);

    /**
     * Handle SOS message (Priority)
     * @param pkt Received packet
     */
    void handleSOS(const Packet& pkt);

    /**
     * Handle ACK message
     * @param pkt Received packet
     */
    void handleACK(const Packet& pkt);

    /**
     * Handle MATRIX request/response
     * @param pkt Received packet
     */
    void handleMatrix(const Packet& pkt);

    // ========== Rebroadcast Logic ==========
    
    /**
     * Check if packet should be rebroadcast
     * @param pkt Packet to evaluate
     * @return true if should rebroadcast
     */
    bool shouldRebroadcast(const Packet& pkt);

    /**
     * Rebroadcast packet with cooldown
     * @param pkt Packet to rebroadcast
     * 
     * Applies 500ms-2s random cooldown to prevent broadcast storms
     */
    void rebroadcast(Packet& pkt);

    // ========== Diagnostic Hooks ==========
    
    /**
     * Set callback for packet logging (DiagnosticService)
     * @param callback Function to call on RX/TX packets
     */
    void setPacketLogger(void (*callback)(const Packet&, bool isRX));

    /**
     * Process pending rebroadcasts (call from update loop)
     */
    void processPendingRebroadcasts();

    /**
     * Get neighbor count
     * @return Number of active neighbors
     */
    uint8_t getNeighborCount() const { return neighborCount; }

private:
    uint16_t localDeviceId;
    RebroadcastMode rebroadcastMode;
    
    // Seen buffer (circular buffer, 32 entries)
    static constexpr size_t SEEN_BUFFER_SIZE = 32;
    SeenMessage seenBuffer[SEEN_BUFFER_SIZE];
    uint8_t seenBufferIndex;
    
    // Rebroadcast cooldown tracking
    struct PendingRebroadcast {
        Packet packet;
        unsigned long rebroadcastTime;
        bool pending;
    };
    PendingRebroadcast pendingRebroadcasts[4];  // Max 4 queued rebroadcasts
    
    // Neighbor tracking (simple for now)
    uint8_t neighborCount;
    
    // Diagnostic callback
    void (*packetLogger)(const Packet&, bool isRX);
    
    /**
     * Check if packet is duplicate
     * @param senderHash Sender ID hash
     * @param msgId Message ID
     * @return true if already seen
     */
    bool isDuplicate(uint16_t senderHash, uint16_t msgId);

    /**
     * Add packet to seen buffer
     * @param senderHash Sender ID hash
     * @param msgId Message ID
     */
    void addToSeenBuffer(uint16_t senderHash, uint16_t msgId);

    /**
     * Dispatch packet to appropriate message handler
     * @param pkt Packet to dispatch
     */
    void dispatchMessage(const Packet& pkt);
};

#endif // MESH_SERVICE_H
