/**
 * TrekLink Button Handler Implementation
 * HAL Layer - Button input with debouncing and event detection
 */

#include "hal/button_handler.h"
#include "hardware_config.h"

void ButtonHandler::begin() {
    // Initialize button pins with internal pull-down resistors
    buttons[BTN_ID_MENU].pin = BTN_MENU_PIN;
    buttons[BTN_ID_SOS].pin = BTN_SOS_PIN;
    buttons[BTN_ID_UP].pin = BTN_UP_PIN;
    buttons[BTN_ID_DOWN].pin = BTN_DOWN_PIN;

    for (int i = 0; i < BTN_COUNT; i++) {
        pinMode(buttons[i].pin, INPUT_PULLDOWN);
        buttons[i].currentState = BTN_RELEASED;
        buttons[i].lastState = BTN_RELEASED;
        buttons[i].lastDebounceTime = 0;
        buttons[i].pressTime = 0;
        buttons[i].releaseTime = 0;
        buttons[i].clickPending = false;
        buttons[i].clickTime = 0;
        events[i] = BTN_NONE;
    }

    Serial.println("[ButtonHandler] Initialized");
    Serial.println("  Button config: Active LOW with pull-down");
    Serial.println("  Debounce: 50ms");
    Serial.println("  Double-click window: 300ms");
    Serial.println("  Hold threshold: 1000ms");
}

void ButtonHandler::update() {
    for (int i = 0; i < BTN_COUNT; i++) {
        updateButton((ButtonID)i);
        detectEvents((ButtonID)i);
    }
}

void ButtonHandler::updateButton(ButtonID btn) {
    ButtonState &state = buttons[btn];
    
    // Read current button state (active LOW)
    bool reading = (digitalRead(state.pin) == BTN_PRESSED);
    
    // Debounce logic
    if (reading != state.lastState) {
        state.lastDebounceTime = millis();
    }
    
    if ((millis() - state.lastDebounceTime) > BTN_DEBOUNCE_MS) {
        // State has been stable for debounce period
        if (reading != state.currentState) {
            state.currentState = reading;
            
            // Record press/release times
            if (state.currentState == true) {
                // Button just pressed
                state.pressTime = millis();
            } else {
                // Button just released
                state.releaseTime = millis();
            }
        }
    }
    
    state.lastState = reading;
}

void ButtonHandler::detectEvents(ButtonID btn) {
    ButtonState &state = buttons[btn];
    unsigned long now = millis();
    
    // Detect HOLD event (button still pressed after hold threshold)
    if (state.currentState == true && events[btn] == BTN_NONE) {
        if ((now - state.pressTime) >= BTN_HOLD_MS) {
            events[btn] = BTN_HOLD;
            state.clickPending = false;  // Cancel pending click
            return;
        }
    }
    
    // Detect CLICK and DOUBLE-CLICK on button release
    if (state.currentState == false && state.lastState == true) {
        // Button was just released
        unsigned long pressDuration = state.releaseTime - state.pressTime;
        
        // Only count as click if not a hold
        if (pressDuration < BTN_HOLD_MS) {
            if (state.clickPending) {
                // This is the second click - DOUBLE CLICK
                if ((now - state.clickTime) <= BTN_DOUBLE_CLICK_MS) {
                    events[btn] = BTN_DOUBLE_CLICK;
                    state.clickPending = false;
                } else {
                    // Too slow - treat as new first click
                    state.clickPending = true;
                    state.clickTime = now;
                }
            } else {
                // First click - wait for potential double-click
                state.clickPending = true;
                state.clickTime = now;
            }
        }
    }
    
    // Timeout pending click if double-click window expired
    if (state.clickPending && events[btn] == BTN_NONE) {
        if ((now - state.clickTime) > BTN_DOUBLE_CLICK_MS) {
            events[btn] = BTN_CLICK;
            state.clickPending = false;
        }
    }
}

ButtonEvent ButtonHandler::getEvent(ButtonID btn) {
    if (btn >= BTN_COUNT) return BTN_NONE;
    return events[btn];
}

bool ButtonHandler::isPressed(ButtonID btn) {
    if (btn >= BTN_COUNT) return false;
    return buttons[btn].currentState;
}

void ButtonHandler::clearEvent(ButtonID btn) {
    if (btn < BTN_COUNT) {
        events[btn] = BTN_NONE;
    }
}

void ButtonHandler::printStatus() {
    Serial.println("\n=== Button Status ===");
    
    const char* btnNames[] = {"MENU", "SOS", "UP", "DOWN"};
    const char* eventNames[] = {"NONE", "CLICK", "DOUBLE", "HOLD"};
    
    for (int i = 0; i < BTN_COUNT; i++) {
        Serial.print(btnNames[i]);
        Serial.print(": ");
        Serial.print(buttons[i].currentState ? "PRESSED " : "RELEASED");
        Serial.print(" | Event: ");
        Serial.println(eventNames[events[i]]);
    }
    
    Serial.println("====================\n");
}
