#!/usr/bin/env python3
"""
Preprocess KFall dataset for TrekLink training.

Expected dataset location: Fall Detection/DATASET/kfallData/
Request access from: sites.google.com/view/kfalldataset
Requires institutional email — usually approved within 1–2 days.

KFall is the only public dataset with PRE-IMPACT fall labels:
  - fall_initiation_timestamp: when the fall begins (body still upright)
  - fall_impact_timestamp: when body hits the ground
  Using fall_initiation as the label trains the model to detect BEFORE impact —
  this is critical for trekking where you need time to send a GPS alert.

KFall CSV format (one file per subject/trial):
  Timestamp, AccX, AccY, AccZ, GyroX, GyroY, GyroZ, EulerX, EulerY, EulerZ,
  FallInitiation, FallImpact
  (Euler angles are dropped — only accel + gyro are used to match MPU6050)
  Sampling rate: ~100Hz → downsampled to 50Hz to match training pipeline.

Run with --inspect to check actual column names before processing.

Output: Fall Detection/DATASET/processed/<class>/kfall_<name>.txt
"""

import os
import csv
from pathlib import Path

# ── Adjust if column names differ — run --inspect first ────────────────────────
COL_TIMESTAMP   = "Timestamp"
COL_AX          = "AccX"
COL_AY          = "AccY"
COL_AZ          = "AccZ"
COL_GX          = "GyroX"
COL_GY          = "GyroY"
COL_GZ          = "GyroZ"
COL_FALL_INIT   = "FallInitiation"  # 1 at fall initiation timestamp, else 0
COL_FALL_IMPACT = "FallImpact"      # 1 at impact timestamp, else 0

# KFall sampling rate. Will be downsampled to TARGET_HZ to match MPU6050.
SOURCE_HZ  = 100
TARGET_HZ  = 50
DOWNSAMPLE = SOURCE_HZ // TARGET_HZ  # keep every Nth sample

WINDOW_SIZE = 128   # samples at 50Hz = 2.56s
STEP_SIZE   = 64    # 50% overlap

# KFall activity codes → our 5 classes (check dataset README for exact IDs)
# KFall includes all 15 SisFall fall types + additional ADLs
ADL_CODES = list(range(1, 22))     # ADL01–ADL21
FALL_CODES = list(range(101, 116)) # F01–F15 (same as SisFall fall types)

# Map fall types to our severity classes (aligned with SisFall mapping)
FALL_CLASS_MAP = {
    101: "falling_light",    # F01 - slip forward (trip)
    102: "falling_light",    # F02 - stumble
    103: "falling_regular",  # F03 - slip forward
    104: "falling_regular",  # F04 - slip backward
    105: "falling_regular",  # F05 - lateral slip
    106: "falling_regular",  # F06 - fall from sitting
    107: "falling_regular",  # F07 - fall from standing
    108: "falling_regular",  # F08 - fall from sitting sideways
    109: "falling_regular",  # F09 - fall forward while walking
    110: "falling_regular",  # F10 - fall backward while walking
    111: "falling_extreme",  # F11 - fall downstairs
    112: "falling_extreme",  # F12 - fall upstairs
    113: "falling_extreme",  # F13 - fall with hands dampening
    114: "falling_extreme",  # F14 - fall with knees dampening
    115: "falling_misc",     # F15 - miscellaneous
}


def write_magicwand_window(window, output_file):
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(str(v) for v in row) + "\n")
        f.write("\n")


def downsample(samples):
    """Keep every Nth sample to go from SOURCE_HZ to TARGET_HZ."""
    return [samples[i] for i in range(0, len(samples), DOWNSAMPLE)]


