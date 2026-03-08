@echo off
echo Checking for Python installations...
echo.

echo === Python 3.14 ===
python --version 2>nul
if %errorlevel% equ 0 (
    python --version
    where python
) else (
    echo Not found
)
echo.

echo === Python 3.11 ===
py -3.11 --version 2>nul
if %errorlevel% equ 0 (
    py -3.11 --version
    py -3.11 -c "import sys; print(sys.executable)"
) else (
    echo Not found
)
echo.

echo === All Python versions ===
py --list 2>nul
if %errorlevel% neq 0 (
    echo py launcher not available
)
echo.

echo === Checking common install locations ===
if exist "C:\Python311" (
    echo Found: C:\Python311
    "C:\Python311\python.exe" --version
)
if exist "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311" (
    echo Found: C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311
    "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311\python.exe" --version
)
echo.

echo === TensorFlow Check ===
python -c "import tensorflow as tf; print('TensorFlow version:', tf.__version__)" 2>nul
if %errorlevel% neq 0 (
    echo TensorFlow not installed
)
echo.

pause
