# Fall Detection with ESP32 & MPU6050 - Quick Start

## What You Have

1. **MagicWand-TFLite-ESP32-MPU6050/** - Training framework and ESP32 code
2. **DATASET/** - Preprocessed SisFall dataset with 4,505 samples across 5 classes

## 5 Fall Detection Classes

- **not_falling** (2,397 samples) - Normal activities
- **falling_light** (240 samples) - Stumbles, trips
- **falling_regular** (1,078 samples) - Standard falls
- **falling_extreme** (600 samples) - High-impact falls
- **falling_misc** (190 samples) - Edge cases

## Quick Training Steps

```bash
# 1. Install dependencies
cd MagicWand-TFLite-ESP32-MPU6050/train
pip install -r requirements.txt

# 2. Prepare data
python data_prepare_fall_detection.py

# 3. Split data
python data_split.py

# 4. Train model (takes ~30-60 min)
python train_fall_detection.py --model CNN

# 5. Check outputs
ls *.tflite
```

## What Gets Created

- `model_fall_detection_quantized.tflite` - Your trained model (ready for ESP32)
- Confusion matrix showing accuracy per class
- Model size info (should be <25KB for ESP32)

## Deploy to ESP32

1. Wire MPU6050 to ESP32:
   - VCC → 3.3V
   - GND → GND
   - SCL → GPIO 22
   - SDA → GPIO 21

2. Convert model to C array and update Arduino code
3. Upload to ESP32
4. Test with real movements

## Full Documentation

- **DATASET/DATASET_INFO.md** - Dataset details
- **MagicWand-TFLite-ESP32-MPU6050/FALL_DETECTION_TRAINING.md** - Complete training guide
- **MagicWand-TFLite-ESP32-MPU6050/README.md** - Original MagicWand docs

## Need Help?

Check the training guide for:
- Troubleshooting
- Hyperparameter tuning
- Model evaluation
- Deployment steps
