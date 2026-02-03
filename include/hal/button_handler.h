/**
 * TrekLink Button Handler
 * HAL Layer - Button input with debouncing and event detection
 * 
 * Supports: Click, Double-Click, Hold events
 * Buttons: MENU, SOS, UP, DOWN
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

// Button event types
enum ButtonEvent {
    BTN_NONE,           // No event
    BTN_CLICK,          // Single click
    BTN_DOUBLE_CLICK,   // Double click
    BTN_HOLD            // Long press (hold)
};

// Button IDs
enum ButtonID {
    BTN_ID_MENU = 0,
    BTN_ID_SOS,
    BTN_ID_UP,
    BTN_ID_DOWN,
    BTN_COUNT           // Total number of buttons
};

/**
 * Button state tracking for debounce and event detection
 */
struct ButtonState {
    uint8_t pin;                    // GPIO pin number
    bool currentState;              // Current debounced state
    bool lastState;                 // Previous state
    unsigned long lastDebounceTime; // Last state change time
    unsigned long pressTime;        // Time when button was pressed
    unsigned long releaseTime;      // Time when button was released
    bool clickPending;              // Click waiting for double-click timeout
    unsigned long clickTime;        // Time of first click
};

/**
 * Button Handler Class
 * Manages button inputs with debouncing and event detection
 */
class ButtonHandler {
public:
    /**
     * Initialize button handler
     * Sets up GPIO pins with pull-down resistors
     */
    void begin();

    /**
     * Update button states and detect events
     * Call this in main loop (typically every 10-20ms)
     */
    void update();

    /**
     * Get button event for specific button
     * @param btn Button ID (BTN_ID_MENU, BTN_ID_SOS, etc.)
     * @return Button event (BTN_NONE, BTN_CLICK, BTN_DOUBLE_CLICK, BTN_HOLD)
     */
    ButtonEvent getEvent(ButtonID btn);

    /**
     * Check if button is currently pressed
     * @param btn Button ID
     * @return true if button is pressed, false otherwise
     */
    bool isPressed(ButtonID btn);

    /**
     * Clear event for specific button
     * @param btn Button ID
     */
    void clearEvent(ButtonID btn);

    // ========== Diagnostic Functions ==========
    
    #ifdef DEBUG
    /**
     * Print button states to Serial
     * For debugging and verification
     */
    void printStatus();
    #endif

private:
    ButtonState buttons[BTN_COUNT];  // State for all buttons
    ButtonEvent events[BTN_COUNT];   // Pending events

    /**
     * Update single button state
     * @param btn Button ID
     */
    void updateButton(ButtonID btn);

    /**
     * Detect events from button state transitions
     * @param btn Button ID
     */
    void detectEvents(ButtonID btn);
};

#endif // BUTTON_HANDLER_H
