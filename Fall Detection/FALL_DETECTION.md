# TrekLink Fall Detection

Two-stage system: ML detects the fall, firmware classifies severity and fires SOS.

---

## Hardware

| Part | Detail |
|---|---|
| MCU | ESP32 (TrekLink board, 4 MB flash, 320 KB RAM) |
| IMU | ICM-20948 at I2C 0x68, configured ±16 g / ±2000 dps |
| Radio | Meshtastic mesh — SOS packet sent over the mesh |

---

## Architecture

```
Continuous 50 Hz poll
        │
        ▼
┌───────────────────────┐
│  State machine         │  MONITORING → FREEFALL_DETECTED → IMPACT_DETECTED
│  (physics pre-filter)  │
└──────────┬────────────┘
           │  impact window closes (1 s after first spike)
           ▼
┌───────────────────────┐
│  Stage 1 — ML model   │  CNN1D, INT8, 128 × 6 axes window
│  fall / not_fall       │  fall=96.1%  not_fall=50.3% recall on test set
└──────────┬────────────┘
           │  ML says "fall"
           ▼
┌───────────────────────┐
│  Stage 2 — firmware   │  bitmask 11 flags + lethal meter score
│  severity classifier  │  score ≥ 50 → LETHAL, else NON_LETHAL
└──────────┬────────────┘
           │  10 s inactivity → PRE_ALARM (30 s countdown)
           ▼
      AUTO-SOS mesh packet
  "SOS [LETHAL|0x0246] - [lat],[lon]"
```

The physics pre-filter (freefall threshold + impact spike) means the ML model only ever sees windows that already look like a fall. The 50% not_fall recall from ML is acceptable because most normal activity is filtered out before ML runs.

---

## Stage 1 — ML Model

### Files

| File | Purpose |
|---|---|
| `Fall Detection/MagicWand-TFLite-ESP32-MPU6050/train/train_fall_detection.py` | Training entry point |
| `Fall Detection/MagicWand-TFLite-ESP32-MPU6050/train/data_load.py` | Dataset loader + Z-score normalizer |
| `Fall Detection/MagicWand-TFLite-ESP32-MPU6050/train/data_augmentation.py` | Augmentation (doubles fall class) |
| `Fall Detection/MagicWand-TFLite-ESP32-MPU6050/train/data_prepare_fall_detection.py` | Converts preprocessed dataset windows into training JSONL |
| `Fall Detection/DATASET/preprocess_sisfall.py` | SisFall 200 Hz → 50 Hz, 128-sample windows |
| `Fall Detection/DATASET/preprocess_fallalld.py` | FallAllD 238 Hz → ~50 Hz, 128-sample windows |
| `Fall Detection/DATASET/preprocess_kfall.py` | KFall 100 Hz → 50 Hz, 128-sample windows |
| `Fall Detection/DATASET/preprocess_umafall.py` | UMAFall → 50 Hz, 128-sample windows |
| `Fall Detection/Context/model_cnn1d_binary_best_recall_quantized.tflite` | Deployed model (INT8, 26 KB) |
| `Fall Detection/Context/norm_stats_binary.json` | Z-score mean/std — **must match firmware constants** |
| `src/modules/fall_detection_model.h` | Model baked as C array for firmware |

### Model details

- Architecture: `CNN1D` — Conv1D(16) → MaxPool → Conv1D(32) → MaxPool → Conv1D(64) → GAP → Dense(32) → Dense(2)
- Input: `(1, 128, 6)` INT8 — 128 timesteps × [ax, ay, az, gx, gy, gz]
- Units: acceleration in **g**, gyro in **rad/s**
- Normalisation: Z-score per axis using `norm_stats_binary.json`
- Output: `[not_fall_score, fall_score]` INT8 softmax
- Test set: 91 subjects, subject-based split (54/18/19 train/valid/test)

### Key training parameters

```python
# train_fall_detection.py
_SEED        = 42          # fixed seed — reproducible runs across Kaggle sessions
class_weight = {0: 1.0, 1: 2.5}   # not_falling / fall

# FallRecallCallback
MIN_OTHER_RECALL = 0.45   # not_falling floor — relax if fall recall too low
WARMUP_EPOCHS    = 10     # skip callback during chaotic early training
PATIENCE         = 25     # early stop if score stalls (was 15 — extra runway)

# ReduceLROnPlateau
factor=0.5, patience=8, min_lr=1e-6
```

