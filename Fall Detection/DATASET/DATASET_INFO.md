# Fall Detection Dataset - Processed for MagicWand Training

## Overview
This dataset is derived from the SisFall public dataset and has been preprocessed and organized into 5 fall detection classes suitable for TinyML training on ESP32 with MPU6050.

## Dataset Statistics

| Class | Files | Description |
|-------|-------|-------------|
| **not_falling** | 2,397 | Normal daily activities |
| **falling_light** | 240 | Stumbles and trips |
| **falling_regular** | 1,078 | Standard falls |
| **falling_extreme** | 600 | High-impact falls |
| **falling_misc** | 190 | Edge cases |
| **TOTAL** | **4,505** | |

## Class Definitions

### 1. not_falling (2,397 samples)
Normal activities of daily living (ADLs):
- Walking (slowly, quickly)
- Jogging (slowly, quickly)
- Walking stairs (up/down, slow/fast)
- Sitting in chairs (various heights and speeds)
- Lying down and getting up
- Bending at knees
- Bending without bending knees
- Getting in/out of car
- Gentle jumping

**Activity Codes:** D01, D02, D03, D04, D05, D06, D07, D08, D09, D10, D12, D13, D14, D15, D16, D17, D19

### 2. falling_light (240 samples)
Minor falls and near-fall events:
- Stumbling while walking
- Tripping while walking (forward fall caused by trip)

**Activity Codes:** D18, F04

### 3. falling_regular (1,078 samples)
Standard fall scenarios:
- Falls caused by slips (forward, backward, lateral)
- Falls when trying to get up (forward, backward, lateral)
- Falls while sitting (forward, backward, lateral)

**Activity Codes:** F01, F02, F03, F05, F06, F07, F08, F09, F10

### 4. falling_extreme (600 samples)
High-impact and severe falls:
- Falls while walking downstairs
- Falls while walking upstairs
- Falls with attempted dampening (using hands, using knees)

**Activity Codes:** F11, F12, F13, F14, F15

### 5. falling_misc (190 samples)
Edge cases and unusual scenarios:
- Attempting to get up but collapsing back into chair
- Recovery attempts that fail

**Activity Codes:** D11

## Data Format

Each file contains sensor readings in MagicWand-compatible format:
```
-.-.-
ax,ay,az,gx,gy,gz
ax,ay,az,gx,gy,gz
...

```

Where:
- `ax, ay, az` = Accelerometer readings (x, y, z axes) from ADXL345
- `gx, gy, gz` = Gyroscope readings (x, y, z axes) from ITG3200
- First line: `-.-.-` (separator marker)
- Last line: blank line

## File Naming Convention
```
output_<class_name>_<activity_code>_<subject_id>_<trial_number>.txt
```

Example: `output_falling_light_D18_SA01_R01.txt`
- Class: falling_light
- Activity: D18 (stumble)
- Subject: SA01
- Trial: R01 (first repetition)

## Source Dataset
**SisFall: A Fall and Movement Dataset**
- Created by: A. Sucerquia, J.D. López, J.F. Vargas-Bonilla
- Institution: SISTEMIC, Faculty of Engineering, Universidad de Antioquía UDEA
- Version: 1.0 (February 2016)
- GitHub: https://github.com/Fall-Prevention-Team/sisfallData
- Paper: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5298771/

## Participants
- 23 young adults (SA01-SA23)
- 15 elderly participants (SE01-SE15)
- Age range: 19-75 years
- Mix of male and female participants

## Sensor Information
Original SisFall device contained:
- ADXL345 accelerometer (±16g)
- ITG3200 gyroscope (±2000°/s)
- MMA8451Q accelerometer (not used in this preprocessing)

For MPU6050 compatibility, we use:
- ADXL345 accelerometer data → MPU6050 accelerometer
- ITG3200 gyroscope data → MPU6050 gyroscope

## Next Steps for Training

1. **Copy to MagicWand structure:**
   ```
   cp -r DATASET/processed/* MagicWand-TFLite-ESP32-MPU6050/data/
   ```

2. **Update training script** to recognize the 5 new classes

3. **Train the model** using the provided training scripts

4. **Deploy to ESP32** with the trained model

## Notes
- Dataset is balanced towards "not_falling" class (normal behavior)
- Consider data augmentation for minority classes (falling_light, falling_misc)
- Sampling rate: ~200Hz (varies slightly per activity)
- Each activity duration: 12-100 seconds depending on type
