#!/usr/bin/env python3
"""
Preprocess FallAllD dataset (raw .dat files from IEEE DataPort) for TrekLink training.

Dataset location: Fall Detection/DATASET/FallAllD/
All 6,274 .dat files should be in a single flat folder (no subfolders).

ACTUAL FORMAT (confirmed by inspection):
  - Files: S{SS}_D{D}_A{AAA}_T{TT}_{X}.dat
      SS  = Subject ID (01-15)
      D   = Device: 1=Neck, 2=Wrist, 3=Waist  ← we only use D3
      AAA = Activity ID (3 digits, zero-padded)
      TT  = Trial number
      X   = Sensor: A=Accelerometer, G=Gyroscope, M=Magnetometer, B=Barometer
  - Content: plain CSV text, NO HEADER, one sample per line: x,y,z (integers)
  - Sampling rate: 238 Hz
  - Typical file length: 4760 lines (~20 seconds of data)

Strategy:
  - Use D3 (Waist) files only — matches MPU6050 hip/waist placement
  - Pair _A.dat (accel) with _G.dat (gyro) for each recording -> 6 axes
  - Downsample 238Hz -> ~47.6Hz by keeping every 5th sample (close enough to 50Hz)
  - Slide 128-sample windows with 50% overlap

Output: Fall Detection/DATASET/processed/<class>/fallalld_<name>.txt
        (same folder as SisFall — data_prepare merges automatically)
"""

import math
import os
from pathlib import Path

# Downsample factor: 238Hz / 5 = 47.6Hz ≈ 50Hz
SOURCE_HZ = 238
TARGET_HZ = 50
DECIMATE   = round(SOURCE_HZ / TARGET_HZ)  # = 5

WINDOW_SIZE = 128
STEP_SIZE   = 64   # 50% overlap

# Physical unit conversion: raw ADC -> m/s² and rad/s
# MPU-9250 ±8g range: 4096 LSB/g (confirmed empirically via vector-magnitude of lying-still file)
# MPU-9250 ±2000°/s:  16.384 LSB/°/s (confirmed from datasheet)
ACCEL_LSB_PER_G  = 4096.0
GYRO_LSB_PER_DPS = 16.384
G        = 9.81
DEG2RAD  = math.pi / 180.0

# Activity ID -> 5-class mapping
# Based on ActivityID2Str.m from FallAllD Script folder
ADL_CODES = set(range(1, 45))  # 1–44 = ADL

FALL_CLASS_MAP = {
    # Forward falls while walking (trip) — standing height, partial control
    101: "falling_light",   # Fall F, walking, trip
    102: "falling_light",   # Fall F, walking, trip, recovery
    # Slip falls while walking — standing height, uncontrolled
    103: "falling_regular", # Fall F, walking, slip
    104: "falling_regular", # Fall F, walking, slip, recovery
    105: "falling_regular", # Fall F, walking, slip, rotational
    106: "falling_regular", # Fall F, walking, slip, rotational, recovery
    107: "falling_regular", # Fall B, walking, slip
    108: "falling_regular", # Fall B, walking, slip, recovery
    109: "falling_regular", # Fall B, walking, slip, rotational
    110: "falling_regular", # Fall B, walking, slip, rotational, recovery
    # Syncope while walking — standing height, fully uncontrolled, dead weight
    111: "falling_extreme", # Fall F, walking, syncope
    112: "falling_extreme", # Fall B, walking, syncope
    113: "falling_extreme", # Fall L, walking, syncope
    114: "falling_regular", # Fall, syncope, table — table shortens fall height
    # Sitting attempt falls — low height (~0.5m), short fall time
    115: "falling_regular", # Fall F, try sit
    116: "falling_regular", # Fall F, try sit, recovery
    117: "falling_regular", # Fall B, try sit
    118: "falling_regular", # Fall B, try sit, recovery
    119: "falling_regular", # Fall L, try sit
    120: "falling_regular", # Fall L, try sit, recovery
    # Jogging falls — high kinetic energy even for trips
    121: "falling_regular", # Fall F, jog, trip — jogging speed = more energy than walking trip
    122: "falling_regular", # Fall F, jog, trip, recovery
    123: "falling_extreme", # Fall F, jog, slip — high speed + uncontrolled = extreme
    124: "falling_extreme", # Fall F, jog, slip, reverse
    125: "falling_extreme", # Fall F, jog, slip, rotational
    126: "falling_regular", # Fall F, jog, slip, rotational, recovery — recovered
    # Bed falls — low height (~0.5m), soft surface
    127: "falling_light",   # Fall L, bed
    128: "falling_light",   # Fall L, bed, recovery
    # Chair syncope — already seated, short fall height (~0.5m)
    129: "falling_regular", # Fall F, chair, syncope
    130: "falling_regular", # Fall B, chair, syncope
    131: "falling_regular", # Fall L, chair, syncope
    # Syncope from standing — full height (~1m), fully uncontrolled, dead weight
    132: "falling_extreme", # Fall F, syncope
    133: "falling_extreme", # Fall B, syncope
    134: "falling_extreme", # Fall L, syncope
    135: "falling_extreme", # Fall, syncope, slide over wall
}

# ADL special cases
ADL_OVERRIDE = {
    23: "falling_light",  # Stumbling
    15: "not_falling",    # Fail to stand up — near-zero height, sits back down
}