### How to retrain

1. Run one of the `preprocess_*.py` scripts (or all of them) to regenerate `DATASET/processed/`
2. Run `data_prepare_fall_detection.py` to build `data/complete_data`
3. Upload `kaggle_training.zip` to Kaggle, nuke previous output, run:
   ```
   python train_fall_detection.py --model CNN1D --mode binary
   ```
4. At the end of training, the script prints either:
   - `*** DEPLOY READY ***` → download `DEPLOY_READY__recall0.XXX__model_cnn1d_binary_best_recall_quantized.tflite`
   - `*** TRAINING FAILED — DO NOT DEPLOY ***` → output has `BAD_MODEL__...tflite` instead; **do not download**, just rerun
5. If deploy-ready: download `DEPLOY_READY__recall0.XXX__...tflite` (rename to `model_cnn1d_binary_best_recall_quantized.tflite`) and `norm_stats_binary.json` into `Fall Detection/Context/`

   **Safety rule: never download a file that doesn't start with `DEPLOY_READY__`.**
6. Regenerate the firmware C array:
   ```
   python -c "
   import pathlib
   data = pathlib.Path('Fall Detection/Context/model_cnn1d_binary_best_recall_quantized.tflite').read_bytes()
   lines = ['#pragma once','#include <cstdint>',
            f'constexpr unsigned int g_fall_model_len = {len(data)};',
            'alignas(8) const unsigned char g_fall_model_data[] = {']
   row = []
   for b in data:
       row.append(f'0x{b:02x}')
       if len(row) == 16:
           lines.append('  ' + ', '.join(row) + ',')
           row = []
   if row: lines.append('  ' + ', '.join(row))
   lines.append('};')
   pathlib.Path('src/modules/fall_detection_model.h').write_text('\n'.join(lines) + '\n')
   print('Done')
   "
   ```
7. Update `NORM_MEAN` and `NORM_STD` in `FallDetectionModule.h` if the norm stats changed (rare — they come from fixed training data)

### Tuning fall recall vs false-alarm rate

| Parameter | File | Effect |
|---|---|---|
| `class_weight[1]` (fall weight) | `train_fall_detection.py` | Higher → more fall recall, more false alarms |
| `MIN_OTHER_RECALL` | `FallRecallCallback` | Lower → allows saving higher-fall-recall checkpoints |
| Current values | — | weight=2.5, floor=0.45 → fall=96%, not_fall=50% |

---

## Stage 2 — Firmware Severity

### State machine (`FallDetectionModule.cpp`)

```
MONITORING
  accel < 0.5 g for ≥ 80 ms
FREEFALL_DETECTED
  accel rises back ≥ 0.5 g
IMPACT_DETECTED
  collect 1 s of impact metrics
  → ML confirmation gate (runMLInference)
  ML rejects → MONITORING
  ML accepts →
INACTIVITY_DETECTED
  movement within INACTIVITY_DURATION (10 s)?
  yes → back to MONITORING
  no →
PRE_ALARM
  30 s buzzer countdown
  user can cancel
  no cancel →
SOS_TRIGGERED
  sends mesh packets, buzzes SOS pattern
```

### Fall category bitmask (`FallCategoryFlags`)

Multiple flags can be set on the same event. They feed the lethal meter.

