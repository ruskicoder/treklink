/**
 * TrekLink Security Service Implementation
 * Service Layer - AES-128-GCM Encryption
 */

#include "services/security_service.h"
#include <string.h>  // For memcpy, memset

// ========== Initialization ==========

SecResult SecurityService::begin(const uint8_t* pskKey) {
    // Copy PSK to internal storage
    memcpy(psk, pskKey, 16);
    
    // Initialize mbedTLS GCM context
    mbedtls_gcm_init(&gcmContext);
    
    // Set key for AES-128-GCM (128 bits = 16 bytes)
    int ret = mbedtls_gcm_setkey(
        &gcmContext,
        MBEDTLS_CIPHER_ID_AES,
        psk,
        128  // Key length in bits
    );
    
    if (ret != 0) {
        initialized = false;
        return SecResult::ERROR_INIT;
    }
    
    initialized = true;
    return SecResult::OK;
}

// ========== Encryption ==========

SecResult SecurityService::encrypt(
    const uint8_t* plaintext,
    size_t plaintextLen,
    uint8_t* ciphertext,
    uint8_t* tag,
    uint16_t msgId,
    uint32_t timestamp
) {
    if (!initialized) {
        return SecResult::ERROR_INIT;
    }
    
    // Generate unique nonce
    uint8_t nonce[12];  // GCM standard nonce size
    generateNonce(nonce, msgId, timestamp);
    
    // Full 16-byte tag buffer (GCM generates 16 bytes, we truncate to 8)
    uint8_t fullTag[16];
    
    // Perform AES-128-GCM encryption
    int ret = mbedtls_gcm_crypt_and_tag(
        &gcmContext,
        MBEDTLS_GCM_ENCRYPT,
        plaintextLen,
        nonce,
        12,  // Nonce length
        nullptr,  // No additional authenticated data (AAD)
        0,        // AAD length
        plaintext,
        ciphertext,
        16,       // Generate full 16-byte tag
        fullTag
    );
    
    if (ret != 0) {
        return SecResult::ERROR_ENCRYPT;
    }
    
    // Truncate tag to 8 bytes (saves packet space)
    memcpy(tag, fullTag, 8);
    
    return SecResult::OK;
}

// ========== Decryption ==========

SecResult SecurityService::decrypt(
    const uint8_t* ciphertext,
    size_t ciphertextLen,
    uint8_t* plaintext,
    const uint8_t* tag,
    uint16_t msgId,
    uint32_t timestamp
) {
    if (!initialized) {
        return SecResult::ERROR_INIT;
    }
    
    // Reconstruct nonce (must match encryption nonce)
    uint8_t nonce[12];
    generateNonce(nonce, msgId, timestamp);
    
    // Expand truncated tag to 16 bytes (pad with zeros)
    uint8_t fullTag[16];
    memcpy(fullTag, tag, 8);
    memset(fullTag + 8, 0, 8);  // Zero-pad upper 8 bytes
    
    // Perform AES-128-GCM decryption with authentication
    int ret = mbedtls_gcm_auth_decrypt(
        &gcmContext,
        ciphertextLen,
        nonce,
        12,  // Nonce length
        nullptr,  // No AAD
        0,        // AAD length
        fullTag,  // 16-byte tag (zero-padded)
        16,       // Tag length
        ciphertext,
        plaintext
    );
    
    if (ret == MBEDTLS_ERR_GCM_AUTH_FAILED) {
        return SecResult::ERROR_AUTH_TAG;  // Message tampered or wrong key
    } else if (ret != 0) {
        return SecResult::ERROR_DECRYPT;
    }
    
    return SecResult::OK;
}

// ========== Header Obfuscation ==========

uint16_t SecurityService::obfuscateId(uint16_t deviceId, uint16_t prfhIndex) {
    // Rotating XOR hash based on PRFH channel index
    // Magic constant 0xA5A5 provides good bit dispersion
    uint16_t hash = (deviceId ^ (prfhIndex * 0xA5A5)) & 0xFFFF;
    return hash;
}

uint16_t SecurityService::deobfuscateId(uint16_t hash, uint16_t prfhIndex) {
    // Reverse operation (XOR is self-inverse)
    uint16_t deviceId = (hash ^ (prfhIndex * 0xA5A5)) & 0xFFFF;
    return deviceId;
}

// ========== Private Helper Methods ==========

void SecurityService::generateNonce(uint8_t* nonce, uint16_t msgId, uint32_t timestamp) {
    // Nonce format (12 bytes):
    // [msgId:2][timestamp:4][PSK-derived padding:6]
    
    // Message ID (2 bytes)
    nonce[0] = (msgId >> 8) & 0xFF;
    nonce[1] = msgId & 0xFF;
    
    // Timestamp (4 bytes)
    nonce[2] = (timestamp >> 24) & 0xFF;
    nonce[3] = (timestamp >> 16) & 0xFF;
    nonce[4] = (timestamp >> 8) & 0xFF;
    nonce[5] = timestamp & 0xFF;
    
    // PSK-derived padding (6 bytes) - Use PSK bytes for entropy
    // This ensures nonce is unique even with same msgId + timestamp
    nonce[6] = psk[0] ^ psk[8];
    nonce[7] = psk[1] ^ psk[9];
    nonce[8] = psk[2] ^ psk[10];
    nonce[9] = psk[3] ^ psk[11];
    nonce[10] = psk[4] ^ psk[12];
    nonce[11] = psk[5] ^ psk[13];
}
