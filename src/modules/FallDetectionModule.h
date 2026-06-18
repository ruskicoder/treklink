#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<ICM_20948.h>)

#include "motion/ICM20948Sensor.h"
#include "detect/ScanI2C.h"
#include <Wire.h>
#include "fall_detection_model.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#ifndef TREKLINK_MSG_FALL
#define TREKLINK_MSG_FALL 0x02
#endif

// ── Fall category flags ────────────────────────────────────────────────────
// Multiple flags can be set on a single fall event.
// Used for logging and as weighted inputs to the lethal meter.
// Calibrate all thresholds via lab drop-test for TrekLink mounting position.
enum FallCategoryFlags : uint16_t {
    FALL_NONE             = 0x0000,
    FALL_TRIP             = 0x0001,  // freefall < FREEFALL_TRIP_MAX_MS (stumble)
    FALL_ELEVATED         = 0x0002,  // freefall > FREEFALL_ELEVATED_MIN_MS (airborne)
    FALL_HIGH_IMPACT      = 0x0004,  // peak G > IMPACT_G_HIGH
    FALL_TUMBLE           = 0x0008,  // high gyro during freefall phase
    FALL_MULTI_IMPACT     = 0x0010,  // >= 2 impact spikes (stairs / bouncing)
    FALL_HARD_SURFACE     = 0x0020,  // high jerk at impact
    FALL_PROLONGED_IMPACT = 0x0040,  // impact phase > IMPACT_PROLONGED_MS (rolling)
    FALL_HIGH_ROT_IMPACT  = 0x0080,  // high gyro at the moment of peak G
    FALL_SUDDEN_ONSET     = 0x0100,  // abrupt freefall entry — no stumble warning
    FALL_FROM_STATIONARY  = 0x0200,  // was still before falling (syncope risk)
    FALL_REPEATED         = 0x0400,  // another SOS within REPEAT_FALL_WINDOW_MS
};

enum FallSeverity : uint8_t {
    SEVERITY_NON_LETHAL = 0,
    SEVERITY_LETHAL     = 1,
};

class FallDetectionModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    FallDetectionModule();

    void cancelFallAlarm();
    void cancelAutoSOS();
    bool isInPreAlarm()     const { return currentState == PRE_ALARM; }
    bool isInSOSTriggered() const { return currentState == SOS_TRIGGERED; }

  protected:
    virtual int32_t runOnce() override;

  private:
    enum FallState {
        MONITORING,
        FREEFALL_DETECTED,
        IMPACT_DETECTED,
        INACTIVITY_DETECTED,
        PRE_ALARM,
        SOS_TRIGGERED
    };

    FallState        currentState;
    ICM20948Singleton *sensor;
    bool             sensorInitialized;

    // ── State machine timestamps ───────────────────────────────────────────
    unsigned long freefallStartTime;
    unsigned long impactTime;
    unsigned long impactWindowStart;   // when first spike seen in IMPACT_DETECTED
    unsigned long spikeStartTime;      // start of the current impact spike
    unsigned long inactivityStartTime;
    unsigned long prealarmStartTime;
    unsigned long lastAlarmBeepTime;
    unsigned long sosPatternStartTime;
    unsigned long lastSOSTime;         // timestamp of last SOS — for FALL_REPEATED

    // ── Buzzer / vibrator state ────────────────────────────────────────────
    bool isBeepOn;
    bool sosBuzzerOn;

    // ── Pre-fall activity tracking ─────────────────────────────────────────
    float preFallAccelEMA;    // exponential moving average of accel in MONITORING
    float lastNormalAccelG;   // last sample before freefall threshold crossed

    // ── Freefall phase ─────────────────────────────────────────────────────
    float freefallMaxGyro;    // peak gyro magnitude seen during freefall
    float freefallMinAccel;   // minimum accel seen during freefall — must reach FREEFALL_DEPTH_MIN

    // ── Impact phase ──────────────────────────────────────────────────────
    float         peakImpactG;          // max accel magnitude in impact window
    float         gyroAtPeakG;          // gyro magnitude at the moment of peakImpactG
    float         peakJerk;             // max |Δaccel| / POLL_INTERVAL_S (g/s)
    float         prevAccelG;           // previous sample for jerk calculation
    unsigned long impactPhaseDuration;  // accumulated ms above IMPACT_THRESHOLD
    int           impactSpikeCount;     // how many times accel crossed threshold
    bool          inImpactSpike;        // currently above IMPACT_THRESHOLD
    bool          impactWindowActive;   // at least one spike has been seen

    // ── Severity results ───────────────────────────────────────────────────
    uint16_t     fallCategories;  // bitmask set by computeFallCategories()
    FallSeverity severity;        // final LETHAL / NON_LETHAL decision
    unsigned long responseTimeMs; // ms from inactivity start to first movement

    // ── ICM-20948 scale ────────────────────────────────────────────────────
    static constexpr float ACCEL_LSB_PER_G  = 2048.0f;
    static constexpr float GYRO_LSB_PER_DPS = 16.384f;
    static constexpr float DEG_TO_RAD_F     = 0.017453292519943f;
    static constexpr float POLL_INTERVAL_S  = 0.02f;  // 50 Hz — matches ML training rate

    // ── Detection thresholds ───────────────────────────────────────────────
    static constexpr float         FREEFALL_THRESHOLD       = 0.5f;   // g
    static constexpr float         IMPACT_THRESHOLD         = 3.5f;   // g — raised from 3.0; running footstrike peaks ~2.5–3g (was 3.0)
    static constexpr float         GYRO_STILLNESS_THRESHOLD = 0.1f;   // rad/s
    static constexpr unsigned long FREEFALL_MIN_DURATION    = 120;    // ms — real freefall ≥ 120ms; filters noise/bumps (was 80)
    static constexpr float         FREEFALL_DEPTH_MIN       = 0.35f;  // g — freefall must dip below this; real falls reach ~0.05–0.2g, bumps ~0.3–0.45g
    static constexpr unsigned long INACTIVITY_DURATION      = 10000;  // ms
    static constexpr unsigned long PREALARM_TIMEOUT         = 30000;  // ms
    static constexpr unsigned long ALARM_BEEP_INTERVAL      = 1000;   // ms

    // ── Fall characterization — calibrate via lab drop-test ────────────────
    static constexpr unsigned long FREEFALL_TRIP_MAX_MS     = 200;    // < this = FALL_TRIP
    static constexpr unsigned long FREEFALL_ELEVATED_MIN_MS = 500;    // > this = FALL_ELEVATED
    static constexpr unsigned long IMPACT_WINDOW_MS         = 1000;   // characterization window after first spike
    static constexpr unsigned long IMPACT_TIMEOUT_MS        = 1500;   // give up if no spike — real impact arrives within ~500ms (was 3000)
    static constexpr unsigned long IMPACT_PROLONGED_MS      = 400;    // ms above threshold = FALL_PROLONGED_IMPACT
    static constexpr unsigned long MIN_IMPACT_SPIKE_MS      = 40;     // spike must hold ≥ 40ms (~2 samples) to count — filters glitches
    static constexpr float         IMPACT_G_HIGH            = 4.0f;   // g — FALL_HIGH_IMPACT
    static constexpr float         TUMBLE_GYRO_THRESHOLD    = 2.0f;   // rad/s during freefall
    static constexpr float         ROT_AT_IMPACT_THRESHOLD  = 1.5f;   // rad/s at peak G
    static constexpr float         JERK_HARD_SURFACE_GS     = 30.0f;  // g/s — FALL_HARD_SURFACE
    static constexpr float         EMA_ALPHA                = 0.01f;  // adjusted for 50 Hz (same 2 s time constant)
    static constexpr float         STATIONARY_EMA_BAND      = 0.15f;  // within ±this of 1g = still
    static constexpr float         SUDDEN_ONSET_MIN_G       = 1.2f;   // accel before drop > this = sudden

    // ── Lethal meter ───────────────────────────────────────────────────────
    static constexpr int           LETHAL_SCORE_THRESHOLD   = 50;
    static constexpr unsigned long REPEAT_FALL_WINDOW_MS    = 300000; // 5 min

    // ── Post-fall response ─────────────────────────────────────────────────
    static constexpr unsigned long RESPONSE_FAST_MS         = 3000;   // < this = quick response
    static constexpr unsigned long RESPONSE_LETHAL_MS       = 15000;  // > this = force LETHAL

    // ── ML Stage 1 — inference ────────────────────────────────────────────
    static constexpr int   IMU_BUFFER_LEN    = 128;
    static constexpr int   TENSOR_ARENA_SIZE = 24 * 1024;
    static constexpr float ML_FALL_THRESHOLD = 0.54f;  // raised from 0.50 (argmax); see MODEL_RESULTS.md threshold table
    // Z-score params from norm_stats_binary.json — axes: ax ay az gx gy gz
    static constexpr float NORM_MEAN[6] = { 0.340f, -6.405f, -0.901f, -0.001f,  0.073f, -0.002f };
    static constexpr float NORM_STD[6]  = { 4.171f,  5.627f,  4.682f,  0.720f,  0.659f,  0.472f };

    float                     imuBuffer[IMU_BUFFER_LEN][6];
    int                       bufferHead;
    bool                      bufferFull;
    bool                      mlInitialized;
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor             *inputTensor;

    bool initML();
    void fillBuffer(float ax, float ay, float az, float gx, float gy, float gz);
    bool runMLInference();

    // ── SOS buzzer LUT ────────────────────────────────────────────────────
    static constexpr bool    SOS_LUT[28] = {
        1,0,1,0,1,0,
        1,1,1,0,1,1,1,0,1,1,1,0,
        1,0,1,0,1,0,
        0,0,0,0
    };
    static constexpr uint8_t SOS_LUT_LEN = 28;

    void         transitionToState(FallState newState);
    bool         readSensor(float &totalAccel_g, float &totalGyro_rads);
    bool         checkInactivity(float totalAccel_g, float totalGyro_rads);
    void         resetFallData();
    void         computeFallCategories(unsigned long freefallDurationMs);
    int          computeLethalScore() const;
    const char  *severityString() const;
    void         activatePreAlarm();
    void         deactivatePreAlarm();
    void         triggerAutoSOS();
    void         updateAlarmFeedback();
    void         updateSOSBuzzerPattern();
};

extern FallDetectionModule *fallDetectionModule;

#endif // ICM-20948 available
