#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<Adafruit_MPU6050.h>)

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

/**
 * Fall Detection Module for TrekLink
 * Monitors MPU6050 accelerometer/gyroscope for fall patterns
 * Triggers automatic SOS if user becomes unconscious after fall
 */

class FallDetectionModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    FallDetectionModule();

    /**
     * Cancel fall detection alarm (called by button press)
     */
    void cancelFallAlarm();

    /**
     * Check if fall detection is currently in pre-alarm state
     */
    bool isInPreAlarm() const { return currentState == PRE_ALARM; }

  protected:
    virtual int32_t runOnce() override;

  private:
    // Fall detection state machine
    enum FallState {
        MONITORING,          // Normal operation, watching for falls
        FREEFALL_DETECTED,   // Detected freefall condition (<0.5g)
        IMPACT_DETECTED,     // Detected impact after freefall (>3g)
        INACTIVITY_DETECTED, // No movement detected after impact
        PRE_ALARM,           // Countdown active, waiting for user cancel
        SOS_TRIGGERED        // Auto-SOS sent, alarm active
    };

    FallState currentState;
    Adafruit_MPU6050 mpu;

    // Timing trackers
    unsigned long freefallStartTime;
    unsigned long impactTime;
    unsigned long inactivityStartTime;
    unsigned long prealarmStartTime;
    unsigned long lastAlarmBeepTime;
    
    // SOS Morse code pattern state
    unsigned long sosPatternStartTime;
    uint8_t sosPatternIndex;
    bool sosBuzzerOn;

    // Detection thresholds (based on design.md)
    static constexpr float FREEFALL_THRESHOLD = 0.5f;      // g (<0.5g indicates freefall)
    static constexpr float IMPACT_THRESHOLD = 3.0f;        // g (>3g indicates impact)
    static constexpr float GYRO_STILLNESS_THRESHOLD = 0.1f; // rad/s (gyro movement threshold)
    static constexpr unsigned long FREEFALL_MIN_DURATION = 500;  // ms
    static constexpr unsigned long INACTIVITY_DURATION = 10000;  // 10 seconds
    static constexpr unsigned long PREALARM_TIMEOUT = 30000;     // 30 seconds
    static constexpr unsigned long ALARM_BEEP_INTERVAL = 1000;   // 1 second beep interval
    
    // SOS Morse code pattern timing (... --- ...)
    static constexpr unsigned long SOS_SHORT_BEEP = 200;   // Short beep (dot)
    static constexpr unsigned long SOS_LONG_BEEP = 600;    // Long beep (dash)
    static constexpr unsigned long SOS_GAP = 200;          // Gap between beeps
    static constexpr unsigned long SOS_PATTERN_REPEAT = 2000; // Repeat pattern every 2s

    // Helper methods
    void transitionToState(FallState newState);
    float calculateTotalAcceleration(sensors_event_t &accel);
    float calculateTotalGyro(sensors_event_t &gyro);
    bool checkInactivity(sensors_event_t &accel, sensors_event_t &gyro);
    
    void activatePreAlarm();
    void deactivatePreAlarm();
    void triggerAutoSOS();
    void updateAlarmFeedback();
    void updateSOSBuzzerPattern();
};

extern FallDetectionModule *fallDetectionModule;

#endif // MPU6050 available