def parse_filename(fname):
    """Extract (subject, device, activity_id, trial) from filename.
    Example: S01_D3_A013_T01_A.dat -> (1, 3, 13, 1)
    """
    try:
        subject  = int(fname[1:3])   # S01 -> "01"
        device   = int(fname[5])     # S01_D3 -> fname[5]='3'
        activity = int(fname[8:11])  # S01_D3_A013 -> fname[8:11]='013'
        trial    = int(fname[13:15]) # S01_D3_A013_T01 -> fname[13:15]='01'
        return subject, device, activity, trial
    except (ValueError, IndexError):
        return None


def read_dat(filepath):
    """Read a .dat file as list of [x, y, z] float lists."""
    samples = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) == 3:
                try:
                    samples.append([float(p) for p in parts])
                except ValueError:
                    continue
    return samples


def decimate(samples, factor):
    """Keep every Nth sample (simple decimation, no anti-aliasing filter)."""
    return [samples[i] for i in range(0, len(samples), factor)]


def write_magicwand_window(window, output_file):
    """Write one 128-sample window in MagicWand/SisFall pipeline format."""
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(f"{v:.4f}" for v in row) + "\n")
        f.write("\n")


def process_dataset(dataset_path, output_path):
    stats = {cls: 0 for cls in ["not_falling", "falling_light",
                                  "falling_regular", "falling_extreme"]}

    # Find all Waist (D3) accelerometer files
    all_files = os.listdir(dataset_path)
    accel_files = [f for f in all_files if "_D3_" in f and f.endswith("_A.dat")]
    print(f"Found {len(accel_files)} D3 (Waist) accelerometer files.")

    if not accel_files:
        print("ERROR: No D3 (Waist) files found. Check dataset_path is correct.")
        return stats

    for accel_fname in sorted(accel_files):
        parsed = parse_filename(accel_fname)
        if parsed is None:
            print(f"  Skipping (bad filename): {accel_fname}")
            continue

        subject, device, activity_id, trial = parsed

        # Determine class
        if activity_id in FALL_CLASS_MAP:
            target_class = FALL_CLASS_MAP[activity_id]
        elif activity_id in ADL_OVERRIDE:
            target_class = ADL_OVERRIDE[activity_id]
        elif activity_id in ADL_CODES:
            target_class = "not_falling"
        else:
            continue  # Unknown activity ID — skip

        # Find matching gyro file (replace trailing _A.dat with _G.dat)
        gyro_fname = accel_fname[:-5] + "G.dat"  # swap last char before .dat
        accel_path = os.path.join(dataset_path, accel_fname)
        gyro_path  = os.path.join(dataset_path, gyro_fname)

        if not os.path.exists(gyro_path):
            print(f"  Warning: no matching gyro file for {accel_fname} — skipping")
            continue

        # Load and downsample
        accel = decimate(read_dat(accel_path), DECIMATE)
        gyro  = decimate(read_dat(gyro_path),  DECIMATE)

        # Align lengths (should be equal but guard against off-by-one)
        n = min(len(accel), len(gyro))
        if n < WINDOW_SIZE:
            continue

        # Merge and convert: raw ADC -> [ax_m/s², ay, az, gx_rad/s, gy, gz]
        combined = []
        for i in range(n):
            a = accel[i]
            g = gyro[i]
            combined.append([
                a[0] / ACCEL_LSB_PER_G * G, a[1] / ACCEL_LSB_PER_G * G, a[2] / ACCEL_LSB_PER_G * G,
                g[0] / GYRO_LSB_PER_DPS * DEG2RAD, g[1] / GYRO_LSB_PER_DPS * DEG2RAD, g[2] / GYRO_LSB_PER_DPS * DEG2RAD,
            ])

        # Create output dir
        cls_dir = output_path / target_class
        cls_dir.mkdir(parents=True, exist_ok=True)

        # Slide windows
        win_idx = 0
        for start in range(0, n - WINDOW_SIZE + 1, STEP_SIZE):
            window = combined[start:start + WINDOW_SIZE]
            fname = f"fallalld_S{subject:02d}_A{activity_id:03d}_T{trial:02d}_W{win_idx:03d}.txt"
            write_magicwand_window(window, cls_dir / fname)
            stats[target_class] += 1
            win_idx += 1

    return stats


def main():
    base_path    = Path(__file__).parent
    dataset_path = base_path / "FallAllD"
    output_path  = base_path / "processed"

    if not dataset_path.exists():
        print(f"Error: {dataset_path} not found.")
        print("Place all FallAllD .dat files (flat, no subfolders) in that folder.")
        return

    print(f"Processing FallAllD from {dataset_path}")
    print(f"  Decimation: every {DECIMATE}th sample ({SOURCE_HZ}Hz -> ~{SOURCE_HZ/DECIMATE:.1f}Hz)")
    print(f"  Window: {WINDOW_SIZE} samples, step: {STEP_SIZE} samples")
    print()

    stats = process_dataset(dataset_path, output_path)

    print("\n=== FallAllD Processing Complete ===")
    total = sum(stats.values())
    for cls, count in stats.items():
        print(f"  {cls}: {count} windows")
    print(f"  Total: {total} windows")
    print(f"\nOutput: {output_path}")
    print("Run data_prepare_fall_detection.py next to merge with SisFall.")


if __name__ == "__main__":
    main()
