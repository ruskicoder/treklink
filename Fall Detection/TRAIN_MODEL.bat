@echo off
echo ========================================
echo Fall Detection Model Training
echo ========================================
echo.
echo This will train a CNN model on your fall detection dataset
echo Training will take approximately 30-60 minutes
echo.
echo Dataset: 4,500 samples across 5 classes
echo - not_falling: 2,392 samples
echo - falling_light: 240 samples
echo - falling_regular: 1,078 samples
echo - falling_extreme: 600 samples
echo - falling_misc: 190 samples
echo.
pause

cd MagicWand-TFLite-ESP32-MPU6050\train

echo.
echo Starting training...
echo.

python train_fall_detection.py --model CNN

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo Training completed successfully!
    echo ========================================
    echo.
    echo Your trained model files:
    echo   - model_fall_detection_quantized.tflite (USE THIS FOR ESP32)
    echo   - model_fall_detection.tflite
    echo.
    echo Next steps:
    echo 1. Check the confusion matrix above for accuracy
    echo 2. Convert model to C array for ESP32
    echo 3. Update Arduino code
    echo 4. Upload to ESP32
    echo.
    echo See FALL_DETECTION_TRAINING.md for deployment guide
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Training failed!
    echo Check the error messages above
    echo ========================================
)

echo.
pause
