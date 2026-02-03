/**
 * TrekLink Security Service
 * Service Layer - AES-128-GCM Encryption with Header Obfuscation
 * 
 * Features:
 * - AES-128-GCM authenticated encryption
 * - 8-byte truncated authentication tags (saves 8 bytes vs 16-byte standard)
 * - Nonce management using msgId + timestamp
 * - Rotating hash for sender/target ID obfuscation (anti-tracking)
 * 
 * Uses ESP32 built-in mbedTLS library
 */

#ifndef SECURITY_SERVICE_H
#define SECURITY_SERVICE_H

#include <Arduino.h>
#include "protocol/packet.h"

// mbedTLS includes (ESP32 built-in)
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>

/**
 * Result codes for security operations
 */
enum class SecResult {
    OK,
    ERROR_INIT,
    ERROR_ENCRYPT,
    ERROR_DECRYPT,
    ERROR_AUTH_TAG,
    ERROR_NONCE
};

/**
 * Security Service Class
 * 
 * Provides encryption, decryption, and ID obfuscation
 * for mesh network privacy and security
 */
class SecurityService {
public:
    /**
     * Initialize security service with Pre-Shared Key
     * @param psk Pre-shared key (16 bytes for AES-128)
     * @return SecResult::OK or error code
     */
    SecResult begin(const uint8_t* psk);

    // ========== Encryption ==========
    
    /**
     * Encrypt plaintext using AES-128-GCM
     * 
     * @param plaintext Input data to encrypt
     * @param plaintextLen Length of plaintext
     * @param ciphertext Output buffer (must be >= plaintextLen)
     * @param tag Output buffer for 8-byte auth tag
     * @param msgId Message ID (part of nonce)
     * @param timestamp Timestamp (part of nonce for uniqueness)
     * @return SecResult::OK or error code
     * 
     * CRITICAL: Nonce MUST be unique for each encryption
     * Nonce = msgId (2 bytes) || timestamp (4 bytes) || padding (6 bytes)
     */
    SecResult encrypt(
        const uint8_t* plaintext,
        size_t plaintextLen,
        uint8_t* ciphertext,
        uint8_t* tag,  // 8-byte truncated tag
        uint16_t msgId,
        uint32_t timestamp
    );

    // ========== Decryption ==========
    
    /**
     * Decrypt ciphertext using AES-128-GCM
     * 
     * @param ciphertext Encrypted data
     * @param ciphertextLen Length of ciphertext
     * @param plaintext Output buffer (must be >= ciphertextLen)
     * @param tag Expected 8-byte auth tag
     * @param msgId Message ID (for nonce reconstruction)
     * @param timestamp Timestamp (for nonce reconstruction)
     * @return SecResult::OK or SecResult::ERROR_AUTH_TAG if tampered
     * 
     * Uses mbedtls_gcm_auth_decrypt() for combined verification + decryption
     */
    SecResult decrypt(
        const uint8_t* ciphertext,
        size_t ciphertextLen,
        uint8_t* plaintext,
        const uint8_t* tag,
        uint16_t msgId,
        uint32_t timestamp
    );

    // ========== Header Obfuscation (Anti-Tracking) ==========
    
    /**
     * Obfuscate device ID using rotating hash
     * 
     * @param deviceId Clear device ID (12-bit, 0-4095)
     * @param prfhIndex Current PRFH channel index (0-31)
     * @return Obfuscated 16-bit hash
     * 
     * Formula: hash = (deviceId XOR (prfhIndex * 0xA5A5)) & 0xFFFF
     * Changes every frequency hop for privacy
     */
    uint16_t obfuscateId(uint16_t deviceId, uint16_t prfhIndex);

    /**
     * Deobfuscate device ID
     * 
     * @param hash Obfuscated ID
     * @param prfhIndex PRFH channel index used during obfuscation
     * @return Clear device ID
     * 
     * Reverse operation of obfuscateId()
     */
    uint16_t deobfuscateId(uint16_t hash, uint16_t prfhIndex);

private:
    mbedtls_gcm_context gcmContext;
    bool initialized;
    
    // Pre-shared key (16 bytes for AES-128)
    uint8_t psk[16];
    
    /**
     * Generate 12-byte nonce from msgId + timestamp
     * 
     * @param nonce Output buffer (12 bytes)
     * @param msgId Message ID
     * @param timestamp Unix timestamp or millis()
     * 
     * Format: [msgId:2][timestamp:4][padding:6]
     * Padding filled with PSK-derived values for additional entropy
     */
    void generateNonce(uint8_t* nonce, uint16_t msgId, uint32_t timestamp);
};

#endif // SECURITY_SERVICE_H
