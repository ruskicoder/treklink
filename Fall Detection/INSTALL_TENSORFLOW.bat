@echo off
echo Installing TensorFlow for Fall Detection Training
echo.

echo Checking Python version...
python --version
echo.

echo === Option 1: Install with current Python ===
echo This will try to install TensorFlow with your current Python
echo Press Ctrl+C to cancel, or
pause

python -m pip install --upgrade pip
python -m pip install tensorflow numpy keras

echo.
echo === Testing TensorFlow ===
python -c "import tensorflow as tf; print('TensorFlow version:', tf.__version__); print('SUCCESS!')"

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo TensorFlow installed successfully!
    echo You can now run training:
    echo   cd MagicWand-TFLite-ESP32-MPU6050\train
    echo   python train_fall_detection.py --model CNN
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Installation failed!
    echo.
    echo Your Python version might be too new.
    echo Please install Python 3.11 from:
    echo https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe
    echo ========================================
)

echo.
pause
