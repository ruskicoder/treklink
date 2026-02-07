/*
 * TrekLink Button Module Implementation
 * Multi-button support for TrekLink device
 */

#include "TrekLinkButtonModule.h"
#include "FallDetectionModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"

TrekLinkButtonModule *trekLinkButtonModule;

TrekLinkButtonModule::TrekLinkButtonModule()
    : SinglePortModule("TrekLinkButton", meshtastic_PortNum_PRIVATE_APP), 
      OSThread("TrekLinkButton"),
      silentModeActive(false),
      vibrationActive(false),
      vibrationStartTime(0)
{
    // Initialize button structures
    initButton(menuButton, BTN_MENU);
    initButton(sosButton, BTN_SOS);
    initButton(upButton, BTN_UP);
    initButton(downButton, BTN_DOWN);
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

    // Configure GPIO with interrupts
    pinMode(pin, INPUT);
    
    // Input-only pins (34, 35) require external pull-ups
    // Other pins use internal pull-downs
    if (pin != 34 && pin != 35) {
        pinMode(pin, INPUT_PULLDOWN);
    }
    
    // Attach interrupt for all buttons (CHANGE trigger for both press and release)
    attachInterrupt(digitalPinToInterrupt(pin), 
                    (pin == BTN_MENU) ? menuButtonISR :
                    (pin == BTN_SOS) ? sosButtonISR :
                    (pin == BTN_UP) ? upButtonISR : downButtonISR,
                    CHANGE);
}

// ISR handlers (IRAM_ATTR for ESP32)
void IRAM_ATTR TrekLinkButtonModule::menuButtonISR() {
    if (trekLinkButtonModule) {
        trekLinkButtonModule->menuButton.currentReading = digitalRead(BTN_MENU);
        trekLinkButtonModule->menuButton.lastDebounceTime = millis();
    }
}

void IRAM_ATTR TrekLinkButtonModule::sosButtonISR() {
    if (trekLinkButtonModule) {
        // Input-only pin, invert logic (LOW = pressed with pull-up)
        trekLinkButtonModule->sosButton.currentReading = !digitalRead(BTN_SOS);
        trekLinkButtonModule->sosButton.lastDebounceTime = millis();
    }
}

void IRAM_ATTR TrekLinkButtonModule::upButtonISR() {
    if (trekLinkButtonModule) {
        trekLinkButtonModule->upButton.currentReading = digitalRead(BTN_UP);
        trekLinkButtonModule->upButton.lastDebounceTime = millis();
    }
}

