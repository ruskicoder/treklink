/*
 * Fall Detection Module — ICM-20948 implementation
 *
 * Two-stage severity system:
 *   Stage 1: threshold state machine detects the fall event
 *   Stage 2: fall category bitmask + lethal meter score → LETHAL / NON_LETHAL
 *
 * Sensor config: gpm16 accel (up to ~16g, no clipping on real falls), dps2000 gyro.
 *
 * Note: ICM20948Sensor (screen wake-on-motion) uses the same singleton.
 * FallDetectionModule takes ownership via sleep(false) + scale reconfigure.
 * Disable wake-on-tap in device config to avoid conflict.
 */

#include "FallDetectionModule.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<ICM_20948.h>)

#include "BuzzerManager.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"

FallDetectionModule *fallDetectionModule;

constexpr bool  FallDetectionModule::SOS_LUT[];
constexpr float FallDetectionModule::NORM_MEAN[];
constexpr float FallDetectionModule::NORM_STD[];

static uint8_t s_tensorArena[FallDetectionModule::TENSOR_ARENA_SIZE];

// ── Constructor ────────────────────────────────────────────────────────────

FallDetectionModule::FallDetectionModule()
    : SinglePortModule("FallDetection", meshtastic_PortNum_PRIVATE_APP),
      OSThread("FallDetection"),
      currentState(MONITORING),
      sensor(nullptr),
      sensorInitialized(false),
      freefallStartTime(0),
      impactTime(0),
      impactWindowStart(0),
      spikeStartTime(0),
      inactivityStartTime(0),
      prealarmStartTime(0),
      lastAlarmBeepTime(0),
      sosPatternStartTime(0),
      lastSOSTime(0),
      isBeepOn(false),
      sosBuzzerOn(false),
      preFallAccelEMA(1.0f),
      lastNormalAccelG(1.0f),
      freefallMaxGyro(0.0f),
      freefallMinAccel(0.0f),
      peakImpactG(0.0f),
      gyroAtPeakG(0.0f),
      peakJerk(0.0f),
      prevAccelG(1.0f),
      impactPhaseDuration(0),
      impactSpikeCount(0),
      inImpactSpike(false),
      impactWindowActive(false),
      fallCategories(FALL_NONE),
      severity(SEVERITY_NON_LETHAL),
      responseTimeMs(0),
      bufferHead(0),
      bufferFull(false),
      mlInitialized(false),
      interpreter(nullptr),
      inputTensor(nullptr)
{
    memset(imuBuffer, 0, sizeof(imuBuffer));
    LOG_INFO("FallDetection: Module created, will init ICM-20948 in runOnce()");
}

// ── ML Stage 1 ─────────────────────────────────────────────────────────────

bool FallDetectionModule::initML()
{
    static bool already = false;
    if (already) return interpreter != nullptr;
    already = true;

    static tflite::MicroMutableOpResolver<8> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddMean();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();

    static tflite::MicroInterpreter si(
        tflite::GetModel(g_fall_model_data), resolver,
        s_tensorArena, TENSOR_ARENA_SIZE);
    interpreter = &si;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        LOG_ERROR("FallDetection: ML AllocateTensors failed — inference disabled");
        interpreter = nullptr;
        return false;
    }

    inputTensor = interpreter->input(0);
    LOG_INFO("FallDetection: ML ready, arena=%u bytes", interpreter->arena_used_bytes());
    return true;
}

void FallDetectionModule::fillBuffer(float ax, float ay, float az,
                                     float gx, float gy, float gz)
{
    imuBuffer[bufferHead][0] = ax;
    imuBuffer[bufferHead][1] = ay;
    imuBuffer[bufferHead][2] = az;
    imuBuffer[bufferHead][3] = gx;
    imuBuffer[bufferHead][4] = gy;
    imuBuffer[bufferHead][5] = gz;
    bufferHead = (bufferHead + 1) % IMU_BUFFER_LEN;
    if (!bufferFull && bufferHead == 0)
        bufferFull = true;
}

