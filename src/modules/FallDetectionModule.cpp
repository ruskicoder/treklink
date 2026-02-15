/*
 * Fall Detection Module Implementation
 * Automatic fall detection and SOS triggering for unconscious users
 */

#include "FallDetectionModule.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<Adafruit_MPU6050.h>)

#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"

FallDetectionModule *fallDetectionModule;

FallDetectionModule::FallDetectionModule()
    : SinglePortModule("FallDetection", meshtastic_PortNum_PRIVATE_APP),
      OSThread("FallDetection"),
      currentState(MONITORING),
      freefallStartTime(0),
      impactTime(0),
      inactivityStartTime(0),
      prealarmStartTime(0),
      lastAlarmBeepTime(0),
      sosPatternStartTime(0),
      sosPatternIndex(0),
      sosBuzzerOn(false)
{
    // Initialize MPU6050
    Wire.begin(I2C_SDA, I2C_SCL);
    
    if (!mpu.begin(0x68)) {
        LOG_ERROR("FallDetection: MPU6050 initialization failed!");
        return;
    }
    
    // Configure MPU6050 for fall detection
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);  // ±8g range for impact detection
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);        // ±500°/s for rotation detection
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);     // Low-pass filter to reduce noise
    
    LOG_INFO("FallDetection: Module initialized, state=MONITORING");
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

void FallDetectionModule::activatePreAlarm()
{
    LOG_WARN("FallDetection: PRE-ALARM activated - 30s countdown");
    
    // Activate buzzer alarm (rapid beeping pattern)
#ifdef PIN_BUZZER
    ledcAttachPin(PIN_BUZZER, 0);
    ledcSetup(0, 2700, 8); // 2.7kHz, 8-bit resolution
    ledcWrite(0, 128); // 50% duty cycle (will beep via updateAlarmFeedback())
#endif
    
    // Activate vibration alarm
#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
}

void FallDetectionModule::deactivatePreAlarm()
{
    LOG_INFO("FallDetection: Pre-alarm deactivated");
    
    // Stop buzzer
#ifdef PIN_BUZZER
    ledcWrite(0, 0);
    ledcDetachPin(PIN_BUZZER);
#endif
    
    // Stop vibration
#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, LOW);
#endif
}

void FallDetectionModule::updateAlarmFeedback()
{
    unsigned long now = millis();
    
    // Beep every second during pre-alarm
    if ((now - lastAlarmBeepTime) >= ALARM_BEEP_INTERVAL) {
        lastAlarmBeepTime = now;
        
#ifdef PIN_BUZZER
        // Pulse buzzer (200ms on, 800ms off)
        ledcWrite(0, 128);
        // Note: No delay() - will turn off on next iteration
#endif

#ifdef PIN_VIBRATOR
        // Pulse vibration
        digitalWrite(PIN_VIBRATOR, HIGH);
#endif
    }
    
    // Turn off after 200ms pulse
    if ((now - lastAlarmBeepTime) >= 200) {
#ifdef PIN_BUZZER
        ledcWrite(0, 0);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
    }
}

void FallDetectionModule::triggerAutoSOS()
{
    LOG_CRIT("FallDetection: AUTO-SOS TRIGGERED!");
    
    //=== 1. Send Position Packet ===
    meshtastic_MeshPacket *posPacket = allocDataPacket();
    posPacket->channel = 0; // Primary channel (broadcast)
    posPacket->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    posPacket->want_ack = false; // No ACK for broadcast emergency
    
    posPacket->decoded.portnum = meshtastic_PortNum_POSITION_APP;
    
    // Use node's current position (already updated by GPS/PositionModule)
    meshtastic_Position pos = meshtastic_Position_init_default;
    meshtastic_NodeInfoLite *node = nodeDB->getNodeNum() ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;
    
    float latitude = 0.0f;
    float longitude = 0.0f;
    bool hasPosition = false;
    
    if (node && nodeDB->hasValidPosition(node)) {
        pos.latitude_i = node->position.latitude_i;
        pos.longitude_i = node->position.longitude_i;
        pos.altitude = node->position.altitude;
        pos.time = node->position.time;
        
        // Convert to float for text message
        latitude = node->position.latitude_i * 1e-7;
        longitude = node->position.longitude_i * 1e-7;
        hasPosition = true;
    }
    
    // Encode and send position packet
    posPacket->decoded.payload.size = pb_encode_to_bytes(
        posPacket->decoded.payload.bytes,
        sizeof(posPacket->decoded.payload.bytes),
        &meshtastic_Position_msg,
        &pos
    );
    
    if (service) {
        service->sendToMesh(posPacket);
    }
    
    //=== 2. Send SOS Text Message (Task 9.7, REQ-MSG-04.2) ===
    meshtastic_MeshPacket *textPacket = allocDataPacket();
    textPacket->channel = 0;
    textPacket->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    textPacket->want_ack = false;
    
    textPacket->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    
    // Format message: "SOS - FALL DETECTED [Lat], [Lon]"
    char message[100];
    if (hasPosition) {
        snprintf(message, sizeof(message), "SOS - FALL DETECTED %.6f, %.6f", latitude, longitude);
    } else {
        snprintf(message, sizeof(message), "SOS - FALL DETECTED (No GPS)");
    }
    
    memcpy(textPacket->decoded.payload.bytes, message, strlen(message));
    textPacket->decoded.payload.size = strlen(message);
    
    if (service) {
        service->sendToMesh(textPacket);
    }
    
    LOG_INFO("FallDetection: Sent SOS text: %s", message);
    
    //=== 3. Activate Local Alarms ===
    // Activate SOS Morse code buzzer pattern (... --- ...)
    sosPatternStartTime = millis();
    sosPatternIndex = 0;
    sosBuzzerOn = false;
    
#ifdef LED_PIN
    digitalWrite(LED_PIN, HIGH); // LED strobe handled separately
#endif

#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, HIGH); // Continuous vibration
#endif
}

