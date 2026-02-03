/**
 * TrekLink Packet Structure
 * Protocol Layer - Mesh packet format and serialization
 * 
 * Packet Format: <50 bytes total
 * - Header (1 byte): Version + Config flags
 * - IDs (4 bytes): Obfuscated sender + target
 * - Message ID (2 bytes): Unique message identifier
 * - Hop count (1 byte): Remaining hops (max 4)
 * - Message type (1 byte): Text/Ping/SOS/ACK/Matrix
 * - GPS (8 bytes): Latitude + Longitude (scaled int32)
 * - Telemetry (2 bytes): Battery + RSSI
 * - Payload (24 bytes): Encrypted message data
 * - CRC-8 (1 byte): Checksum for integrity
 * 
 * Total: 44 bytes (leaves 6 bytes margin for future expansion)
 */

#ifndef PACKET_H
#define PACKET_H

#include <Arduino.h>

/**
 * Message Types
 */
enum class MessageType : uint8_t {
    TEXT = 0x01,          // Text message
    PING = 0x02,          // Location ping request
    SOS = 0x03,           // Emergency SOS broadcast
    ACK = 0x04,           // Acknowledgment
    MATRIX_REQ = 0x05,    // Matrix update request (double-click SOS)
    MATRIX_RESP = 0x06    // Matrix update response
};

/**
 * Packet Structure (44 bytes)
 * 
 * Optimized for LoRa air time (smaller = faster TX)
 */
struct Packet {
    // Header (1 byte)
    uint8_t header;           // [7:6] Version (00), [5:0] Config flags
    
    // Addressing (4 bytes) - Obfuscated for anti-tracking
    uint16_t senderIdHash;    // Rotating hash of sender device ID
    uint16_t targetIdHash;    // Rotating hash of target ID (0xFFFF = broadcast)
    
    // Message Identification (3 bytes)
    uint16_t msgId;           // Unique message ID (for deduplication)
    uint8_t hopCount;         // Remaining hops (decremented on rebroadcast)
    
    // Message Type (1 byte)
    uint8_t msgType;          // MessageType enum
    
    // GPS Coordinates (8 bytes) - Scaled to int32 for compactness
    int32_t latitude;         // Latitude * 1e7 (e.g., 37.7749 → 377749000)
    int32_t longitude;        // Longitude * 1e7
    
    // Telemetry (2 bytes)
    uint8_t battery;          // Battery percentage (0-100)
    int8_t rssi;              // RSSI in dBm (-120 to -30)
    
    // Encrypted Payload (24 bytes)
    uint8_t payload[24];      // AES-128-GCM encrypted data (includes 8-byte auth tag)
    
    // Integrity Check (1 byte)
    uint8_t crc8;             // CRC-8/MAXIM checksum
    
    // ========== Methods ==========
    
    /**
     * Serialize packet to byte array for transmission
     * @param buffer Destination buffer (must be at least 44 bytes)
     * @return Number of bytes written (always 44)
     */
    size_t serialize(uint8_t* buffer) const;
    
    /**
     * Deserialize byte array into packet structure
     * @param buffer Source buffer (must be at least 44 bytes)
     * @return true if deserialization successful and CRC valid
     */
    bool deserialize(const uint8_t* buffer);
    
    /**
     * Calculate CRC-8 checksum for packet
     * @return CRC-8 value
     * 
     * Uses CRC-8/MAXIM polynomial (0x31)
     */
    uint8_t calculateCRC8() const;
    
    /**
     * Validate CRC-8 checksum
     * @return true if CRC matches calculated value
     */
    bool validateCRC() const;
    
    /**
     * Convert GPS coordinates from float to scaled int32
     * @param lat Latitude in decimal degrees (-90 to +90)
     * @param lon Longitude in decimal degrees (-180 to +180)
     */
    void setGPS(float lat, float lon);
    
    /**
     * Convert scaled int32 GPS back to float
     * @param lat Output latitude
     * @param lon Output longitude
     */
    void getGPS(float& lat, float& lon) const;
    
    /**
     * Check if packet is a broadcast (targetIdHash == 0xFFFF)
     * @return true if broadcast packet
     */
    bool isBroadcast() const;
};

/**
 * Packet size constant (for buffer allocation)
 */
constexpr size_t PACKET_SIZE = 44;

/**
 * Broadcast address constant
 */
constexpr uint16_t BROADCAST_ADDR = 0xFFFF;

#endif // PACKET_H
