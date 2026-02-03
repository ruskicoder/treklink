/**
 * TrekLink ESP32 LoRa Mesh Device
 * Main Entry Point - Dual-Core FreeRTOS Integration
 * 
 * Architecture:
 * - Core 0 (Priority 5): meshTask() - LoRa RX/TX, Mesh routing, Encryption
 * - Core 1 (Priority 2): loop() - Serial CLI, Diagnostics
 */

#include <Arduino.h>
#include "hardware_config.h"
#include "device_config.h"

// Service headers
#include "hal/lora_driver.h"
#include "hal/power_gate.h"
#include "hal/button_handler.h"
#include "services/security_service.h"
#include "services/prfh_service.h"
#include "services/mesh_service.h"
#include "core/service_locator.h"

// ========== Global Service Instances ==========

LoRaDriver loraDriver;
SecurityService securityService;
PRFHService prfhService;
MeshService meshService;
PowerGate powerGate;
ButtonHandler buttonHandler;

// ========== FreeRTOS Task Handles ==========

TaskHandle_t meshTaskHandle = nullptr;

// ========== Forward Declarations ==========

void meshTask(void* pvParameters);
void processCLI();
void logPacket(const Packet& pkt, bool isRX);
void printWelcome();
void printPRFHHop(uint8_t channel);

// ========== Setup ==========

void setup() {
    // Initialize Serial (115200 for CLI)
    Serial.begin(CLI_BAUD_RATE);
    delay(100);  // Allow Serial to stabilize (acceptable in setup)
    
    printWelcome();
    
    Serial.println("[INIT] Initializing hardware...");
    
    // Initialize Power Gate
    powerGate.begin();
    Serial.println("[INIT] ✓ Power Gate initialized");
    
    // Initialize Button Handler
    buttonHandler.begin();
    Serial.println("[INIT] ✓ Button Handler initialized");
    
    // Initialize LoRa Driver
    pinMode(LORA_M0_PIN, OUTPUT);
    pinMode(LORA_M1_PIN, OUTPUT);
    pinMode(LORA_AUX_PIN, INPUT);
    
    loraDriver.begin();
    Serial.println("[INIT] ✓ LoRa E32 initialized (UART2 @ 9600 baud)");
    
    // Initialize Security Service with PSK
    SecResult secRes = securityService.begin(PSK);
    if (secRes == SecResult::OK) {
        Serial.println("[INIT] ✓ Security Service initialized (AES-128-GCM)");
    } else {
        Serial.println("[INIT] ✗ Security Service FAILED");
    }
    
    // Initialize PRFH Service with PSK seed
    uint32_t pskSeed = (PSK[0] << 24) | (PSK[1] << 16) | (PSK[2] << 8) | PSK[3];
    prfhService.begin(pskSeed);
    Serial.println("[INIT] ✓ PRFH Service initialized (LCG hopping)");
    
    // Initialize Mesh Service
    meshService.begin(NODE_ID, RebroadcastMode::ALL);
    meshService.setPacketLogger(logPacket);  // Hook diagnostic logger
    Serial.print("[INIT] ✓ Mesh Service initialized (Node ID: 0x");
    Serial.print(NODE_ID, HEX);
    Serial.println(")");
    
    // Register services with ServiceLocator
    ServiceLocator::registerLora(&loraDriver);
    ServiceLocator::registerSecurity(&securityService);
    ServiceLocator::registerPRFH(&prfhService);
    ServiceLocator::registerMesh(&meshService);
    Serial.println("[INIT] ✓ ServiceLocator registered");
    
    // Create Mesh Task on Core 0 (Priority 5 - High for networking)
    xTaskCreatePinnedToCore(
        meshTask,           // Task function
        "MeshTask",         // Task name
        4096,               // Stack size (bytes)
        nullptr,            // Parameters
        5,                  // Priority (high)
        &meshTaskHandle,    // Task handle
        0                   // Core 0 (networking critical)
    );
    Serial.println("[INIT] ✓ Mesh Task created on Core 0 (Priority 5)");
    
    Serial.println("\n[READY] System initialized. Type 'help' for commands.\n");
}

// ========== Loop (Core 1, Priority 2) ==========

void loop() {
    // Poll Serial CLI
    processCLI();
    
    // Poll button handler (for future UI integration)
    buttonHandler.update();
    
    // Keep loop light - target <10ms iteration
    delay(10);
}

// ========== Mesh Task (Core 0, Priority 5) ==========