bool FallDetectionModule::runMLInference()
{
    if (!mlInitialized || interpreter == nullptr)
        return true;  // no ML — let state machine decide

    if (!bufferFull) {
        LOG_DEBUG("FallDetection: ML buffer not full yet, skipping");
        return true;
    }

    float   in_scale = inputTensor->params.scale;
    int32_t in_zero  = inputTensor->params.zero_point;
    int8_t *in_data  = inputTensor->data.int8;

    for (int i = 0; i < IMU_BUFFER_LEN; i++) {
        int src = (bufferHead + i) % IMU_BUFFER_LEN;  // oldest → newest
        for (int ch = 0; ch < 6; ch++) {
            float   norm = (imuBuffer[src][ch] - NORM_MEAN[ch]) / NORM_STD[ch];
            int32_t q    = (int32_t)roundf(norm / in_scale) + in_zero;
            if (q >  127) q =  127;
            if (q < -128) q = -128;
            in_data[i * 6 + ch] = (int8_t)q;
        }
    }

    if (interpreter->Invoke() != kTfLiteOk) {
        LOG_ERROR("FallDetection: ML invoke failed");
        return true;
    }

    TfLiteTensor *out      = interpreter->output(0);
    float         out_sc   = out->params.scale;
    int32_t       out_zero = out->params.zero_point;
    float not_fall = (out->data.int8[0] - out_zero) * out_sc;
    float fall     = (out->data.int8[1] - out_zero) * out_sc;

    LOG_INFO("FallDetection: ML fall=%.3f not_fall=%.3f threshold=%.2f -> %s",
             fall, not_fall, ML_FALL_THRESHOLD, fall >= ML_FALL_THRESHOLD ? "FALL" : "not_fall");

    return fall >= ML_FALL_THRESHOLD;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void FallDetectionModule::transitionToState(FallState newState)
{
    if (currentState == newState) return;
    LOG_DEBUG("FallDetection: State %d -> %d", currentState, newState);
    currentState = newState;
}

bool FallDetectionModule::readSensor(float &totalAccel_g, float &totalGyro_rads)
{
    if (!sensor->dataReady())
        return false;

    sensor->getAGMT();

    float ax = sensor->agmt.acc.axes.x / ACCEL_LSB_PER_G;
    float ay = sensor->agmt.acc.axes.y / ACCEL_LSB_PER_G;
    float az = sensor->agmt.acc.axes.z / ACCEL_LSB_PER_G;
    totalAccel_g = sqrtf(ax * ax + ay * ay + az * az);

    float gx = sensor->agmt.gyr.axes.x / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
    float gy = sensor->agmt.gyr.axes.y / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
    float gz = sensor->agmt.gyr.axes.z / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
    totalGyro_rads = sqrtf(gx * gx + gy * gy + gz * gz);

    fillBuffer(ax, ay, az, gx, gy, gz);
    return true;
}

bool FallDetectionModule::checkInactivity(float totalAccel_g, float totalGyro_rads)
{
    return (fabsf(totalAccel_g - 1.0f) < 0.2f) && (totalGyro_rads < GYRO_STILLNESS_THRESHOLD);
}

void FallDetectionModule::resetFallData()
{
    // preFallAccelEMA and lastNormalAccelG are intentionally NOT reset here —
    // they are accumulated during MONITORING and read at freefall entry.
    freefallMaxGyro      = 0.0f;
    freefallMinAccel     = FREEFALL_THRESHOLD;  // will be driven down during freefall phase
    peakImpactG          = 0.0f;
    gyroAtPeakG          = 0.0f;
    peakJerk             = 0.0f;
    prevAccelG           = 1.0f;
    impactPhaseDuration  = 0;
    impactSpikeCount     = 0;
    inImpactSpike        = false;
    impactWindowActive   = false;
    impactWindowStart    = 0;
    spikeStartTime       = 0;
    fallCategories       = FALL_NONE;
    severity             = SEVERITY_NON_LETHAL;
    responseTimeMs       = 0;
}

void FallDetectionModule::computeFallCategories(unsigned long freefallDurationMs)
{
    fallCategories = FALL_NONE;

    // Freefall duration
    if (freefallDurationMs < FREEFALL_TRIP_MAX_MS)
        fallCategories |= FALL_TRIP;
    if (freefallDurationMs >= FREEFALL_ELEVATED_MIN_MS)
        fallCategories |= FALL_ELEVATED;

    // Impact characteristics
    if (peakImpactG >= IMPACT_G_HIGH)
        fallCategories |= FALL_HIGH_IMPACT;
    if (peakJerk >= JERK_HARD_SURFACE_GS)
        fallCategories |= FALL_HARD_SURFACE;
    if (impactPhaseDuration >= IMPACT_PROLONGED_MS)
        fallCategories |= FALL_PROLONGED_IMPACT;
    if (impactSpikeCount >= 2)
        fallCategories |= FALL_MULTI_IMPACT;
    if (gyroAtPeakG >= ROT_AT_IMPACT_THRESHOLD)
        fallCategories |= FALL_HIGH_ROT_IMPACT;

    // Freefall phase
    if (freefallMaxGyro >= TUMBLE_GYRO_THRESHOLD)
        fallCategories |= FALL_TUMBLE;

    // Pre-fall state
    if (fabsf(preFallAccelEMA - 1.0f) < STATIONARY_EMA_BAND)
        fallCategories |= FALL_FROM_STATIONARY;
    if (lastNormalAccelG >= SUDDEN_ONSET_MIN_G)
        fallCategories |= FALL_SUDDEN_ONSET;

    // Repeated event
    if (lastSOSTime > 0 && (millis() - lastSOSTime) < REPEAT_FALL_WINDOW_MS)
        fallCategories |= FALL_REPEATED;

    LOG_INFO("FallDetection: Categories=0x%04X dur=%lums peak=%.2fg "
             "jerk=%.1fg/s gyroFF=%.2f spikes=%d",
             fallCategories, freefallDurationMs, peakImpactG,
             peakJerk, freefallMaxGyro, impactSpikeCount);
}

int FallDetectionModule::computeLethalScore() const
{
    int score = 0;

    if (fallCategories & FALL_ELEVATED)         score += 30;
    if (fallCategories & FALL_HIGH_IMPACT)      score += 25 + (int)((peakImpactG - IMPACT_G_HIGH) * 3.0f);
    if (fallCategories & FALL_TUMBLE)           score += 15;
    if (fallCategories & FALL_MULTI_IMPACT)     score += 20;
    if (fallCategories & FALL_HARD_SURFACE)     score += 10;
    if (fallCategories & FALL_PROLONGED_IMPACT) score += 15;
    if (fallCategories & FALL_HIGH_ROT_IMPACT)  score += 10;
    if (fallCategories & FALL_REPEATED)         score += 25;
    if (fallCategories & FALL_SUDDEN_ONSET)     score += 5;

    // Syncope signature: stationary + sudden onset together
    if ((fallCategories & (FALL_FROM_STATIONARY | FALL_SUDDEN_ONSET)) ==
                          (FALL_FROM_STATIONARY | FALL_SUDDEN_ONSET))
        score += 20;

    return score;
}

const char *FallDetectionModule::severityString() const
{
    return (severity == SEVERITY_LETHAL) ? "LETHAL" : "NON_LETHAL";
}

// ── Alarm / SOS ────────────────────────────────────────────────────────────

void FallDetectionModule::cancelFallAlarm()
{
    if (currentState == PRE_ALARM) {
        LOG_INFO("FallDetection: Alarm cancelled by user");
        deactivatePreAlarm();
        transitionToState(MONITORING);
    }
}

void FallDetectionModule::cancelAutoSOS()
{
    if (currentState == SOS_TRIGGERED) {
        LOG_INFO("FallDetection: Auto-SOS cancelled by user");
#ifdef PIN_BUZZER
        BuzzerManager::instance().write(0);
        BuzzerManager::instance().release(OWNER_FALL);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
#ifdef LED_PIN
        digitalWrite(LED_PIN, LOW);
#endif
        sosBuzzerOn = false;
        transitionToState(MONITORING);
    }
}

void FallDetectionModule::activatePreAlarm()
{
    LOG_WARN("FallDetection: PRE-ALARM — 30s countdown, severity=%s cats=0x%04X",
             severityString(), fallCategories);
#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(OWNER_FALL);
    BuzzerManager::instance().write(128);
#endif
#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
    isBeepOn = true;
}

void FallDetectionModule::deactivatePreAlarm()
{
#ifdef PIN_BUZZER
    BuzzerManager::instance().write(0);
    BuzzerManager::instance().release(OWNER_FALL);
#endif
#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, LOW);
#endif
    isBeepOn = false;
}

void FallDetectionModule::updateAlarmFeedback()
{
    unsigned long elapsed = millis() - lastAlarmBeepTime;
    if (elapsed >= ALARM_BEEP_INTERVAL) {
        lastAlarmBeepTime = millis();
        isBeepOn = true;
#ifdef PIN_BUZZER
        BuzzerManager::instance().write(128);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, HIGH);
#endif
    } else if (isBeepOn && elapsed >= 200) {
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
    lastSOSTime = millis();
    LOG_CRIT("FallDetection: AUTO-SOS! severity=%s cats=0x%04X peak=%.2fg resp=%lums",
             severityString(), fallCategories, peakImpactG, responseTimeMs);

    meshtastic_Position pos = meshtastic_Position_init_default;
    meshtastic_NodeInfoLite *node = nodeDB->getNodeNum()
        ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;

    bool  hasPosition = false;
    float latitude = 0.0f, longitude = 0.0f;

    if (node && nodeDB->hasValidPosition(node)) {
        pos.latitude_i  = node->position.latitude_i;
        pos.longitude_i = node->position.longitude_i;
        pos.altitude    = node->position.altitude;
        pos.time        = node->position.time;
        latitude        = pos.latitude_i * 1e-7f;
        longitude       = pos.longitude_i * 1e-7f;
        hasPosition     = true;
    }

    // Position packet
    meshtastic_MeshPacket *posPacket = allocDataPacket();
    posPacket->channel              = 0;
    posPacket->priority             = meshtastic_MeshPacket_Priority_MAX;
    posPacket->want_ack             = false;
    posPacket->decoded.portnum      = meshtastic_PortNum_POSITION_APP;
    posPacket->decoded.payload.size = pb_encode_to_bytes(
        posPacket->decoded.payload.bytes, sizeof(posPacket->decoded.payload.bytes),
        &meshtastic_Position_msg, &pos);
    if (service) service->sendToMesh(posPacket);

    // SOS text: "SOS [LETHAL|0x0246] - [lat],[lon]"
    // Category hex lets responders/logs reconstruct exactly what triggered.
    meshtastic_MeshPacket *textPacket = allocDataPacket();
    textPacket->channel         = 0;
    textPacket->priority        = meshtastic_MeshPacket_Priority_MAX;
    textPacket->want_ack        = false;
    textPacket->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;

    char message[120];
    size_t msgLen = hasPosition
        ? snprintf(message, sizeof(message), "SOS [%s|0x%04X] - [%.6f],[%.6f]",
                   severityString(), fallCategories, latitude, longitude)
        : snprintf(message, sizeof(message), "SOS [%s|0x%04X] - [No GPS]",
                   severityString(), fallCategories);
    if (msgLen >= sizeof(message)) msgLen = sizeof(message) - 1;

    memcpy(textPacket->decoded.payload.bytes, message, msgLen);
    textPacket->decoded.payload.size = msgLen;
    if (service) service->sendToMesh(textPacket);

    LOG_INFO("FallDetection: Sent: %s", message);

    sosPatternStartTime = millis();
    sosBuzzerOn = false;
#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(OWNER_FALL);
#endif
}

void FallDetectionModule::updateSOSBuzzerPattern()
{
#ifdef PIN_BUZZER
    uint32_t elapsed  = millis() - sosPatternStartTime;
    uint8_t  index    = (elapsed / 200) % SOS_LUT_LEN;
    bool     shouldOn = SOS_LUT[index];
    if (shouldOn != sosBuzzerOn) {
        BuzzerManager::instance().write(shouldOn ? 128 : 0);
        sosBuzzerOn = shouldOn;
    }
#endif
}

// ── Main loop ──────────────────────────────────────────────────────────────

int32_t FallDetectionModule::runOnce()
{
    if (!sensorInitialized) {
        sensor = ICM20948Singleton::GetInstance();

        if (sensor->status != ICM_20948_Stat_Ok) {
            Wire.begin(I2C_SDA, I2C_SCL);
            extern ScanI2C::DeviceAddress accelerometer_found;
            ScanI2C::FoundDevice found(ScanI2C::DeviceType::ICM20948, accelerometer_found);
            if (!sensor->init(found)) {
                LOG_WARN("FallDetection: ICM-20948 not ready, retry in 5s");
                return 5000;
            }
        }

        sensor->sleep(false);
        sensor->setFullScaleRangeAccel(gpm16);
        sensor->setFullScaleRangeGyro(dps2000);
        sensorInitialized = true;
        mlInitialized     = initML();

#ifdef PIN_VIBRATOR
        pinMode(PIN_VIBRATOR, OUTPUT);
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
        LOG_INFO("FallDetection: ICM-20948 ready at gpm16/dps2000  ML=%s",
                 mlInitialized ? "on" : "off");
    }

    // SOS state: skip I2C, just drive outputs
    if (currentState == SOS_TRIGGERED) {
        updateSOSBuzzerPattern();
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, sosBuzzerOn ? HIGH : LOW);
#endif
#ifdef LED_PIN
        digitalWrite(LED_PIN, (millis() / 250) % 2);
#endif
        return 200;
    }

    float totalAccel_g, totalGyro_rads;
    if (!readSensor(totalAccel_g, totalGyro_rads))
        return 50;

    unsigned long now = millis();

    switch (currentState) {

    case MONITORING:
        // Build EMA of accel to detect stationary-before-fall
        preFallAccelEMA  = (1.0f - EMA_ALPHA) * preFallAccelEMA + EMA_ALPHA * totalAccel_g;
        lastNormalAccelG = totalAccel_g;

        if (totalAccel_g < FREEFALL_THRESHOLD) {
            resetFallData();
            freefallStartTime = now;
            transitionToState(FREEFALL_DETECTED);
            LOG_DEBUG("FallDetection: Freefall start %.2fg (EMA=%.2fg last=%.2fg)",
                      totalAccel_g, preFallAccelEMA, lastNormalAccelG);
        }
        break;

    case FREEFALL_DETECTED:
        if (totalGyro_rads > freefallMaxGyro)
            freefallMaxGyro = totalGyro_rads;
        if (totalAccel_g < freefallMinAccel)
            freefallMinAccel = totalAccel_g;

        if (totalAccel_g >= FREEFALL_THRESHOLD) {
            unsigned long dur = now - freefallStartTime;
            if (dur >= FREEFALL_MIN_DURATION) {
                if (freefallMinAccel > FREEFALL_DEPTH_MIN) {
                    // Shallow dip — too high to be real freefall (bump / step / sensor noise)
                    LOG_DEBUG("FallDetection: Shallow freefall (min=%.2fg > %.2fg) — ignored",
                              freefallMinAccel, FREEFALL_DEPTH_MIN);
                    transitionToState(MONITORING);
                } else {
                    impactTime = now;
                    prevAccelG = totalAccel_g;
                    transitionToState(IMPACT_DETECTED);
                    LOG_INFO("FallDetection: Freefall %lums minAccel=%.2fg maxGyro=%.2frad/s",
                             dur, freefallMinAccel, freefallMaxGyro);
                }
            } else {
                transitionToState(MONITORING);
            }
        }
        break;

    case IMPACT_DETECTED: {
        // Jerk: rate of accel change between poll ticks (g/s)
        float jerk = fabsf(totalAccel_g - prevAccelG) / POLL_INTERVAL_S;
        if (jerk > peakJerk) peakJerk = jerk;
        prevAccelG = totalAccel_g;

        // Peak G and gyro at that moment
        if (totalAccel_g > peakImpactG) {
            peakImpactG = totalAccel_g;
            gyroAtPeakG = totalGyro_rads;
        }

        // Spike counting and phase duration
        // impactSpikeCount is only incremented when a spike ENDS and lasted ≥ MIN_IMPACT_SPIKE_MS,
        // filtering single-sample sensor glitches and vibration transients.
        if (totalAccel_g > IMPACT_THRESHOLD) {
            if (!inImpactSpike) {
                inImpactSpike = true;
                spikeStartTime = now;
                if (!impactWindowActive) {
                    impactWindowActive = true;
                    impactWindowStart  = now;
                }
            }
        } else if (inImpactSpike) {
            unsigned long spikeDur = now - spikeStartTime;
            inImpactSpike          = false;
            impactPhaseDuration   += spikeDur;
            if (spikeDur >= MIN_IMPACT_SPIKE_MS)
                impactSpikeCount++;
        }

        bool windowDone = impactWindowActive && (now - impactWindowStart) >= IMPACT_WINDOW_MS;
        bool timedOut   = (now - impactTime) >= IMPACT_TIMEOUT_MS;

        if (windowDone) {
            // Close any open spike
            if (inImpactSpike) {
                unsigned long spikeDur = now - spikeStartTime;
                impactPhaseDuration   += spikeDur;
                if (spikeDur >= MIN_IMPACT_SPIKE_MS)
                    impactSpikeCount++;
                inImpactSpike = false;
            }

            // ML Stage 1 confirmation — reject noise-triggered events
            if (!runMLInference()) {
                LOG_INFO("FallDetection: ML rejected event — back to MONITORING");
                transitionToState(MONITORING);
                break;
            }

            unsigned long freefallDuration = impactTime - freefallStartTime;
            computeFallCategories(freefallDuration);
            int score = computeLethalScore();
            severity  = (score >= LETHAL_SCORE_THRESHOLD) ? SEVERITY_LETHAL : SEVERITY_NON_LETHAL;
            LOG_INFO("FallDetection: Lethal score=%d -> %s", score, severityString());
            inactivityStartTime = now;
            responseTimeMs      = 0;
            transitionToState(INACTIVITY_DETECTED);
        } else if (timedOut) {
            LOG_DEBUG("FallDetection: Impact timeout, no spike — back to MONITORING");
            transitionToState(MONITORING);
        }
        break;
    }

    case INACTIVITY_DETECTED:
        if (!checkInactivity(totalAccel_g, totalGyro_rads)) {
            responseTimeMs = now - inactivityStartTime;

            // Quick response can de-escalate a borderline lethal score
            if (responseTimeMs < RESPONSE_FAST_MS && severity == SEVERITY_LETHAL) {
                int score = computeLethalScore();
                if (score < LETHAL_SCORE_THRESHOLD + 10)
                    severity = SEVERITY_NON_LETHAL;
            }

            LOG_INFO("FallDetection: Movement at %lums -> %s cats=0x%04X",
                     responseTimeMs, severityString(), fallCategories);
            transitionToState(MONITORING);

        } else if ((now - inactivityStartTime) >= INACTIVITY_DURATION) {
            responseTimeMs    = INACTIVITY_DURATION;
            prealarmStartTime = now;
            lastAlarmBeepTime = now;
            transitionToState(PRE_ALARM);
            activatePreAlarm();
        }
        break;

    case PRE_ALARM:
        updateAlarmFeedback();

        // Force lethal if still not moving 15s after the fall
        if ((now - inactivityStartTime) >= RESPONSE_LETHAL_MS)
            severity = SEVERITY_LETHAL;

        if ((now - prealarmStartTime) >= PREALARM_TIMEOUT) {
            deactivatePreAlarm();
            transitionToState(SOS_TRIGGERED);
            triggerAutoSOS();
        }
        break;

    default:
        break;
    }

    return 20;  // 50 Hz — matches ML training sample rate
}

#endif // ICM-20948 available