void IRAM_ATTR TrekLinkButtonModule::downButtonISR() {
    if (trekLinkButtonModule) {
        // Input-only pin, invert logic (LOW = pressed with pull-up)
        trekLinkButtonModule->downButton.currentReading = !digitalRead(BTN_DOWN);
        trekLinkButtonModule->downButton.lastDebounceTime = millis();
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
        if (pressed && (now - btn.releaseTime) < DOUBLE_CLICK_WINDOW_MS) {
            btn.clickCount++;
            btn.pressTime = now;
            btn.state = PRESS_DETECTED;
        } else if ((now - btn.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            btn.state = IDLE;
        }
        break;
    }
}

void TrekLinkButtonModule::handleMenuButton()
{
    unsigned long now = millis();

    // Check if we need to cancel fall alarm (any button press)
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        fallDetectionModule->cancelFallAlarm();
        menuButton.clickCount = 0; // Consume the button press
        return;
    }
#endif

    switch (menuButton.state) {
    case HOLD_DETECTED:
        // Hold 1s: Toggle Silent Mode
        toggleSilentMode();
        menuButton.state = IDLE;
        break;

    case IDLE:
        if (menuButton.clickCount == 1 && (now - menuButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            // Single click: Navigate Meshtastic menu (handled by UI system)
            // This is a placeholder - actual navigation happens in UIRenderer
            LOG_DEBUG("MENU: Single click - navigate menu");
            menuButton.clickCount = 0;
        }
        break;

    default:
        break;
    }
}

void TrekLinkButtonModule::handleSOSButton()
{
    unsigned long now = millis();

    switch (sosButton.state) {
    case HOLD_DETECTED:
        // Hold 3s: Trigger SOS
        if ((now - sosButton.pressTime) > 3000) {
            triggerSOS();
            sosButton.state = IDLE;
        }
        break;

    case IDLE:
        if (sosButton.clickCount == 1 && (now - sosButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            // Single click: Broadcast position
            broadcastPosition();
            sosButton.clickCount = 0;
        } else if (sosButton.clickCount == 2 && (now - sosButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
            // Double click: Matrix request (broadcast NodeInfo query)
            LOG_INFO("SOS: Double-click - broadcast NodeInfo request");
            // TODO: Implement matrix request
            sosButton.clickCount = 0;
        }
        break;

    default:
        break;
    }
}

void TrekLinkButtonModule::handleUpButton()
{
    unsigned long now = millis();

    if (upButton.state == IDLE && upButton.clickCount == 1 && (now - upButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
        // Single click: Scroll up (handled by UI/CannedMessage module)
        LOG_DEBUG("UP: Single click - scroll up");
        upButton.clickCount = 0;
    }
}

void TrekLinkButtonModule::handleDownButton()
{
    unsigned long now = millis();

    if (downButton.state == IDLE && downButton.clickCount == 1 && (now - downButton.releaseTime) > DOUBLE_CLICK_WINDOW_MS) {
        // Single click: Scroll down (handled by UI/CannedMessage module)
        LOG_DEBUG("DOWN: Single click - scroll down");
        downButton.clickCount = 0;
    }
}

void TrekLinkButtonModule::toggleSilentMode()
{
    silentModeActive = !silentModeActive;
    
    LOG_INFO("Silent Mode: %s", silentModeActive ? "ON" : "OFF");
    
    // Note: Hardware power gating (GPIO 23) removed in design
    // Silent mode now handled by Meshtastic firmware power management
    // Just provide vibration feedback
    
#ifdef PIN_VIBRATOR
    // Non-blocking vibration: turn on, will be turned off in runOnce() after 200ms
    digitalWrite(PIN_VIBRATOR, HIGH);
    vibrationStartTime = millis();
    vibrationActive = true;
#endif
}

void TrekLinkButtonModule::broadcastPosition()
{
    LOG_INFO("Broadcasting position (ping)");
    
    // Use Meshtastic's position broadcast service
    if (service && service->sendPosition) {
        service->sendPosition();
    }
}

void TrekLinkButtonModule::triggerSOS()
{
    LOG_WARN("SOS TRIGGERED!");
    
    // Create high-priority emergency packet
    meshtastic_MeshPacket *packet = allocMeshPacket();
    packet->channel = 0; // Primary channel (broadcast)
    packet->priority = meshtastic_MeshPacket_Priority_CRITICAL;
    packet->want_ack = false; // No ACK for broadcast emergency
    
    // Set packet type to position with SOS flag
    packet->decoded.portnum = meshtastic_PortNum_POSITION_APP;
    
    // Populate position data from GPS
    meshtastic_Position pos = meshtastic_Position_init_default;
#if !MESHTASTIC_EXCLUDE_GPS
    if (gps && gps->isConnected) {
        pos.latitude_i = gps->latitude;
        pos.longitude_i = gps->longitude;
        pos.altitude = gps->altitude;
        pos.time = gps->getTime();
    }
#endif
    
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

int32_t TrekLinkButtonModule::runOnce()
{
#ifdef PIN_VIBRATOR
    // Handle non-blocking vibration timeout
    if (vibrationActive && (millis() - vibrationStartTime) >= VIBRATION_DURATION_MS) {
        digitalWrite(PIN_VIBRATOR, LOW);
        vibrationActive = false;
    }
#endif

    // Poll all buttons
    updateButton(menuButton);
    updateButton(sosButton);
    updateButton(upButton);
    updateButton(downButton);

    // Handle button actions
    handleMenuButton();
    handleSOSButton();
    handleUpButton();
    handleDownButton();

    // Run at 100Hz (10ms interval) for responsive button detection
    return 10;
}
