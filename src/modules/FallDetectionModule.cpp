/*
 * Fall Detection Module Implementation
 * Automatic fall detection and SOS triggering for unconscious users
 */

#include "FallDetectionModule.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<Adafruit_MPU6050.h>)

#include "TrekLinkSOSHelper.h"
#include "BuzzerManager.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"

FallDetectionModule *fallDetectionModule;

// F12: Static LUT definition (declared in header)
constexpr bool FallDetectionModule::SOS_LUT[];

FallDetectionModule::FallDetectionModule()
    : SinglePortModule("FallDetection", meshtastic_PortNum_PRIVATE_APP),
      OSThread("FallDetection"),
      currentState(MONITORING),
      mpuInitialized(false),  // F5: Lazy init
      freefallStartTime(0),
      impactTime(0),
      inactivityStartTime(0),
      prealarmStartTime(0),
      lastAlarmBeepTime(0),
      isBeepOn(false),        // F3: Alarm robustness
      sosPatternStartTime(0),
      sosBuzzerOn(false)
{
    // F5: MPU6050 init moved to runOnce() for lazy initialization
    LOG_INFO("FallDetection: Module created, will init MPU6050 in runOnce()");
}

void FallDetectionModule::transitionToState(FallState newState)
{
    if (currentState == newState) return;
    
    LOG_DEBUG("FallDetection: State transition %d -> %d", currentState, newState);
    currentState = newState;
}

float FallDetectionModule::calculateTotalAcceleration(sensors_event_t &accel)
{
    // Calculate magnitude of acceleration vector
    return sqrt(sq(accel.acceleration.x) + 
                sq(accel.acceleration.y) + 
                sq(accel.acceleration.z)) / 9.81f; // Convert to g-force
}

float FallDetectionModule::calculateTotalGyro(sensors_event_t &gyro)
{
    // Calculate magnitude of gyroscope vector
    return sqrt(sq(gyro.gyro.x) + sq(gyro.gyro.y) + sq(gyro.gyro.z));
}

bool FallDetectionModule::checkInactivity(sensors_event_t &accel, sensors_event_t &gyro)
{
    float totalAccel = calculateTotalAcceleration(accel);
    float totalGyro = calculateTotalGyro(gyro);
    
    // Check if device is stationary (acceleration near 1g, no rotation)
    bool accelStill = (fabs(totalAccel - 1.0f) < 0.2f); // Within 0.2g of 1g
    bool gyroStill = (totalGyro < GYRO_STILLNESS_THRESHOLD);
    
    return (accelStill && gyroStill);
}

void FallDetectionModule::cancelFallAlarm()
{
    if (currentState == PRE_ALARM) {
        LOG_INFO("FallDetection: Fall alarm cancelled by user");
        deactivatePreAlarm();
        transitionToState(MONITORING);
    }
}

// F21: SOS exit path — cancels auto-SOS (called by SOS hold 3s)
void FallDetectionModule::cancelAutoSOS()
{
    if (currentState == SOS_TRIGGERED) {
        LOG_INFO("FallDetection: Auto-SOS cancelled by user");
        TrekLinkSOSHelper::instance().deactivateAlarms(OWNER_FALL);
        sosBuzzerOn = false;
        transitionToState(MONITORING);
    }
}

void FallDetectionModule::activatePreAlarm()
{
    LOG_WARN("FallDetection: PRE-ALARM activated - 30s countdown");
    TrekLinkSOSHelper::instance().activateAlarms(OWNER_FALL);
    isBeepOn = true;
}

void FallDetectionModule::deactivatePreAlarm()
{
    LOG_INFO("FallDetection: Pre-alarm deactivated");
    TrekLinkSOSHelper::instance().deactivateAlarms(OWNER_FALL);
    isBeepOn = false;
}

// F3: Robust alarm feedback with explicit isBeepOn state tracking
void FallDetectionModule::updateAlarmFeedback()
{
    unsigned long elapsed = millis() - lastAlarmBeepTime;

    if (elapsed >= ALARM_BEEP_INTERVAL) {
        // Start new beep cycle
        lastAlarmBeepTime = millis();
        isBeepOn = true;

#ifdef PIN_BUZZER
        BuzzerManager::instance().write(128);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, HIGH);
#endif
    } else if (isBeepOn && elapsed >= 200) {
        // End beep after 200ms (200ms on, 800ms off)
        isBeepOn = false;

#ifdef PIN_BUZZER
        BuzzerManager::instance().write(0);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
    }
}

void FallDetectionModule::triggerAutoSOS()
{
    LOG_CRIT("FallDetection: AUTO-SOS TRIGGERED!");
    
    // 1. Send emergency position packet via helper
    TrekLinkSOSHelper::instance().broadcastPosition();
    
    // 2. Send SOS text message with fall context
    TrekLinkSOSHelper::instance().sendSOSTextMessage("SOS - FALL DETECTED");
    
    // 3. Start SOS Morse code LUT pattern (fall-specific buzzer pattern)
    sosPatternStartTime = millis();
    sosBuzzerOn = false;

#ifdef PIN_BUZZER
    // F4: Acquire buzzer via BuzzerManager (already acquired during PRE_ALARM, re-acquire for safety)
    BuzzerManager::instance().acquire(OWNER_FALL);
#endif

    // F22: Vibrator will be pulsed in sync with SOS pattern in runOnce()
}

