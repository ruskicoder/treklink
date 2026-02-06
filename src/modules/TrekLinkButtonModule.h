/*
 * TrekLink Button Module Header
 * Handles multi-button input for TrekLink device (MENU, SOS, UP, DOWN)
 * Supports click, double-click, and hold events
 */

#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

// Button GPIO definitions (from variant.h)
#define BTN_MENU 25  // MENU button
#define BTN_SOS 34   // SOS button (input-only, requires external pull-up)
#define BTN_UP 32    // UP button  
#define BTN_DOWN 35  // DOWN button (input-only, requires external pull-up)

// Timing constants (milliseconds)
#define DEBOUNCE_MS 50
#define HOLD_THRESHOLD_MS 1000
#define DOUBLE_CLICK_WINDOW_MS 300

class TrekLinkButtonModule : public SinglePortModule, private concurrency::OSThread
{
private:
    // Button state tracking
    enum ButtonState {
        IDLE,
        PRESS_DETECTED,
        HOLD_DETECTED,
        WAIT_DOUBLE_CLICK
    };

    struct ButtonInfo {
        uint8_t pin;
        ButtonState state;
        unsigned long pressTime;
        unsigned long releaseTime;
        bool lastReading;
        bool currentReading;
        unsigned long lastDebounceTime;
        int clickCount;
    };

    ButtonInfo menuButton;
    ButtonInfo sosButton;
    ButtonInfo upButton;
    ButtonInfo downButton;

    bool silentModeActive;
    
    // Vibration feedback timing (non-blocking)
    bool vibrationActive;
    unsigned long vibrationStartTime;
    const unsigned long VIBRATION_DURATION_MS = 200;

    // Private methods
    void initButton(ButtonInfo &btn, uint8_t pin);
    void updateButton(ButtonInfo &btn);
    
    // ISR handlers (must be static for attachInterrupt)
    static void IRAM_ATTR menuButtonISR();
    static void IRAM_ATTR sosButtonISR();
    static void IRAM_ATTR upButtonISR();
    static void IRAM_ATTR downButtonISR();
    
    void handleMenuButton();
    void handleSOSButton();
    void handleUpButton();
    void handleDownButton();
    void toggleSilentMode();
    void broadcastPosition();
    void triggerSOS();
    void activateSOSAlarms();

protected:
    virtual int32_t runOnce() override;

public:
    TrekLinkButtonModule();
    
    // Module lifecycle
    virtual bool wantUIFrame() override { return false; }
    
    // Accessors
    bool isSilentModeActive() const { return silentModeActive; }
};

extern TrekLinkButtonModule *trekLinkButtonModule;