def process_file(csv_path, output_path, file_idx):
    stats = {cls: 0 for cls in ["not_falling", "falling_light", "falling_regular",
                                  "falling_extreme", "falling_misc"]}

    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        return stats

    # Determine if this file is ADL or Fall based on filename or content
    # KFall filenames typically include activity code: e.g. Task_01_Subject_01.csv
    fname_lower = csv_path.stem.lower()

    # Try to extract activity code from filename (e.g. "task_101" → 101)
    activity_id = None
    for part in csv_path.stem.replace("-", "_").split("_"):
        try:
            activity_id = int(part)
            if activity_id in ADL_CODES or activity_id in FALL_CODES:
                break
        except ValueError:
            continue

    if activity_id is None:
        print(f"  Warning: could not determine activity from {csv_path.name} — skipping")
        return stats

    if activity_id in ADL_CODES:
        target_class = "not_falling"
    elif activity_id in FALL_CLASS_MAP:
        target_class = FALL_CLASS_MAP[activity_id]
    else:
        return stats

    # Parse and downsample
    samples = []
    fall_init_idx = None
    for i, row in enumerate(rows):
        if i % DOWNSAMPLE != 0:
            continue
        try:
            imu = [
                float(row[COL_AX]), float(row[COL_AY]), float(row[COL_AZ]),
                float(row[COL_GX]), float(row[COL_GY]), float(row[COL_GZ]),
            ]
            samples.append(imu)

            # Record fall initiation position for pre-impact windowing
            if fall_init_idx is None and COL_FALL_INIT in row:
                try:
                    if int(float(row[COL_FALL_INIT])) == 1:
                        fall_init_idx = len(samples) - 1
                except ValueError:
                    pass
        except (KeyError, ValueError):
            continue

    if len(samples) < WINDOW_SIZE:
        return stats

    cls_dir = output_path / target_class
    cls_dir.mkdir(parents=True, exist_ok=True)

    # For fall files: anchor a window around fall_initiation if available
    # This trains the model on pre-impact signal rather than post-impact.
    if target_class != "not_falling" and fall_init_idx is not None:
        # Window ending at fall initiation (pre-impact window)
        start = max(0, fall_init_idx - WINDOW_SIZE + 1)
        end = start + WINDOW_SIZE
        if end <= len(samples):
            window = samples[start:end]
            fname = f"kfall_F{file_idx:03d}_A{activity_id}_preimpact.txt"
            write_magicwand_window(window, cls_dir / fname)
            stats[target_class] += 1

    # Also slide windows over entire recording for more training examples
    win_idx = 0
    for start in range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE):
        window = samples[start:start + WINDOW_SIZE]
        fname = f"kfall_F{file_idx:03d}_A{activity_id}_W{win_idx:03d}.txt"
        write_magicwand_window(window, cls_dir / fname)
        stats[target_class] += 1
        win_idx += 1

    return stats


def inspect_csv(csv_path):
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        print(f"Columns: {reader.fieldnames}")
        for i, row in enumerate(reader):
            if i >= 3:
                break
            print(f"  Row {i}: {dict(row)}")


def main(inspect=False):
    base_path = Path(__file__).parent
    dataset_path = base_path / "kfallData"
    output_path  = base_path / "processed"

    if not dataset_path.exists():
        print(f"Error: {dataset_path} not found.")
        print("Request access from: sites.google.com/view/kfalldataset")
        print("Requires institutional email — approved in 1–2 days.")
        return

    csv_files = list(dataset_path.glob("**/*.csv"))
    if not csv_files:
        print(f"No CSV files found in {dataset_path}")
        return

    if inspect:
        print(f"Inspecting {csv_files[0]}")
        inspect_csv(csv_files[0])
        print("\nIf column names differ, edit the COL_* constants at the top of this script.")
        print("Also verify activity codes in ADL_CODES / FALL_CLASS_MAP match your dataset README.")
        return

    total_stats = {cls: 0 for cls in ["not_falling", "falling_light",
                                        "falling_regular", "falling_extreme", "falling_misc"]}
    for idx, csv_path in enumerate(csv_files):
        print(f"Processing {csv_path.name}...")
        stats = process_file(csv_path, output_path, idx)
        for cls, count in stats.items():
            total_stats[cls] += count

    print("\n=== KFall Processing Complete ===")
    for cls, count in total_stats.items():
        print(f"  {cls}: {count} windows")
    print(f"\nOutput written to: {output_path}")
    print("Run data_prepare_fall_detection.py next to merge all datasets.")


if __name__ == "__main__":
    import sys
    main(inspect="--inspect" in sys.argv)
