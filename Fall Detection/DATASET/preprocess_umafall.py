#!/usr/bin/env python3
"""
Preprocess UMAFall dataset for TrekLink training.

USE THE CORRECTED VERSION:
  Dataset location: Fall Detection/DATASET/UMAFall_Dataset_corrected_version/
  (Not UMAFall_Dataset/ — the corrected version fixes sensor data errors.)

ACTUAL FORMAT (confirmed by inspection):
  - One CSV per subject/activity/trial, named:
      UMAFall_Subject_{SS}_{ADL|Fall}_{activityName}_{trial}_{timestamp}.csv
  - File content:
      Lines starting with '%' = header/metadata comments — SKIP THESE
      Data lines (semicolon-separated, 7 columns):
        TimeStamp ; Sample No ; X-Axis ; Y-Axis ; Z-Axis ; Sensor Type ; Sensor ID
        96;1;0.048;0.950;0.069;0;0
  - Sensor Type:  0=Accelerometer, 1=Gyroscope, 2=Magnetometer
  - Sensor ID:    0=RightPocket, 1=Chest, 2=Waist, 3=Wrist, 4=Ankle

⚠️  SAMPLING RATE WARNING (Waist sensor):
  The Waist sensor (SensorTag device) produces ~299 rows per recording file
  for accel AND 299 rows for gyro. If a recording lasts ~30s, this is ~10Hz.
  Training data from SisFall is at 50Hz (128 samples = 2.56s window).
  Mixing 10Hz waist data with 50Hz SisFall data means windows from UMAFall
  actually span ~12.8 seconds of real time, not 2.56 seconds.

  RECOMMENDED: Use RightPocket (Sensor ID=0) smartphone accelerometer data
  which has ~2977 rows per file ≈ 100Hz. This matches our 50Hz target better
  after decimation (keep every 2nd sample → 50Hz).

  This script defaults to RightPocket data. Set USE_WAIST=True below to
  override and use Waist SensorTag data (10Hz, may degrade model).

ACTIVITY from FILENAME (not from data columns):
  Filename contains: ADL_Walking, ADL_Jogging, Fall_forwardFall, etc.
  Fall types: forwardFall, backwardFall, lateralFall → all falling_regular
  ADL types: Walking, Jogging, Hopping, Bending, etc. → not_falling

Output: Fall Detection/DATASET/processed/<class>/umafall_<name>.txt
"""

import math
import os
from pathlib import Path

# ── Configuration ────────────────────────────────────────────────────────────
# False = use RightPocket smartphone (ID=0, ~100Hz, better quality for 50Hz pipeline)
# True  = use Waist SensorTag (ID=2, ~10Hz, may degrade model — see warning above)
USE_WAIST = True  # RightPocket has accel-only (no gyro rows); Waist SensorTag has 6-axis at ~10Hz

WAIST_ID      = 2
RIGHTPOCKET_ID = 0
SENSOR_ACCEL  = 0
SENSOR_GYRO   = 1

# Decimation to reach ~50Hz
# RightPocket (~100Hz): keep every 2nd sample → 50Hz
# Waist (~10Hz): no decimation (already below 50Hz)
SOURCE_HZ_POCKET = 100
SOURCE_HZ_WAIST  = 10
TARGET_HZ        = 50

WINDOW_SIZE = 128
STEP_SIZE   = 64

# Physical unit conversion
# UMAFall accel is in g (confirmed: lying-still Y ≈ 0.948g = gravity)
# UMAFall gyro is in °/s (confirmed: at-rest drift ≈ ±2.6°/s, consistent with uncalibrated MEMS)
G       = 9.81
DEG2RAD = math.pi / 180.0

# Fall types in filename → our 5 classes
FALL_NAME_MAP = {
    "forwardFall":  "falling_regular",
    "backwardFall": "falling_regular",
    "lateralFall":  "falling_regular",
}


def parse_activity_from_filename(fname):
    """Extract (is_fall, class_name) from UMAFall filename.

    Examples:
      UMAFall_Subject_02_Fall_forwardFall_1_... → (True, 'falling_regular')
      UMAFall_Subject_02_ADL_Walking_1_...      → (False, 'not_falling')
    """
    parts = fname.split("_")
    # Find ADL or Fall marker
    try:
        if "Fall" in parts:
            idx = parts.index("Fall")
            fall_type = parts[idx + 1]  # e.g. 'forwardFall'
            cls = FALL_NAME_MAP.get(fall_type, "falling_regular")
            return True, cls
        elif "ADL" in parts:
            return False, "not_falling"
    except (ValueError, IndexError):
        pass
    return None, None