void FallDetectionModule::updateSOSBuzzerPattern()
{
#ifndef PIN_BUZZER
    return; // No buzzer available
#endif
    
    unsigned long now = millis();
    unsigned long elapsed = now - sosPatternStartTime;
    
    // SOS pattern timing: ... --- ...
    // Pattern array: [beep_duration, gap_duration, beep_duration, gap_duration, ...]
    // S (3 dots): 200ms beep, 200ms gap (repeat 3x)
    // O (3 dashes): 600ms beep, 200ms gap (repeat 3x)
    // S (3 dots): 200ms beep, 200ms gap (repeat 3x)
    
    const unsigned long pattern[] = {
        // S: ... (3 short beeps)
        SOS_SHORT_BEEP, SOS_GAP,  // Dot 1
        SOS_SHORT_BEEP, SOS_GAP,  // Dot 2
        SOS_SHORT_BEEP, SOS_GAP,  // Dot 3
        // O: --- (3 long beeps)
        SOS_LONG_BEEP, SOS_GAP,   // Dash 1
        SOS_LONG_BEEP, SOS_GAP,   // Dash 2
        SOS_LONG_BEEP, SOS_GAP,   // Dash 3
        // S: ... (3 short beeps)
        SOS_SHORT_BEEP, SOS_GAP,  // Dot 1
        SOS_SHORT_BEEP, SOS_GAP,  // Dot 2
        SOS_SHORT_BEEP, SOS_GAP   // Dot 3
    };
    
    const uint8_t patternLength = sizeof(pattern) / sizeof(pattern[0]);
    
    // Calculate total pattern duration
    unsigned long totalDuration = 0;
    for (uint8_t i = 0; i < patternLength; i++) {
        totalDuration += pattern[i];
    }
    
    // Reset pattern after repeat interval
    if (elapsed >= SOS_PATTERN_REPEAT) {
        sosPatternStartTime = now;
        sosPatternIndex = 0;
        sosBuzzerOn = false;
        elapsed = 0;
    }
    
    // Find current position in pattern
    unsigned long accumulatedTime = 0;
    for (uint8_t i = 0; i < patternLength; i++) {
        accumulatedTime += pattern[i];
        
        if (elapsed < accumulatedTime) {
            // We're in this segment
            bool shouldBeOn = (i % 2 == 0); // Even indices = beep, odd = gap
            
            if (shouldBeOn && !sosBuzzerOn) {
                // Turn on buzzer
                ledcAttachPin(PIN_BUZZER, 0);
                ledcSetup(0, 2700, 8); // 2.7kHz
                ledcWrite(0, 128); // 50% duty cycle
                sosBuzzerOn = true;
            } else if (!shouldBeOn && sosBuzzerOn) {
                // Turn off buzzer
                ledcWrite(0, 0);
                sosBuzzerOn = false;
            }
            break;
        }
    }
}

int32_t FallDetectionModule::runOnce()
{
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
        // Note: User cancellation is handled by external call to cancelFallAlarm()
        break;
        
    case SOS_TRIGGERED:
        // SOS active indefinitely until manually reset
        // Update SOS Morse code buzzer pattern
        updateSOSBuzzerPattern();
        break;
    }
    
    // Check every 100ms for responsive detection
    return 100;
}

#endif // MPU6050 available