| Flag | Hex | Condition |
|---|---|---|
| `FALL_TRIP` | 0x0001 | Freefall < 200 ms (stumble) |
| `FALL_ELEVATED` | 0x0002 | Freefall > 500 ms (airborne) |
| `FALL_HIGH_IMPACT` | 0x0004 | Peak G > 4.0 g |
| `FALL_TUMBLE` | 0x0008 | Gyro > 2.0 rad/s during freefall |
| `FALL_MULTI_IMPACT` | 0x0010 | ≥ 2 impact spikes (stairs/bounce) |
| `FALL_HARD_SURFACE` | 0x0020 | Jerk > 30 g/s at impact |
| `FALL_PROLONGED_IMPACT` | 0x0040 | Impact phase > 400 ms (rolling) |
| `FALL_HIGH_ROT_IMPACT` | 0x0080 | Gyro > 1.5 rad/s at peak G |
| `FALL_SUDDEN_ONSET` | 0x0100 | Accel > 1.2 g just before drop |
| `FALL_FROM_STATIONARY` | 0x0200 | EMA within ±0.15 g of 1 g (syncope risk) |
| `FALL_REPEATED` | 0x0400 | Another SOS within 5 min |

### Lethal meter scoring

```
FALL_ELEVATED           +30
FALL_HIGH_IMPACT        +25 + (peakG − 4.0) × 3   (scaled by impact force)
FALL_TUMBLE             +15
FALL_MULTI_IMPACT       +20
FALL_PROLONGED_IMPACT   +15
FALL_HARD_SURFACE       +10
FALL_HIGH_ROT_IMPACT    +10
FALL_REPEATED           +25
FALL_SUDDEN_ONSET       +5
FALL_FROM_STATIONARY
  + FALL_SUDDEN_ONSET   +20   (syncope combo)

score ≥ 50  →  LETHAL
score < 50  →  NON_LETHAL
```

Post-fall override: if the person is still not moving 15 s after the fall, severity is forced to `LETHAL` regardless of score.

Quick response override: if movement is detected in < 3 s and score is between 50–59, severity is de-escalated to `NON_LETHAL`.

### Thresholds that need lab calibration

All of these are in `FallDetectionModule.h`. Calibrate via drop-test with TrekLink mounted in its actual worn position.

| Constant | Current value | What to test |
|---|---|---|
| `FREEFALL_THRESHOLD` | 0.5 g | Should stay below this during clean freefall |
| `IMPACT_THRESHOLD` | 3.0 g | Minimum spike to count as impact |
| `IMPACT_G_HIGH` | 4.0 g | Threshold for FALL_HIGH_IMPACT flag |
| `FREEFALL_TRIP_MAX_MS` | 200 ms | Max duration for a stumble vs real fall |
| `FREEFALL_ELEVATED_MIN_MS` | 500 ms | Min duration to count as elevated/airborne |
| `JERK_HARD_SURFACE_GS` | 30.0 g/s | Rate of accel change for hard surface flag |
| `TUMBLE_GYRO_THRESHOLD` | 2.0 rad/s | Gyro during freefall to flag tumble |
| `ROT_AT_IMPACT_THRESHOLD` | 1.5 rad/s | Gyro at peak G for high-rotation flag |
| `LETHAL_SCORE_THRESHOLD` | 50 | Score cutoff for LETHAL verdict |
| `INACTIVITY_DURATION` | 10 000 ms | How long still before PRE_ALARM fires |
| `PREALARM_TIMEOUT` | 30 000 ms | How long before auto-SOS if no cancel |

### SOS message format

```
SOS [LETHAL|0x0246] - [10.762622],[106.660172]
SOS [NON_LETHAL|0x0005] - [No GPS]
```

The hex is the `fallCategories` bitmask — responders/logs can decode exactly which flags triggered.

---

## Firmware build

```bash
pio run -e treklink
pio run -e treklink -t upload
```

The `espressif/esp-tflite-micro` library is fetched automatically by PlatformIO on first build. Build time is longer the first time.

**If you get a linker OOM error:** reduce `TENSOR_ARENA_SIZE` in `FallDetectionModule.h` (currently 24 KB). Check the serial log line `ML ready, arena=XXXX bytes` after boot — the arena can be shrunk to that reported value plus ~2 KB headroom.

---

## Key source files

| File | What it does |
|---|---|
| `src/modules/FallDetectionModule.h` | All constants, thresholds, class declaration |
| `src/modules/FallDetectionModule.cpp` | State machine, ML gate, SOS sender |
| `src/modules/fall_detection_model.h` | Embedded TFLite model (auto-generated, do not edit by hand) |
| `variants/esp32/treklink/platformio.ini` | Build env — lib deps including TFLite Micro |
