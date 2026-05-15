#!/usr/bin/env python3
"""
Preprocess UMAFall dataset for TrekLink training.

Expected dataset location: Fall Detection/DATASET/umaFallData/
Download from: figshare.com/articles/dataset/UMAFall_Fall_Detection_Dataset/4214283
No registration required (CC BY 4.0).

UMAFall CSV format (one file per subject/activity):
  TimeStamp, SampleNo, X, Y, Z, GyroX, GyroY, GyroZ, Device, Activity
  Device values: Ankle(1), RightPocket(2), Chest(3), Wrist(4), Waist(5)
  Activity values: 1–11 = ADL, 12–14 = Falls

Only Device == 5 (Waist) is used to match MPU6050 placement.

Run with --inspect to print column names from your actual files first.

Output: Fall Detection/DATASET/processed/<class>/umafall_<name>.txt
"""

import os
import csv
from pathlib import Path

# ── Adjust if column names differ — run --inspect first ────────────────────────
COL_AX       = "X"
COL_AY       = "Y"
COL_AZ       = "Z"
COL_GX       = "GyroX"
COL_GY       = "GyroY"
COL_GZ       = "GyroZ"
COL_DEVICE   = "Device"
COL_ACTIVITY = "Activity"

WAIST_DEVICE_ID = 5   # Device==5 is Waist in UMAFall

WINDOW_SIZE = 128
STEP_SIZE   = 64

# UMAFall activity IDs → our 5 classes
# Activities 1–11 are ADL, 12–14 are Falls (forward, lateral, backward)
# UMAFall does not have stumble/trip as a separate class — map falls by impact type
ACTIVITY_MAPPING = {
    "not_falling":     [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
    "falling_light":   [],           # UMAFall has no stumble class
    "falling_regular": [12, 13, 14], # Forward, lateral, backward falls
    "falling_extreme": [],           # No stair falls in UMAFall
    "falling_misc":    [],
}

CODE_TO_CLASS = {}
for cls, codes in ACTIVITY_MAPPING.items():
    for c in codes:
        CODE_TO_CLASS[c] = cls


def write_magicwand_window(window, output_file):
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(str(v) for v in row) + "\n")
        f.write("\n")


def process_file(csv_path, output_path, file_idx):
    stats = {cls: 0 for cls in ACTIVITY_MAPPING}

    with open(csv_path, "r") as f:
        # UMAFall files may use semicolons or commas — detect from first line
        sample = f.read(512)
        f.seek(0)
        delimiter = ";" if sample.count(";") > sample.count(",") else ","
        reader = csv.DictReader(f, delimiter=delimiter)
        rows = list(reader)

    if not rows:
        return stats

    # Group by (activity) — each file is already one subject/trial
    groups = {}
    for row in rows:
        try:
            device_id = int(float(row[COL_DEVICE]))
            activity_id = int(float(row[COL_ACTIVITY]))
        except (KeyError, ValueError):
            continue

        if device_id != WAIST_DEVICE_ID:
            continue

        target_class = CODE_TO_CLASS.get(activity_id)
        if target_class is None:
            continue

        if activity_id not in groups:
            groups[activity_id] = (target_class, [])

        try:
            imu = [
                float(row[COL_AX]), float(row[COL_AY]), float(row[COL_AZ]),
                float(row[COL_GX]), float(row[COL_GY]), float(row[COL_GZ]),
            ]
            groups[activity_id][1].append(imu)
        except (KeyError, ValueError):
            continue

    for activity_id, (cls, samples) in groups.items():
        if len(samples) < WINDOW_SIZE:
            continue

        cls_dir = output_path / cls
        cls_dir.mkdir(parents=True, exist_ok=True)

        win_idx = 0
        for start in range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE):
            window = samples[start:start + WINDOW_SIZE]
            fname = f"umafall_F{file_idx:03d}_A{activity_id}_W{win_idx:03d}.txt"
            write_magicwand_window(window, cls_dir / fname)
            stats[cls] += 1
            win_idx += 1

    return stats


def inspect_csv(csv_path):
    with open(csv_path, "r") as f:
        sample = f.read(512)
        f.seek(0)
        delimiter = ";" if sample.count(";") > sample.count(",") else ","
        reader = csv.DictReader(f, delimiter=delimiter)
        print(f"Delimiter: '{delimiter}'")
        print(f"Columns: {reader.fieldnames}")
        for i, row in enumerate(reader):
            if i >= 3:
                break
            print(f"  Row {i}: {dict(row)}")


def main(inspect=False):
    base_path = Path(__file__).parent
    dataset_path = base_path / "umaFallData"
    output_path  = base_path / "processed"

    if not dataset_path.exists():
        print(f"Error: {dataset_path} not found.")
        print("Download from: figshare.com/articles/dataset/UMAFall_Fall_Detection_Dataset/4214283")
        return

    csv_files = list(dataset_path.glob("**/*.csv"))
    if not csv_files:
        print(f"No CSV files found in {dataset_path}")
        return

    if inspect:
        print(f"Inspecting {csv_files[0]}")
        inspect_csv(csv_files[0])
        print("\nIf column names differ, edit the COL_* constants at the top of this script.")
        return

    total_stats = {cls: 0 for cls in ACTIVITY_MAPPING}
    for idx, csv_path in enumerate(csv_files):
        print(f"Processing {csv_path.name}...")
        stats = process_file(csv_path, output_path, idx)
        for cls, count in stats.items():
            total_stats[cls] += count

    print("\n=== UMAFall Processing Complete ===")
    for cls, count in total_stats.items():
        print(f"  {cls}: {count} windows")
    print(f"\nOutput written to: {output_path}")
    print("Run data_prepare_fall_detection.py next to merge with SisFall.")


if __name__ == "__main__":
    import sys
    main(inspect="--inspect" in sys.argv)
