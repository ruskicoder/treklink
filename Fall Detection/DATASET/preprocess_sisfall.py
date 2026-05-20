#!/usr/bin/env python3
"""
Preprocess SisFall dataset for TrekLink training.

Dataset location: Fall Detection/DATASET/sisfallData/
Expected structure: sisfallData/SA01/, SA02/, ..., SE15/
  each containing files like D01_SA01_R01.txt, F01_SA01_R01.txt, etc.

ACTUAL FORMAT (confirmed by inspection):
  - 9 comma-separated integers per line, no header
  - Columns: ADXL345_x, ADXL345_y, ADXL345_z, ITG3200_x, ITG3200_y, ITG3200_z,
             MMA8451Q_x, MMA8451Q_y, MMA8451Q_z
  - We use columns 0-2 (ADXL345 accel) + 3-5 (ITG3200 gyro) = 6 axes
  - Sampling rate: 200 Hz
  - Typical file length: ~19,999 lines (~100 seconds)

Subjects:
  SA01-SA23: 23 young adults
  SE01-SE15: 15 elderly participants

Activities:
  D01-D19: Activities of Daily Living (ADL)
  F01-F15: Falls

Downsampling: 200Hz → 50Hz (keep every 4th sample)
Windowing: 128-sample sliding windows, 50% overlap (64-sample step)

Output: Fall Detection/DATASET/processed/<class>/sisfall_<name>.txt
"""

import math
import os
from pathlib import Path

SOURCE_HZ   = 200
TARGET_HZ   = 50
DECIMATE    = SOURCE_HZ // TARGET_HZ  # = 4

WINDOW_SIZE = 128
STEP_SIZE   = 64   # 50% overlap

# Physical unit conversion: raw ADC -> m/s² and rad/s
# ADXL345 ±2g full-resolution mode: 256 LSB/g (confirmed from datasheet + vector-magnitude check)
# ITG3200: 14.375 LSB/°/s (confirmed from datasheet)
ACCEL_LSB_PER_G  = 256.0
GYRO_LSB_PER_DPS = 14.375
G        = 9.81
DEG2RAD  = math.pi / 180.0

# SisFall activity code → 5-class mapping
ACTIVITY_MAPPING = {
    "not_falling": [
        "D01", "D02", "D03", "D04", "D05", "D06", "D07", "D08",
        "D09", "D10", "D11", "D12", "D13", "D14", "D15", "D16", "D17",
        "D19",
    ],
    "falling_light": [
        "D18",  # Stumble while walking
        "F04",  # Fall forward while walking caused by a trip
    ],
    "falling_regular": [
        "F01",  # Fall forward while walking caused by a slip
        "F02",  # Fall backward while walking caused by a slip
        "F03",  # Lateral fall while walking caused by a slip
        "F05",  # Fall forward when trying to get up
        "F06",  # Fall backward when trying to get up
        "F07",  # Lateral fall when trying to get up
        "F08",  # Fall forward while sitting
        "F09",  # Fall backward while sitting
        "F10",  # Lateral fall while sitting
    ],
    "falling_extreme": [
        "F11",  # Fall while walking downstairs — multiple impacts, high energy
        "F12",  # Fall while walking upstairs — high energy
        "F13",  # Fall forward using hands to dampen — standing height, long fall time
        "F14",  # Fall forward using knees to dampen — standing height, long fall time
        "F15",  # Fall backward using hands to dampen — standing height, long fall time
    ],
}

# Invert for O(1) lookup
CODE_TO_CLASS = {}
for cls, codes in ACTIVITY_MAPPING.items():
    for c in codes:
        CODE_TO_CLASS[c] = cls


def read_sisfall_file(filepath):
    """Read SisFall file, return list of [ax, ay, az, gx, gy, gz] per sample.

    SisFall has two line formats depending on recording:
      ADL files:  'value,value,...,value\\n'        (no trailing semicolon)
      Fall files: 'value,value,...,value;\\n'       (trailing semicolon)
    Both are handled by stripping the trailing semicolon before parsing.
    """
    samples = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip().rstrip(";")
            if not line:
                continue
            try:
                values = [int(x.strip()) for x in line.split(",")]
            except ValueError:
                continue
            if len(values) != 9:
                continue
            ax_ms2 = values[0] / ACCEL_LSB_PER_G * G
            ay_ms2 = values[1] / ACCEL_LSB_PER_G * G
            az_ms2 = values[2] / ACCEL_LSB_PER_G * G
            gx_rs  = values[3] / GYRO_LSB_PER_DPS * DEG2RAD
            gy_rs  = values[4] / GYRO_LSB_PER_DPS * DEG2RAD
            gz_rs  = values[5] / GYRO_LSB_PER_DPS * DEG2RAD
            samples.append([ax_ms2, ay_ms2, az_ms2, gx_rs, gy_rs, gz_rs])
    return samples


def decimate(samples, factor):
    return [samples[i] for i in range(0, len(samples), factor)]


def write_magicwand_window(window, output_file):
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(f"{v:.4f}" for v in row) + "\n")
        f.write("\n")


def process_dataset(sisfall_path, output_path):
    stats = {cls: 0 for cls in ["not_falling", "falling_light", "falling_regular", "falling_extreme"]}

    subject_dirs = sorted([
        d for d in sisfall_path.iterdir()
        if d.is_dir() and not d.name.startswith(".")
    ])
    print(f"Found {len(subject_dirs)} subject folders.")

    for subject_dir in subject_dirs:
        txt_files = sorted(subject_dir.glob("*.txt"))
        for data_file in txt_files:
            activity_code = data_file.stem.split("_")[0]  # e.g. D01_SA01_R01 → D01
            target_class = CODE_TO_CLASS.get(activity_code)
            if target_class is None:
                continue

            samples = read_sisfall_file(data_file)
            if not samples:
                continue

            # Downsample 200Hz → 50Hz
            samples = decimate(samples, DECIMATE)

            if len(samples) < WINDOW_SIZE:
                continue

            cls_dir = output_path / target_class
            cls_dir.mkdir(parents=True, exist_ok=True)

            win_idx = 0
            for start in range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE):
                window = samples[start:start + WINDOW_SIZE]
                fname = f"sisfall_{data_file.stem}_W{win_idx:03d}.txt"
                write_magicwand_window(window, cls_dir / fname)
                stats[target_class] += 1
                win_idx += 1

    return stats


def main():
    base_path    = Path(__file__).parent
    sisfall_path = base_path / "sisfallData"
    output_path  = base_path / "processed"

    if not sisfall_path.exists():
        print(f"Error: {sisfall_path} not found.")
        return

    print(f"Processing SisFall from {sisfall_path}")
    print(f"  Subjects: {len(list(sisfall_path.glob('S*')))} folders")
    print(f"  Decimation: every {DECIMATE}th sample ({SOURCE_HZ}Hz -> {TARGET_HZ}Hz)")
    print(f"  Window: {WINDOW_SIZE} samples, step: {STEP_SIZE}")
    print()

    stats = process_dataset(sisfall_path, output_path)

    print("\n=== SisFall Processing Complete ===")
    total = sum(stats.values())
    for cls, count in stats.items():
        print(f"  {cls}: {count} windows")
    print(f"  Total: {total} windows")
    print(f"\nOutput: {output_path}")


if __name__ == "__main__":
    main()