// F12: LUT-based SOS Morse pattern — O(1) per call, no loop/stack allocation
void FallDetectionModule::updateSOSBuzzerPattern()
{
#ifndef PIN_BUZZER
    return; // No buzzer available
#else
    uint32_t elapsed = millis() - sosPatternStartTime;
    uint8_t index = (elapsed / 200) % SOS_LUT_LEN;
    bool shouldBeOn = SOS_LUT[index];
    
    if (shouldBeOn != sosBuzzerOn) {
        BuzzerManager::instance().write(shouldBeOn ? 128 : 0);
        sosBuzzerOn = shouldBeOn;
    }
#endif
}

int32_t FallDetectionModule::runOnce()
{
    // F5: Lazy init — attempt MPU6050 init in thread context with 5s retry
    if (!mpuInitialized) {
        Wire.begin(I2C_SDA, I2C_SCL);
        if (!mpu.begin(0x68)) {
            LOG_WARN("FallDetection: MPU6050 not ready, retrying in 5s");
            return 5000; // Retry in 5 seconds
        }
        
        // Configure MPU6050 for fall detection
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);  // ±8g range for impact detection
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);        // ±500°/s for rotation detection
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);     // Low-pass filter to reduce noise
        
        mpuInitialized = true;
        LOG_INFO("FallDetection: MPU6050 initialized, state=MONITORING");

        // Safety: ensure vibrator pin is OUTPUT (may already be set by TrekLinkButtonModule)
#ifdef PIN_VIBRATOR
        pinMode(PIN_VIBRATOR, OUTPUT);
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
    }

    // F14: In SOS_TRIGGERED, skip I2C reads — only update buzzer/vibrator/LED
    if (currentState == SOS_TRIGGERED) {
        updateSOSBuzzerPattern();

#ifdef PIN_VIBRATOR
        // F22: Pulse vibrator in sync with SOS pattern (saves ~50% motor power)
        digitalWrite(PIN_VIBRATOR, sosBuzzerOn ? HIGH : LOW);
#endif

#ifdef LED_PIN
        // F23: 2Hz LED strobe during auto-SOS
        digitalWrite(LED_PIN, (millis() / 250) % 2);
#endif

        return 200; // F14: Align with 200ms LUT slots, no I2C needed
    }

    // Read sensor data (only for non-SOS states)
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);
    
    float totalAccel = calculateTotalAcceleration(accel);
    float totalGyro = calculateTotalGyro(gyro);
    unsigned long now = millis();
    
    switch (currentState) {
    case MONITORING:
        // Detect freefall (total acceleration < 0.5g)
        if (totalAccel < FREEFALL_THRESHOLD) {
            freefallStartTime = now;
            transitionToState(FREEFALL_DETECTED);
            LOG_DEBUG("FallDetection: Freefall detected (%.2fg)", totalAccel);
        }
        break;
        
    case FREEFALL_DETECTED:
        // Check if still in freefall
        if (totalAccel >= FREEFALL_THRESHOLD) {
            // Freefall ended, check duration
            unsigned long freefallDuration = now - freefallStartTime;
            
            if (freefallDuration >= FREEFALL_MIN_DURATION) {
                // Valid freefall, now wait for impact
                impactTime = now;
                transitionToState(IMPACT_DETECTED);
                LOG_INFO("FallDetection: Freefall confirmed, waiting for impact");
            } else {
                // Too short, false alarm
                transitionToState(MONITORING);
                LOG_DEBUG("FallDetection: Freefall too short (%lums), ignoring", freefallDuration);
            }
        }
        break;
        
    case IMPACT_DETECTED:
        // Look for high-G impact within 2 seconds of freefall
        if (totalAccel > IMPACT_THRESHOLD) {
            inactivityStartTime = now;
            transitionToState(INACTIVITY_DETECTED);
            LOG_WARN("FallDetection: Impact detected (%.2fg), monitoring for inactivity", totalAccel);
        } else if ((now - impactTime) > 2000) {
            // No impact detected within 2s, false alarm
            transitionToState(MONITORING);
            LOG_DEBUG("FallDetection: No impact after freefall, false alarm");
        }
        break;
        
    case INACTIVITY_DETECTED:
        // Check for movement/activity
        if (!checkInactivity(accel, gyro)) {
            // User is moving, cancel fall detection
            transitionToState(MONITORING);
            LOG_INFO("FallDetection: Movement detected, user is OK");
        } else if ((now - inactivityStartTime) >= INACTIVITY_DURATION) {
            // Inactivity threshold reached, enter pre-alarm
            prealarmStartTime = now;
            lastAlarmBeepTime = now;
            transitionToState(PRE_ALARM);
            activatePreAlarm();
        }
        break;
        
    case PRE_ALARM:
        // Update alarm feedback (beeping/vibration)
        updateAlarmFeedback();
        
        // Check for timeout
        if ((now - prealarmStartTime) >= PREALARM_TIMEOUT) {
            // Timeout reached, trigger auto-SOS
            transitionToState(SOS_TRIGGERED);
            triggerAutoSOS();
        }
        // Note: User cancellation handled by cancelFallAlarm() (any button)
        break;

    default:
        break;
    }
    
    // F14: Adaptive polling — 2Hz idle, 10Hz active detection
    switch (currentState) {
        case MONITORING:    return 500;   // 2Hz idle (saves 80% I2C reads)
        default:            return 100;   // 10Hz active detection
    }
}

#endif // MPU6050 available
