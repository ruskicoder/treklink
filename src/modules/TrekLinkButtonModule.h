/*
 * TrekLink Button Module Header
 * Handles SOS button for TrekLink device emergency functions
 * 
 * Note: UP/DOWN/MENU navigation buttons are handled by TrekLinkButtonInput
 * which integrates with Meshtastic's InputBroker system.
 */

#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

// SOS Button GPIO (from variant.h)
#define BTN_SOS 34   // SOS button (input-only, requires external pull-up)

// Timing constants (milliseconds)
#define DEBOUNCE_MS 50
#define HOLD_THRESHOLD_MS 3000  // 3 seconds for SOS trigger/cancel

// TrekLink message type discriminator (F8)
#define TREKLINK_MSG_SOS   0x01
#define TREKLINK_MSG_FALL  0x02
#define TREKLINK_MSG_PRIV  0x03  // Future private protocol

class TrekLinkButtonModule : public SinglePortModule, private concurrency::OSThread
{
private:
    // Button state tracking (F18: removed WAIT_DOUBLE_CLICK)
    enum ButtonState {
        IDLE,
        PRESS_DETECTED,
        HOLD_DETECTED
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

    ButtonInfo sosButton;  // SOS button only
    
    // ISR flag (F1: volatile flag pattern, no millis() in ISR)
    volatile bool sosPinChanged;
    
    // SOS state tracking
    bool sosActive;
    unsigned long sosStartTime;
    unsigned long lastSOSTxTime;  // F10: SOS beacon timing
    
    // Vibration feedback timing (non-blocking)
    bool vibrationActive;
    unsigned long vibrationStartTime;
    const unsigned long VIBRATION_DURATION_MS = 200;

    // Private methods
    void initButton(ButtonInfo &btn, uint8_t pin);
    void updateButton(ButtonInfo &btn);
    
    // ISR handler for SOS button (must be static for attachInterrupt)
    static void IRAM_ATTR sosButtonISR();
    
    void handleSOSButton();
    void broadcastPosition();
    void triggerSOS();
    void activateSOSAlarms();
    void cancelSOS();

protected:
    virtual int32_t runOnce() override;

public:
    TrekLinkButtonModule();
    
    // Module lifecycle
    virtual bool wantUIFrame() override { return false; }
    
    // Accessors
    bool isSOSActive() const { return sosActive; }
};

extern TrekLinkButtonModule *trekLinkButtonModule;
