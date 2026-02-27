/*
 * TrekLink Button Module Implementation
 * SOS button support for TrekLink device emergency functions
 * 
 * Note: UP/DOWN/MENU navigation is handled by TrekLinkButtonInput + InputBroker
 */

#include "TrekLinkButtonModule.h"
#include "BuzzerManager.h"
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
      sosPinChanged(false),
      sosActive(false),
      sosStartTime(0),
      lastSOSTxTime(0),
      vibrationActive(false),
      vibrationStartTime(0)
{
    // Initialize SOS button only
    initButton(sosButton, BTN_SOS);

#ifdef PIN_BUZZER
    // One-time BuzzerManager init (F4: prevents LEDC channel conflict)
    BuzzerManager::instance().init();
#endif
}

void TrekLinkButtonModule::initButton(ButtonInfo &btn, uint8_t pin)
{
    btn.pin = pin;
    btn.state = IDLE;
    btn.pressTime = 0;
    btn.releaseTime = 0;
    btn.lastReading = false;    // F2: Not pressed (inverted: pull-up, LOW=pressed)
    btn.currentReading = false; // F2: Prevents phantom press at boot
    btn.lastDebounceTime = 0;
    btn.clickCount = 0;

    // Configure GPIO - Input-only pin (34) requires external pull-up
    pinMode(pin, INPUT);
    
    // Attach interrupt (CHANGE trigger for both press and release)
    attachInterrupt(digitalPinToInterrupt(pin), sosButtonISR, CHANGE);
    
    LOG_INFO("TrekLinkButton: SOS button initialized on GPIO %d", pin);
}

// ISR handler for SOS button (F1: volatile flag + setInterval(0) for immediate wake)
void IRAM_ATTR TrekLinkButtonModule::sosButtonISR() {
    if (trekLinkButtonModule) {
        trekLinkButtonModule->sosPinChanged = true;     // ISR-safe: no millis()
        trekLinkButtonModule->setInterval(0);           // Wake thread immediately
    }
}

void TrekLinkButtonModule::updateButton(ButtonInfo &btn)
{
    // F1: Pin state is now read in runOnce() thread context, not ISR
    unsigned long now = millis();
    
    // Debounce: only process if stable for DEBOUNCE_MS
    if ((now - btn.lastDebounceTime) < DEBOUNCE_MS) {
        return; // Still bouncing, ignore
    }
    
    bool pressed = (btn.currentReading != btn.lastReading && btn.currentReading == true);
    bool released = (btn.currentReading != btn.lastReading && btn.currentReading == false);
    
    if (pressed || released) {
        btn.lastReading = btn.currentReading;
    } else {
        return; // No state change
    }

    // F18: Simplified state machine (removed WAIT_DOUBLE_CLICK)
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
            btn.clickCount = 1;
            btn.releaseTime = now;
            btn.state = IDLE; // F18: Immediate transition, no 300ms wait
        } else if ((now - btn.pressTime) > HOLD_THRESHOLD_MS) {
            btn.state = HOLD_DETECTED;
        }
        break;

    case HOLD_DETECTED:
        if (released) {
            btn.state = IDLE;
        }
        break;
    }
}

void TrekLinkButtonModule::handleSOSButton()
{
    unsigned long now = millis();

    // F11/F24: Any SOS button press cancels fall PRE_ALARM
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        if (sosButton.state == PRESS_DETECTED || sosButton.state == HOLD_DETECTED) {
            LOG_INFO("SOS Button: Cancelling fall alarm (any press)");
            fallDetectionModule->cancelFallAlarm();
            sosButton.state = IDLE;
            sosButton.clickCount = 0;
            return;
        }
    }
    // F21: SOS hold 3s also cancels auto-SOS (SOS_TRIGGERED state)
    if (fallDetectionModule && fallDetectionModule->isInSOSTriggered()) {
        if (sosButton.state == HOLD_DETECTED && (now - sosButton.pressTime) >= HOLD_THRESHOLD_MS) {
            LOG_INFO("SOS Button: Cancelling auto-SOS (hold 3s)");
            fallDetectionModule->cancelAutoSOS();
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
                lastSOSTxTime = now; // F10: Reset beacon timer
            } else {
                LOG_INFO("SOS Button: Cancelling SOS mode (hold 3s)");
                cancelSOS();
                sosActive = false;
            }
            sosButton.state = IDLE;
        }
        break;

    case IDLE:
        // F18: Single click processes immediately (no 300ms WAIT_DOUBLE_CLICK delay)
        if (sosButton.clickCount == 1) {
            LOG_INFO("SOS Button: Broadcasting position (single click)");
            broadcastPosition();
            sosButton.clickCount = 0;
        }
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
    packet->priority = meshtastic_MeshPacket_Priority_MAX; // F15: MAX priority
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
    // F4: Use BuzzerManager instead of raw LEDC
    BuzzerManager::instance().acquire(OWNER_SOS);
    BuzzerManager::instance().write(128); // 50% duty cycle
#endif

#ifdef PIN_VIBRATOR
    // Vibration pulse (will be synced with SOS pattern by runOnce)
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
}

void TrekLinkButtonModule::cancelSOS()
{
    LOG_INFO("SOS CANCELLED");
    
#ifdef PIN_BUZZER
    // F4: Use BuzzerManager - release ownership
    BuzzerManager::instance().write(0);
    BuzzerManager::instance().release(OWNER_SOS);
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
    // F1: Read pin state in thread context (ISR only sets sosPinChanged flag)
    if (sosPinChanged) {
        sosPinChanged = false;
        sosButton.currentReading = !digitalRead(BTN_SOS); // Inverted: pull-up, LOW=pressed
        sosButton.lastDebounceTime = millis();
    }

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

    // F10: SOS beacon - retransmit position periodically
    if (sosActive) {
        unsigned long elapsed = millis() - sosStartTime;
        unsigned long interval = (elapsed < 60000) ? 5000 : 30000; // 5s first min, then 30s
        if ((millis() - lastSOSTxTime) >= interval) {
            LOG_INFO("SOS Beacon: Retransmitting position");
            broadcastPosition();
            lastSOSTxTime = millis();
        }

#ifdef LED_PIN
        // F23: 2Hz LED strobe during SOS
        digitalWrite(LED_PIN, (millis() / 250) % 2);
#endif
    }

    // F13: Event-driven polling. Active = 10ms; Idle = 5000ms (ISR calls setInterval(0) to wake)
    bool active = (sosButton.state != IDLE) || sosActive || vibrationActive;
    return active ? 10 : 5000;
}
