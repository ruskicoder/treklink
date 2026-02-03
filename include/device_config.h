/**
 * TrekLink Device Configuration
 * Hardcoded values for Phase 2 testing
 *
 * CRITICAL: Change NODE_ID when flashing to second device
 * - Device A: NODE_ID = 0x0001
 * - Device B: NODE_ID = 0x0002
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

// ========== Device Identity ==========

/**
 * Device Node ID (12-bit, 0-4095)
 *
 * CHANGE THIS FOR SECOND DEVICE:
 * - Device A: 0x0001
 * - Device B: 0x0002
 */
#define NODE_ID 0x0001 // ⚠️ CHANGE ME FOR DEVICE B

// ========== Pre-Shared Key (PSK) ==========

/**
 * AES-128 Pre-Shared Key (16 bytes)
 * MUST BE IDENTICAL on all devices in mesh network
 *
 * Used by:
 * - SecurityService (AES-GCM encryption)
 * - PRFHService (LCG seed for channel hopping)
 */
const uint8_t PSK[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

// ========== Network Configuration ==========

// PRFH synchronization mode (Phase 2 testing without GPS)
#define SYNC_MASTER_MODE false // Set true for one device to act as time source

// Serial CLI baud rate (higher than LoRa UART to prevent blocking)
#define CLI_BAUD_RATE 115200

// LoRa UART baud rate (from hardware_config.h, repeated for clarity)
#define LORA_UART_BAUD 9600

#endif // DEVICE_CONFIG_H
