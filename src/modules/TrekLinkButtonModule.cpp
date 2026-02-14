/*
 * TrekLink Button Module Implementation
 * SOS button support for TrekLink device emergency functions
 * 
 * Note: UP/DOWN/MENU navigation is handled by TrekLinkButtonInput + InputBroker
 */

#include "TrekLinkButtonModule.h"
#include "FallDetectionModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"

#if !MESHTASTIC_EXCLUDE_GPS
#include "modules/PositionModule.h"
#endif

TrekLinkButtonModule *trekLinkButtonModule;

TrekLinkButtonModule::TrekLinkButtonModule()
    : SinglePortModule("TrekLinkButton", meshtastic_PortNum_PRIVATE_APP), 
      OSThread("TrekLinkButton"),
      sosActive(false),
      sosStartTime(0),
      vibrationActive(false),
      vibrationStartTime(0)
{
    // Initialize SOS button only
    initButton(sosButton, BTN_SOS);
}

void TrekLinkButtonModule::initButton(ButtonInfo &btn, uint8_t pin)
{
    btn.pin = pin;
    btn.state = IDLE;
    btn.pressTime = 0;
    btn.releaseTime = 0;
    btn.lastReading = HIGH;
    btn.currentReading = HIGH;
    btn.lastDebounceTime = 0;
    btn.clickCount = 0;

    // Configure GPIO - Input-only pin (34) requires external pull-up
    pinMode(pin, INPUT);
    
    // Attach interrupt (CHANGE trigger for both press and release)
    attachInterrupt(digitalPinToInterrupt(pin), sosButtonISR, CHANGE);
    
    LOG_INFO("TrekLinkButton: SOS button initialized on GPIO %d", pin);
}

// ISR handler for SOS button (IRAM_ATTR for ESP32)
void IRAM_ATTR TrekLinkButtonModule::sosButtonISR() {
    if (trekLinkButtonModule) {
        // Input-only pin, invert logic (LOW = pressed with pull-up)
        trekLinkButtonModule->sosButton.currentReading = !digitalRead(BTN_SOS);
        trekLinkButtonModule->sosButton.lastDebounceTime = millis();
    }
}