def read_umafall_csv(filepath, sensor_id, decimate_factor):
    """Read accel and gyro rows for a specific sensor_id, return 6-axis list."""
    accel_rows = []
    gyro_rows  = []

    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("%"):
                continue
            parts = line.split(";")
            if len(parts) < 7:
                continue
            try:
                s_type = int(float(parts[5]))
                s_id   = int(float(parts[6]))
                x = float(parts[2])
                y = float(parts[3])
                z = float(parts[4])
            except (ValueError, IndexError):
                continue

            if s_id != sensor_id:
                continue

            if s_type == SENSOR_ACCEL:
                accel_rows.append([x, y, z])
            elif s_type == SENSOR_GYRO:
                gyro_rows.append([x, y, z])

    # Decimate both
    accel_rows = [accel_rows[i] for i in range(0, len(accel_rows), decimate_factor)]
    gyro_rows  = [gyro_rows[i]  for i in range(0, len(gyro_rows),  decimate_factor)]

    # Align lengths
    n = min(len(accel_rows), len(gyro_rows))
    if n == 0:
        return []

    # Merge and convert to physical units: [ax_m/s², ay, az, gx_rad/s, gy, gz]
    result = []
    for i in range(n):
        a = accel_rows[i]
        g = gyro_rows[i]
        result.append([
            a[0] * G, a[1] * G, a[2] * G,
            g[0] * DEG2RAD, g[1] * DEG2RAD, g[2] * DEG2RAD,
        ])
    return result


def write_magicwand_window(window, output_file):
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(f"{v:.6f}" for v in row) + "\n")
        f.write("\n")


def process_dataset(dataset_path, output_path):
    stats = {cls: 0 for cls in ["not_falling", "falling_light",
                                  "falling_regular", "falling_extreme"]}

    if USE_WAIST:
        sensor_id = WAIST_ID
        decimate_factor = 1  # 10Hz, no decimation
        print("Using Waist (SensorTag, ~10Hz) — see sampling rate warning in script header.")
    else:
        sensor_id = RIGHTPOCKET_ID
        decimate_factor = max(1, round(SOURCE_HZ_POCKET / TARGET_HZ))  # = 2
        print(f"Using RightPocket smartphone (~{SOURCE_HZ_POCKET}Hz -> decimated to ~{SOURCE_HZ_POCKET//decimate_factor}Hz).")

    csv_files = sorted(dataset_path.glob("*.csv"))
    print(f"Found {len(csv_files)} CSV files.")

    for i, csv_path in enumerate(csv_files):
        if i % 50 == 0:
            print(f"  Processing {i}/{len(csv_files)}...")

        is_fall, target_class = parse_activity_from_filename(csv_path.stem)
        if target_class is None:
            continue

        samples = read_umafall_csv(csv_path, sensor_id, decimate_factor)
        if len(samples) < WINDOW_SIZE:
            continue

        cls_dir = output_path / target_class
        cls_dir.mkdir(parents=True, exist_ok=True)

        win_idx = 0
        for start in range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE):
            window = samples[start:start + WINDOW_SIZE]
            safe_stem = csv_path.stem.replace(" ", "_")[:60]
            fname = f"umafall_{safe_stem}_W{win_idx:03d}.txt"
            write_magicwand_window(window, cls_dir / fname)
            stats[target_class] += 1
            win_idx += 1

    return stats


def main():
    base_path    = Path(__file__).parent
    # Always use corrected version
    dataset_path = base_path / "UMAFall_Dataset_corrected_version"
    output_path  = base_path / "processed"

    if not dataset_path.exists():
        print(f"Error: {dataset_path} not found.")
        print("Expected folder: UMAFall_Dataset_corrected_version/")
        print("Do NOT use UMAFall_Dataset/ (has sensor data errors).")
        return

    print(f"Processing UMAFall (corrected version) from {dataset_path}")
    print(f"  Window: {WINDOW_SIZE} samples, step: {STEP_SIZE}")
    print()

    stats = process_dataset(dataset_path, output_path)

    print("\n=== UMAFall Processing Complete ===")
    total = sum(stats.values())
    for cls, count in stats.items():
        print(f"  {cls}: {count} windows")
    print(f"  Total: {total} windows")
    if total == 0:
        print("\n  Got 0 windows — check sensor ID / decimation settings.")
        print("  Try toggling USE_WAIST = True/False at top of script.")
    print(f"\nOutput: {output_path}")


if __name__ == "__main__":
    main()
