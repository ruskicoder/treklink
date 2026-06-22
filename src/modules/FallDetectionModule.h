#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(TREKLINK_VARIANT) && !defined(TREKLINK_V3)

#include "FallSensorInterface.h"

// TrekLink message type discriminator (F8 - shared with TrekLinkSOSHelper.h)
#ifndef TREKLINK_MSG_FALL
#define TREKLINK_MSG_FALL 0x02
#endif

/**
 * Fall Detection Module for TrekLink
 * Monitors IMU accelerometer/gyroscope for fall patterns via FallSensorInterface.
 * Triggers automatic SOS if user becomes unconscious after fall.
 *
 * IMU-agnostic: accepts any FallSensorInterface adapter (MPU6050, ICM20948, QMI8658).
 */

class FallDetectionModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    /**
     * @param sensor  Pointer to an IMU adapter implementing FallSensorInterface.
     *                Ownership is transferred to this module (will be deleted in destructor).
     *                Pass nullptr to disable fall detection (e.g., v3.0 T-Beam with no IMU).
     */
    explicit FallDetectionModule(FallSensorInterface *sensor);
    ~FallDetectionModule();

    /**
     * Cancel fall detection alarm during PRE_ALARM (called by any button press)
     */
    void cancelFallAlarm();

    /**
     * Cancel auto-SOS during SOS_TRIGGERED state (F21: SOS exit path)
     * Called by SOS button hold (3s) to silence buzzer/vibrator/LED
     */
    void cancelAutoSOS();

    /**
     * Check if fall detection is currently in pre-alarm state
     */
    bool isInPreAlarm() const { return currentState == PRE_ALARM; }

    /**
     * Check if fall detection is in SOS_TRIGGERED state (F21)
     */
    bool isInSOSTriggered() const { return currentState == SOS_TRIGGERED; }

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
        SOS_TRIGGERED        // Auto-SOS sent, alarm active (F21: has exit path)
    };

    FallState currentState;
    FallSensorInterface *sensor;  // IMU adapter (owned, may be nullptr)
    bool sensorInitialized;      // Lazy init flag

    // Timing trackers
    unsigned long freefallStartTime;
    unsigned long impactTime;
    unsigned long inactivityStartTime;
    unsigned long prealarmStartTime;
    unsigned long lastAlarmBeepTime;
    bool isBeepOn; // F3: Alarm feedback robustness flag
    
    // SOS Morse code pattern state (F12: LUT-based)
    unsigned long sosPatternStartTime;
    bool sosBuzzerOn;

    // Detection thresholds (based on design.md)
    static constexpr float FREEFALL_THRESHOLD = 0.5f;       // g (<0.5g indicates freefall)
    static constexpr float IMPACT_THRESHOLD = 3.0f;         // g (>3g indicates impact)
    static constexpr float GYRO_STILLNESS_THRESHOLD = 0.1f; // rad/s (gyro movement threshold)
    static constexpr unsigned long FREEFALL_MIN_DURATION = 500;  // ms
    static constexpr unsigned long INACTIVITY_DURATION = 10000;  // 10 seconds
    static constexpr unsigned long PREALARM_TIMEOUT = 30000;     // 30 seconds
    static constexpr unsigned long ALARM_BEEP_INTERVAL = 1000;   // 1 second beep interval
    
    // F12: SOS Morse LUT — 1=ON, 0=OFF. Each index = 200ms slot.
    // Pattern: ... --- ... (gap) = 28 slots × 200ms = 5600ms total
    static constexpr bool SOS_LUT[28] = {
        1,0,1,0,1,0,                       // S (...) — 1200ms
        1,1,1,0,1,1,1,0,1,1,1,0,           // O (---) — 2400ms
        1,0,1,0,1,0,                       // S (...) — 1200ms
        0,0,0,0                             // Gap    —  800ms
    };
    static constexpr uint8_t SOS_LUT_LEN = 28;

    // Helper methods
    void transitionToState(FallState newState);
    float calculateTotalAcceleration(const SensorVec3 &accel);
    float calculateTotalGyro(const SensorVec3 &gyro);
    bool checkInactivity(const SensorVec3 &accel, const SensorVec3 &gyro);
    
    void activatePreAlarm();
    void deactivatePreAlarm();
    void triggerAutoSOS();
    void updateAlarmFeedback();
    void updateSOSBuzzerPattern();
};

extern FallDetectionModule *fallDetectionModule;

#else // stub for non-TrekLink or TREKLINK_V3 builds

class FallDetectionModule {
  public:
    void cancelFallAlarm() {}
    void cancelAutoSOS() {}
    bool isInPreAlarm() const { return false; }
    bool isInSOSTriggered() const { return false; }
};

extern FallDetectionModule *fallDetectionModule;

#endif // guard