void TrekLinkButtonModule::updateButton(ButtonInfo &btn)
{
    // currentReading is updated by ISR, check for debounced state changes
    unsigned long now = millis();
    
    // Debounce: only process if stable for DEBOUNCE_MS
    if ((now - btn.lastDebounceTime) < DEBOUNCE_MS) {
        return; // Still bouncing, ignore
    }
    
    bool pressed = (btn.currentReading != btn.lastReading && btn.currentReading == HIGH);
    bool released = (btn.currentReading != btn.lastReading && btn.currentReading == LOW);
    
    if (pressed || released) {
        btn.lastReading = btn.currentReading;
    } else {
        return; // No state change
    }

    switch (btn.state) {
    case IDLE:
        if (pressed) {
            btn.state = PRESS_DETECTED;
            btn.pressTime = now;
            btn.clickCount = 0;
        }
        break;

    case PRESS_DETECTED:
        if (released) {
            btn.clickCount++;
            btn.releaseTime = now;
            btn.state = WAIT_DOUBLE_CLICK;
        } else if ((now - btn.pressTime) > HOLD_THRESHOLD_MS) {
            btn.state = HOLD_DETECTED;
        }
        break;

    case HOLD_DETECTED:
        if (released) {
            btn.state = IDLE;
        }
        break;

    case WAIT_DOUBLE_CLICK:
        if ((now - btn.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            btn.state = IDLE;
        }
        break;
    }
}

void TrekLinkButtonModule::handleSOSButton()
{
    unsigned long now = millis();

    // CRITICAL: Fall alarm cancellation - ONLY SOS hold 3s can cancel
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        if (sosButton.state == HOLD_DETECTED && (now - sosButton.pressTime) >= HOLD_THRESHOLD_MS) {
            LOG_INFO("SOS Button: Cancelling fall alarm (hold 3s)");
            fallDetectionModule->cancelFallAlarm();
            sosButton.state = IDLE;
            sosButton.clickCount = 0;
            return;
        }
    }
#endif

    switch (sosButton.state) {
    case HOLD_DETECTED:
        // Hold 3s: Trigger SOS or Cancel active SOS
        if ((now - sosButton.pressTime) >= HOLD_THRESHOLD_MS) {
            if (!sosActive) {
                LOG_WARN("SOS Button: Triggering SOS mode (hold 3s)");
                triggerSOS();
                sosActive = true;
                sosStartTime = now;
            } else {
                LOG_INFO("SOS Button: Cancelling SOS mode (hold 3s)");
                cancelSOS();
                sosActive = false;
            }
            sosButton.state = IDLE;
        }
        break;

    case IDLE:
        // Single click: Broadcast position (ping)
        if (sosButton.clickCount == 1 && (now - sosButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            LOG_INFO("SOS Button: Broadcasting position (single click)");
            broadcastPosition();
            sosButton.clickCount = 0;
        }
        // NOTE: SOS double-click removed per user directive
        break;

    default:
        break;
    }
}

void TrekLinkButtonModule::broadcastPosition()
{
    LOG_INFO("Broadcasting position (ping)");
    
    // Delegate to PositionModule for position broadcast
    if (positionModule) {
        positionModule->sendOurPosition();
    }
}

void TrekLinkButtonModule::triggerSOS()
{
    LOG_WARN("SOS TRIGGERED!");
    
    // Create high-priority emergency packet
    meshtastic_MeshPacket *packet = allocDataPacket();
    packet->channel = 0; // Primary channel (broadcast)
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    packet->want_ack = false; // No ACK for broadcast emergency
    
    // Set packet type to position with SOS flag
    packet->decoded.portnum = meshtastic_PortNum_POSITION_APP;
    
    // Use node's current position (already updated by GPS/PositionModule)
    meshtastic_Position pos = meshtastic_Position_init_default;
    meshtastic_NodeInfoLite *node = nodeDB->getNodeNum() ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;
    if (node && nodeDB->hasValidPosition(node)) {
        pos.latitude_i = node->position.latitude_i;
        pos.longitude_i = node->position.longitude_i;
        pos.altitude = node->position.altitude;
        pos.time = node->position.time;  // Use node's existing time
    }
    
    // Encode position into packet
    packet->decoded.payload.size = pb_encode_to_bytes(
        packet->decoded.payload.bytes,
        sizeof(packet->decoded.payload.bytes),
        &meshtastic_Position_msg,
        &pos
    );
    
    // Send via router
    if (service) {
        service->sendToMesh(packet);
    }
    
    // Activate local alarms
    activateSOSAlarms();
}

void TrekLinkButtonModule::activateSOSAlarms()
{
    LOG_INFO("Activating SOS alarms");
    
#ifdef PIN_BUZZER
    // Continuous buzzer alarm at 2.7kHz
    ledcAttachPin(PIN_BUZZER, 0);
    ledcSetup(0, 2700, 8); // 2.7kHz, 8-bit resolution
    ledcWrite(0, 128); // 50% duty cycle
#endif

#ifdef LED_PIN
    // LED strobe (handled by separate blinker task)
    digitalWrite(LED_PIN, HIGH);
#endif

#ifdef PIN_VIBRATOR
    // Vibration pulse pattern (implement as needed)
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
}

void TrekLinkButtonModule::cancelSOS()
{
    LOG_INFO("SOS CANCELLED");
    
#ifdef PIN_BUZZER
    ledcWrite(0, 0); // Stop buzzer
    ledcDetachPin(PIN_BUZZER);
#endif

#ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
#endif

#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, LOW);
#endif
}

int32_t TrekLinkButtonModule::runOnce()
{
#ifdef PIN_VIBRATOR
    // Handle non-blocking vibration timeout
    if (vibrationActive && (millis() - vibrationStartTime) >= VIBRATION_DURATION_MS) {
        digitalWrite(PIN_VIBRATOR, LOW);
        vibrationActive = false;
    }
#endif

    // Poll SOS button only (UP/DOWN/MENU handled by TrekLinkButtonInput)
    updateButton(sosButton);
    handleSOSButton();

    // Run at 100Hz (10ms interval) for responsive button detection
    return 10;
}