void meshTask(void* pvParameters) {
    uint8_t rxBuffer[PACKET_SIZE];
    unsigned long lastHopCheck = 0;
    uint8_t lastChannel = 0;
    
    Serial.println("[MESH] Task started on Core 0");
    
    for (;;) {
        // Check for incoming LoRa packets
        size_t bytesReceived = loraDriver.receive(rxBuffer, PACKET_SIZE);
        if (bytesReceived > 0) {
            // Process through mesh service
            MeshResult res = meshService.processIncoming(rxBuffer, bytesReceived);
            
            // Log result
            if (res != MeshResult::OK && res != MeshResult::ERROR_DUPLICATE) {
                Serial.print("[MESH] RX Error: ");
                Serial.println(static_cast<int>(res));
            }
        }
        
        // Process pending rebroadcasts (handles cooldown timing)
        meshService.processPendingRebroadcasts();
        
        // Check for PRFH hop
        unsigned long now = millis();
        if (now - lastHopCheck >= 1000) {  // Check every 1s
            uint8_t currentChannel = prfhService.hopToNextChannel(now);
            if (currentChannel != lastChannel) {
                printPRFHHop(currentChannel);
                loraDriver.setChannel(currentChannel);  // Apply channel change
                lastChannel = currentChannel;
            }
            lastHopCheck = now;
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ========== CLI Command Processor ==========

void processCLI() {
    if (!Serial.available()) {
        return;
    }
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.length() == 0) {
        return;
    }
    
    // Parse command
    if (cmd.startsWith("tx ")) {
        // Send text message
        String msg = cmd.substring(3);
        Packet pkt;
        pkt.msgType = static_cast<uint8_t>(MessageType::TEXT);
        pkt.msgId = millis() & 0xFFFF;
        pkt.targetIdHash = BROADCAST_ADDR;  // Broadcast
        pkt.latitude = 0;
        pkt.longitude = 0;
        pkt.battery = 100;
        pkt.rssi = -70;
        
        // Copy message to payload (max 24 bytes)
        strncpy((char*)pkt.payload, msg.c_str(), 24);
        
        meshService.sendPacket(pkt);
        Serial.print("[TX] Sent: ");
        Serial.println(msg);
        
    } else if (cmd == "sos") {
        // Trigger SOS
        Packet pkt;
        pkt.msgType = static_cast<uint8_t>(MessageType::SOS);
        pkt.msgId = millis() & 0xFFFF;
        pkt.targetIdHash = BROADCAST_ADDR;
        meshService.sendPacket(pkt);
        Serial.println("[TX] SOS broadcast sent");
        
    } else if (cmd == "hop") {
        // Force channel hop
        uint8_t newChannel = prfhService.hopToNextChannel(millis());
        loraDriver.setChannel(newChannel);
        Serial.print("[PRFH] Forced hop to channel ");
        Serial.println(newChannel);
        
    } else if (cmd == "status") {
        // Print mesh status
        Serial.println("\n=== TrekLink Status ===");
        Serial.print("Node ID: 0x");
        Serial.println(NODE_ID, HEX);
        Serial.print("PRFH Channel: ");
        Serial.println(prfhService.getCurrentChannel());
        Serial.print("Neighbors: ");
        Serial.println(meshService.getNeighborCount());
        Serial.print("Uptime: ");
        Serial.print(millis() / 1000);
        Serial.println("s");
        Serial.println("=======================\n");
        
    } else if (cmd == "dump") {
        Serial.println("[CLI] Packet dump is always enabled (via logger hook)");
        
    } else if (cmd == "ids") {
        // Print device ID and obfuscated hash
        uint16_t prfhIndex = prfhService.getCurrentChannel();
        uint16_t hash = securityService.obfuscateId(NODE_ID, prfhIndex);
        Serial.print("[IDS] DeviceID: 0x");
        Serial.print(NODE_ID, HEX);
        Serial.print(" | SenderHash (obfuscated): 0x");
        Serial.println(hash, HEX);
        
    } else if (cmd == "time") {
        // Print network time and PRFH index
        Serial.print("[TIME] Uptime: ");
        Serial.print(millis());
        Serial.print("ms | PRFH Channel: ");
        Serial.println(prfhService.getCurrentChannel());
        
    } else if (cmd == "help") {
        Serial.println("\n=== TrekLink CLI Commands ===");
        Serial.println("tx <msg>  - Send text message");
        Serial.println("sos       - Trigger SOS broadcast");
        Serial.println("hop       - Force PRFH channel hop");
        Serial.println("status    - Print mesh status");
        Serial.println("dump      - Packet logging (always on)");
        Serial.println("ids       - Print DeviceID + SenderHash");
        Serial.println("time      - Print NetworkTime + PRFH Index");
        Serial.println("help      - Show this menu");
        Serial.println("==============================\n");
        
    } else {
        Serial.print("[CLI] Unknown command: ");
        Serial.println(cmd);
    }
}

// ========== Diagnostic Callbacks ==========

void logPacket(const Packet& pkt, bool isRX) {
    Serial.print(isRX ? "[RX] " : "[TX] ");
    Serial.print("MsgID:0x");
    Serial.print(pkt.msgId, HEX);
    Serial.print(" Type:");
    Serial.print(pkt.msgType);
    Serial.print(" Hops:");
    Serial.print(pkt.hopCount);
    Serial.print(" RSSI:");
    Serial.print(pkt.rssi);
    Serial.println("dBm");
}

void printWelcome() {
    Serial.println(R"(
  ╔════════════════════════════════════════╗
  ║    TrekLink ESP32 LoRa Mesh Network    ║
  ║    Phase 2: Ping-Pong Baseline Test    ║
  ╚════════════════════════════════════════╝
)");
}

void printPRFHHop(uint8_t channel) {
    float freq = 433.0 + (channel * 0.025);  // E32 channel spacing: 25kHz
    Serial.print("[PRFH] Hopped to Channel ");
    Serial.print(channel);
    Serial.print(" (");
    Serial.print(freq, 3);
    Serial.print("MHz) | Time: ");
    Serial.print(millis());
    Serial.println("ms");
}
