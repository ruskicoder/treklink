# Install Python 3.11 for Fall Detection Training

## Quick Install Steps

### Option 1: Direct Download (Easiest)

1. **Download Python 3.11.9:**
   - Go to: https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe
   - Or visit: https://www.python.org/downloads/release/python-3119/

2. **Run the installer:**
   - ✅ Check "Add Python 3.11 to PATH"
   - ✅ Check "Install for all users" (optional)
   - Click "Install Now"

3. **Verify installation:**
   ```bash
   py -3.11 --version
   ```
   Should show: `Python 3.11.9`

### Option 2: Using Winget (Command Line)

Open PowerShell as Administrator and run:
```powershell
winget install Python.Python.3.11
```

### Option 3: Using Chocolatey

If you have Chocolatey installed:
```bash
choco install python311
```

## After Installing Python 3.11

### Create Virtual Environment

```bash
# Navigate to training folder
cd "D:\Code Stuffs\Fall Detection\MagicWand-TFLite-ESP32-MPU6050\train"

# Create virtual environment with Python 3.11
py -3.11 -m venv venv

# Activate it
venv\Scripts\activate

# Install TensorFlow and dependencies
pip install tensorflow numpy

# Verify TensorFlow works
python -c "import tensorflow as tf; print(tf.__version__)"
```

### Train Your Model

```bash
# Make sure virtual environment is activated (you'll see (venv) in prompt)
python train_fall_detection.py --model CNN
```

## Troubleshooting

### "py -3.11 not found"
- Restart your terminal after installing Python
- Or use full path: `C:\Users\YourName\AppData\Local\Programs\Python\Python311\python.exe`

### "tensorflow not found"
- Make sure virtual environment is activated
- Run: `pip install tensorflow numpy` again

### Multiple Python versions
- Use `py -3.11` to specifically use Python 3.11
- Or activate the virtual environment which locks to 3.11

## Alternative: Google Colab (No Install)

If you don't want to install Python 3.11:

1. Go to: https://colab.research.google.com/
2. Upload your training files
3. Run training in the cloud (free GPU!)
4. Download the trained model

### Colab Setup:
```python
# In Colab notebook
!git clone https://github.com/your-repo/fall-detection.git
%cd fall-detection/MagicWand-TFLite-ESP32-MPU6050/train
!python train_fall_detection.py --model CNN
```

## Quick Reference

| Task | Command |
|------|---------|
| Check Python version | `py -3.11 --version` |
| Create venv | `py -3.11 -m venv venv` |
| Activate venv | `venv\Scripts\activate` |
| Install TensorFlow | `pip install tensorflow numpy` |
| Train model | `python train_fall_detection.py --model CNN` |
| Deactivate venv | `deactivate` |

## What Happens During Training

1. Loads 4,500 samples from dataset
2. Trains for 50 epochs (~30-60 minutes)
3. Creates these files:
   - `model_fall_detection_quantized.tflite` ← Use this for ESP32
   - `model_fall_detection.tflite`
   - Weight files in `netmodels/CNN_fall_detection/`

## Next Steps After Training

See `FALL_DETECTION_TRAINING.md` for:
- How to evaluate your model
- How to deploy to ESP32
- How to tune hyperparameters
