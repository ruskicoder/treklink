#!/usr/bin/env python3
"""
Preprocess FallAllD dataset (Kaggle derived version) for TrekLink training.

Expected dataset location: Fall Detection/DATASET/fallalldData/
Download from: kaggle.com/datasets/sankalpsinghvishen/derived-fallalld-dataset

Expected CSV structure (Kaggle derived version, pre-processed to 50Hz):
  SubjectID, ActivityID, TrialNo, AccX, AccY, AccZ, GyroX, GyroY, GyroZ
  OR columns may be named differently — run with --inspect flag first to check.

Only waist sensor data is used (Device == 'waist' or DeviceID == 1).
If your CSV has a device column, set DEVICE_FILTER below.

Output: Fall Detection/DATASET/processed/<class>/fallalld_<name>.txt
        (same folder as SisFall output — data_prepare merges them automatically)
"""

import os
import csv
from pathlib import Path

# ── Adjust these to match your actual CSV column names ─────────────────────────
# Run: python preprocess_fallalld.py --inspect   to print column names first.
COL_SUBJECT  = "SubjectID"
COL_ACTIVITY = "ActivityID"
COL_AX       = "AccX"
COL_AY       = "AccY"
COL_AZ       = "AccZ"
COL_GX       = "GyroX"
COL_GY       = "GyroY"
COL_GZ       = "GyroZ"
COL_DEVICE   = "Device"   # set to None if no device column

# Only keep waist sensor rows. Set to None to skip filtering.
DEVICE_WAIST_VALUE = "waist"  # or 1, or "Waist" — check your CSV

# Window size in samples (50Hz × 2.56s = 128 samples, matching SisFall windows)
WINDOW_SIZE = 128
STEP_SIZE   = 64   # 50% overlap

# FallAllD activity codes → our 5 classes
# ActivityID 1–34 = ADL, 101–115 = Falls (check dataset README for exact mapping)
ACTIVITY_MAPPING = {
    "not_falling": list(range(1, 35)),   # ADL01–ADL34
    "falling_light": [101, 102],         # Falls with low impact (trip, stumble)
    "falling_regular": [103, 104, 105, 106, 107, 108],
    "falling_extreme": [109, 110, 111, 112, 113],
    "falling_misc": [114, 115],
}

# Invert for O(1) lookup
CODE_TO_CLASS = {}
for cls, codes in ACTIVITY_MAPPING.items():
    for c in codes:
        CODE_TO_CLASS[c] = cls


def write_magicwand_window(window, output_file):
    """Write one 128-sample window in MagicWand format."""
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(str(v) for v in row) + "\n")
        f.write("\n")


def process_file(csv_path, output_path):
    """Segment a single CSV file into fixed windows per activity."""
    stats = {cls: 0 for cls in ACTIVITY_MAPPING}

    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        return stats

    # Group rows by (subject, activity, trial) to avoid cross-trial contamination
    groups = {}
    for row in rows:
        try:
            activity_id = int(row[COL_ACTIVITY])
        except (KeyError, ValueError):
            continue

        target_class = CODE_TO_CLASS.get(activity_id)
        if target_class is None:
            continue

        if COL_DEVICE and COL_DEVICE in row:
            if str(row[COL_DEVICE]).lower() not in (str(DEVICE_WAIST_VALUE).lower(),):
                continue

        key = (row.get(COL_SUBJECT, "0"), activity_id)
        if key not in groups:
            groups[key] = (target_class, [])

        try:
            imu = [
                float(row[COL_AX]), float(row[COL_AY]), float(row[COL_AZ]),
                float(row[COL_GX]), float(row[COL_GY]), float(row[COL_GZ]),
            ]
            groups[key][1].append(imu)
        except (KeyError, ValueError):
            continue

    # Slide windows over each group
    for (subject, activity_id), (cls, samples) in groups.items():
        if len(samples) < WINDOW_SIZE:
            continue

        cls_dir = output_path / cls
        cls_dir.mkdir(parents=True, exist_ok=True)

        win_idx = 0
        for start in range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE):
            window = samples[start:start + WINDOW_SIZE]
            fname = f"fallalld_S{subject}_A{activity_id}_W{win_idx:03d}.txt"
            write_magicwand_window(window, cls_dir / fname)
            stats[cls] += 1
            win_idx += 1

    return stats


def inspect_csv(csv_path):
    """Print column names and a few rows — run this first to verify mapping."""
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        print(f"Columns: {reader.fieldnames}")
        for i, row in enumerate(reader):
            if i >= 3:
                break
            print(f"  Row {i}: {dict(row)}")


def main(inspect=False):
    base_path = Path(__file__).parent
    dataset_path = base_path / "fallalldData"
    output_path  = base_path / "processed"

    if not dataset_path.exists():
        print(f"Error: {dataset_path} not found.")
        print("Download from kaggle.com/datasets/sankalpsinghvishen/derived-fallalld-dataset")
        return

    csv_files = list(dataset_path.glob("**/*.csv"))
    if not csv_files:
        print(f"No CSV files found in {dataset_path}")
        return

    if inspect:
        print(f"Inspecting {csv_files[0]}")
        inspect_csv(csv_files[0])
        print("\nIf column names differ from defaults, edit the COL_* constants at the top of this script.")
        return

    total_stats = {cls: 0 for cls in ACTIVITY_MAPPING}
    for csv_path in csv_files:
        print(f"Processing {csv_path.name}...")
        stats = process_file(csv_path, output_path)
        for cls, count in stats.items():
            total_stats[cls] += count

    print("\n=== FallAllD Processing Complete ===")
    for cls, count in total_stats.items():
        print(f"  {cls}: {count} windows")
    print(f"\nOutput written to: {output_path}")
    print("Run data_prepare_fall_detection.py next to merge with SisFall.")


if __name__ == "__main__":
    import sys
    main(inspect="--inspect" in sys.argv)
