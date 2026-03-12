/*
 * TrekLink Button Module Implementation
 * SOS button support for TrekLink device emergency functions
 *
 * Debounce: Clean 4-state machine (IDLE→DEBOUNCE→PRESSED→HOLD_ACTIVE)
 * that separates debounce from event detection, preventing swallowed presses.
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

#if HAS_SCREEN
#include "graphics/Screen.h"
#endif

#if !MESHTASTIC_EXCLUDE_GPS
#include "modules/PositionModule.h"
#endif

TrekLinkButtonModule *trekLinkButtonModule;

TrekLinkButtonModule::TrekLinkButtonModule()
    : SinglePortModule("TrekLinkButton", meshtastic_PortNum_PRIVATE_APP),
      OSThread("TrekLinkButton"),
      btnPhase(SOS_BTN_IDLE),
      lastStableReading(true), // Pull-up idle = HIGH
      debounceStartTime(0),
      pressStartTime(0),
      sosActive(false),
      sosStartTime(0),
      lastSOSTxTime(0),
      confirmPulsesRemaining(0),
      confirmPulseOnMs(0),
      confirmPulseOffMs(0),
      lastConfirmPulseTime(0),
      confirmPulseOn(false)
{
    // Configure SOS button GPIO — input-only pin (34) requires external pull-up
    // Note: NO attachInterrupt() — we use polling instead.
    // ESP32 Errata GPIO-3.14: edge interrupts conflict within GPIO group 32-39.
    // GPIOs 32 (UP), 34 (SOS), 35 (DOWN) are all in this group.
    // Polling at 20ms matches the working UpDownInterruptBase pattern.
    pinMode(BTN_SOS, INPUT);
    LOG_INFO("TrekLinkButton: SOS button initialized on GPIO %d (polling mode)", BTN_SOS);

    // Initialize vibrator GPIO (CRITICAL: was missing — GPIO defaults to INPUT on ESP32)
#ifdef PIN_VIBRATOR
    pinMode(PIN_VIBRATOR, OUTPUT);
    digitalWrite(PIN_VIBRATOR, LOW);
    LOG_INFO("TrekLinkButton: Vibrator initialized on GPIO %d", PIN_VIBRATOR);
#endif

#ifdef PIN_BUZZER
    // One-time BuzzerManager init (F4: prevents LEDC channel conflict)
    BuzzerManager::instance().init();
#endif
}

// ============================================================
//  Core Debounce State Machine
//  Separates debounce from event detection to prevent
//  swallowed presses (fixes the original bug).
// ============================================================
void TrekLinkButtonModule::pollButton()
{
    bool rawReading = digitalRead(BTN_SOS); // HIGH=idle, LOW=pressed
    uint32_t now = millis();

    switch (btnPhase) {

    case SOS_BTN_IDLE:
        // Wait for a state change (HIGH→LOW = press start)
        if (rawReading != lastStableReading) {
            btnPhase = SOS_BTN_DEBOUNCE;
            debounceStartTime = now;
        }
        break;

    case SOS_BTN_DEBOUNCE:
        // If the reading bounced back to stable, abort
        if (rawReading == lastStableReading) {
            btnPhase = SOS_BTN_IDLE;
            break;
        }
        // If reading has been stable for DEBOUNCE_MS, accept the transition
        if ((now - debounceStartTime) >= DEBOUNCE_MS) {
            lastStableReading = rawReading;
            if (!rawReading) {
                // Transition: HIGH→LOW = button pressed
                btnPhase = SOS_BTN_PRESSED;
                pressStartTime = now;
                debounceStartTime = 0; // Reset for use as release debounce
                onButtonPress();
            } else {
                // Transition: LOW→HIGH = button released (from DEBOUNCE only if we
                // somehow got here after a HOLD_ACTIVE re-entering debounce)
                btnPhase = SOS_BTN_IDLE;
            }
        }
        break;

    case SOS_BTN_PRESSED:
        // Detect button release (HIGH reading while last stable was LOW)
        if (rawReading == true && lastStableReading == false) {
            // Start release debounce timer (only once)
            if (debounceStartTime == 0) {
                debounceStartTime = now;
            }
            // Wait for stable HIGH for DEBOUNCE_MS
            if ((now - debounceStartTime) >= DEBOUNCE_MS) {
                lastStableReading = true;
                uint32_t holdDuration = now - pressStartTime;
                onButtonRelease(holdDuration);
                btnPhase = SOS_BTN_IDLE;
                debounceStartTime = 0;
            }
        } else {
            // Button still LOW (or bounced back) — reset release debounce
            debounceStartTime = 0;
            // Check hold threshold (no feedback during hold — clean silence)
            uint32_t holdElapsed = now - pressStartTime;
            if (holdElapsed >= HOLD_THRESHOLD_MS) {
                onHoldThreshold();
                btnPhase = SOS_BTN_HOLD_ACTIVE;
            }
        }
        break;

    case SOS_BTN_HOLD_ACTIVE:
        // Wait for button release to return to IDLE
        if (rawReading == true && lastStableReading == false) {
            if (debounceStartTime == 0) {
                debounceStartTime = now;
            }
            if ((now - debounceStartTime) >= DEBOUNCE_MS) {
                lastStableReading = true;
                btnPhase = SOS_BTN_IDLE;
                debounceStartTime = 0;
            }
        } else {
            debounceStartTime = 0;
        }
        break;
    }
}

// ============================================================
//  Event Handlers — called from the debounce state machine
// ============================================================

void TrekLinkButtonModule::onButtonPress()
{
    // F11/F24: Any SOS button press cancels fall PRE_ALARM
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        LOG_INFO("SOS Button: Cancelling fall alarm (press)");
        fallDetectionModule->cancelFallAlarm();
        btnPhase = SOS_BTN_IDLE;
        lastStableReading = true;
        return;
    }
#endif
    // Press start is tracked — hold timing begins in pollButton()
}

void TrekLinkButtonModule::onButtonRelease(uint32_t holdDuration)
{
    // Only process short presses (hold is handled by onHoldThreshold)
    if (holdDuration < HOLD_THRESHOLD_MS) {
        LOG_INFO("SOS Button: Broadcasting position (single click, %dms)", holdDuration);
        broadcastPosition();

        // Ping confirmation: 1 short vibrator pulse
        vibratorPulse(true);
        startConfirmation(1, CONFIRM_PING_MS, 0);

        // Show banner matching Meshtastic convention (MenuHandler.cpp:997)
        showBanner("Position\nSent", 3000);
    }
}

void TrekLinkButtonModule::onHoldThreshold()
{
    // F21: SOS hold 3s also cancels auto-SOS (SOS_TRIGGERED state)
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInSOSTriggered()) {
        LOG_INFO("SOS Button: Cancelling auto-SOS (hold 3s)");
        fallDetectionModule->cancelAutoSOS();
        return;
    }
#endif

    if (!sosActive) {
        LOG_WARN("SOS Button: Triggering SOS mode (hold 3s)");
        triggerSOS();
        sosActive = true;
        sosStartTime = millis();
        lastSOSTxTime = millis();

        // Trigger confirmation: 3 short pulses
        startConfirmation(CONFIRM_TRIGGER_PULSES, CONFIRM_TRIGGER_ON_MS, CONFIRM_TRIGGER_OFF_MS);

        // Show persistent SOS banner (duration=0 = stays until dismissed)
        showBanner("SOS ACTIVE", 0);
    } else {
        LOG_INFO("SOS Button: Cancelling SOS mode (hold 3s)");
        cancelSOS();
        sosActive = false;

        // Cancel confirmation: 2 long pulses
        startConfirmation(CONFIRM_CANCEL_PULSES, CONFIRM_CANCEL_ON_MS, CONFIRM_CANCEL_OFF_MS);

        // Show cancel banner for 3 seconds
        showBanner("SOS Cancelled", 3000);
    }
}

// ============================================================
//  SOS Core Functions
// ============================================================

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
    packet->priority = meshtastic_MeshPacket_Priority_MAX; // MAX priority
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
        pos.time = node->position.time;
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

    // Activate local alarms (buzzer continuous, vibrator will be synced)
    activateSOSAlarms();
}

void TrekLinkButtonModule::activateSOSAlarms()
{
    LOG_INFO("Activating SOS alarms");

#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(OWNER_SOS);
    BuzzerManager::instance().write(128); // 50% duty cycle
#endif

#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
}

void TrekLinkButtonModule::cancelSOS()
{
    LOG_INFO("SOS CANCELLED");

#ifdef PIN_BUZZER
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

// ============================================================
//  Feedback Helpers (all non-blocking)
// ============================================================


void TrekLinkButtonModule::startConfirmation(uint8_t pulses, uint32_t onMs, uint32_t offMs)
{
    confirmPulsesRemaining = pulses;
    confirmPulseOnMs = onMs;
    confirmPulseOffMs = offMs;
    confirmPulseOn = true;
    lastConfirmPulseTime = millis();
    vibratorPulse(true);
}

void TrekLinkButtonModule::updateConfirmation()
{
    if (confirmPulsesRemaining == 0)
        return;

    uint32_t now = millis();
    uint32_t elapsed = now - lastConfirmPulseTime;

    if (confirmPulseOn) {
        // Currently ON — wait for ON duration
        if (elapsed >= confirmPulseOnMs) {
            vibratorPulse(false);
            confirmPulseOn = false;
            confirmPulsesRemaining--;
            lastConfirmPulseTime = now;

            if (confirmPulsesRemaining == 0) {
                return; // Done
            }
        }
    } else {
        // Currently OFF — wait for OFF duration
        if (elapsed >= confirmPulseOffMs) {
            vibratorPulse(true);
            confirmPulseOn = true;
            lastConfirmPulseTime = now;
        }
    }
}

void TrekLinkButtonModule::vibratorPulse(bool on)
{
#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, on ? HIGH : LOW);
#endif
}

void TrekLinkButtonModule::showBanner(const char *message, uint32_t durationMs)
{
#if HAS_SCREEN
    if (screen) {
        screen->showSimpleBanner(message, durationMs);
    }
#endif
}

// ============================================================
//  Main run loop — polled every 20ms
// ============================================================
int32_t TrekLinkButtonModule::runOnce()
{
    // Run the debounce state machine
    pollButton();

    // Run confirmation feedback (non-blocking pulse sequence)
    updateConfirmation();

    // SOS beacon — retransmit position periodically
    if (sosActive) {
        uint32_t now = millis();
        uint32_t elapsed = now - sosStartTime;
        uint32_t interval = (elapsed < 60000) ? 5000 : 30000; // 5s first min, then 30s
        if ((now - lastSOSTxTime) >= interval) {
            LOG_INFO("SOS Beacon: Retransmitting position");
            broadcastPosition();
            lastSOSTxTime = now;
        }

#ifdef LED_PIN
        // 2Hz LED strobe during SOS
        digitalWrite(LED_PIN, (now / 250) % 2);
#endif
    }

    return 20; // Poll every 20ms (matches UpDownInterruptBase)
}
