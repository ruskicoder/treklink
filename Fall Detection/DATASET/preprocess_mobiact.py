#!/usr/bin/env python3
"""
Preprocess MobiAct v2.0 dataset for TrekLink training.

Dataset location: Fall Detection/DATASET/MobiActData/
Expected structure:
  MobiActData/Annotated Data/<ActivityCode>/<SubjectID>_<ActivityCode>_<TrialNumber>.csv

CSV columns (no header):
  timestamp, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, azimuth, pitch, roll, label
  acc_x/y/z : m/s²  — already in target units, no conversion needed
  gyro_x/y/z: rad/s — already in target units, no conversion needed
  azimuth/pitch/roll: degrees (dropped — not available in firmware)
  label     : activity code string (e.g. "FOL", "WAL") or "0" for transitions

Sampling rate: 100 Hz → 50 Hz (keep every 2nd sample)
Windowing:
  Falls — extract 1–2 windows centered on the annotated fall segment
  ADL   — sliding 128-sample windows, 64-sample step over entire file

Fall activities (use annotation to window the fall event):
  FOL: Forward fall
  FKL: Fall on knees
  BSC: Backward slow fall (syncope-like)
  SDL: Lateral fall

ADL activities (slide entire file — these are missing from SisFall/KFall):
  WAL: Walking          — level-walking diversity currently weak in training data
  JOG: Jogging          — critical: high G non-fall, currently absent
  JUM: Jumping          — high-G burst, similar impulse profile to light falls
  STD: Standing
  STU: Going up stairs
  STN: Going down stairs
  CSO: Getting into car
  CSI: Getting out of car
  SIT: Sitting
  LYI: Lying

Output: Fall Detection/DATASET/processed/<class>/mobiact_<subject>_<activity>_<trial>_W<n>.txt
"""

import csv
import os
from pathlib import Path

SOURCE_HZ  = 100
TARGET_HZ  = 50
DECIMATE   = SOURCE_HZ // TARGET_HZ  # = 2

WINDOW_SIZE = 128
STEP_SIZE   = 64   # 50% overlap for ADL; also used as minimum separation check for falls

# Fall activity codes → severity class (same taxonomy as SisFall/KFall)
FALL_CLASS_MAP = {
    "FOL": "falling_regular",   # Forward fall — typical slip/trip outcome
    "FKL": "falling_light",     # Fall on knees — dampened, partial control
    "BSC": "falling_extreme",   # Backward syncope — uncontrolled, full height
    "SDL": "falling_regular",   # Lateral fall — hip impact, no hand dampen
}

# ADL activity codes to include (mapped to not_falling)
ADL_CODES = {
    "WAL", "JOG", "JUM",
    "STD", "STU", "STN",
    "CSO", "CSI", "SIT", "LYI",
}

ALL_ACTIVITY_CODES = set(FALL_CLASS_MAP.keys()) | ADL_CODES


def read_mobiact_csv(filepath):
    """Read MobiAct CSV, return list of ([ax, ay, az, gx, gy, gz], label_str)."""
    samples = []
    with open(filepath, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].strip().lower() in ("timestamp", "time"):
                continue  # skip header if present
            if len(row) < 10:
                continue
            try:
                ax = float(row[1])
                ay = float(row[2])
                az = float(row[3])
                gx = float(row[4])
                gy = float(row[5])
                gz = float(row[6])
                label = row[10].strip() if len(row) > 10 else ""
            except (ValueError, IndexError):
                continue
            samples.append(([ax, ay, az, gx, gy, gz], label))
    return samples


def decimate(samples, factor):
    return [samples[i] for i in range(0, len(samples), factor)]


def write_magicwand_window(window, output_file):
    with open(output_file, "w") as f:
        f.write("-.-.-\n")
        for row in window:
            f.write(",".join(f"{v:.6f}" for v in row) + "\n")
        f.write("\n")


def extract_fall_windows(samples_imu, labeled_indices):
    """Return 1–2 windows around the annotated fall segment.

    samples_imu : list of [ax, ay, az, gx, gy, gz] after decimation
    labeled_indices: list of decimated-frame indices where label == fall code
    """
    if not labeled_indices:
        return []

    n = len(samples_imu)
    windows = []

    onset  = labeled_indices[0]
    impact = labeled_indices[-1]

    # Primary: center on fall midpoint
    center = (onset + impact) // 2
    start = max(0, min(center - WINDOW_SIZE // 2, n - WINDOW_SIZE))
    if start + WINDOW_SIZE <= n:
        windows.append(samples_imu[start:start + WINDOW_SIZE])

    # Secondary: onset-anchored window if the fall spans more than half a window
    if (impact - onset) > WINDOW_SIZE // 2:
        start2 = max(0, min(onset - 16, n - WINDOW_SIZE))
        if start2 + WINDOW_SIZE <= n and abs(start2 - start) > STEP_SIZE:
            windows.append(samples_imu[start2:start2 + WINDOW_SIZE])

    return windows


def process_dataset(data_root, output_path):
    stats = {cls: 0 for cls in ["not_falling", "falling_light", "falling_regular", "falling_extreme"]}
    skipped = 0

    annotated_root = data_root / "Annotated Data"
    if not annotated_root.exists():
        # Some distributions drop the subfolder — try root directly
        annotated_root = data_root

    activity_dirs = sorted(
        d for d in annotated_root.iterdir()
        if d.is_dir() and d.name in ALL_ACTIVITY_CODES
    )
    print(f"Found {len(activity_dirs)} matching activity folders: "
          f"{sorted(d.name for d in activity_dirs)}")

    for act_dir in activity_dirs:
        activity_code = act_dir.name
        is_fall = activity_code in FALL_CLASS_MAP
        target_class = FALL_CLASS_MAP.get(activity_code, "not_falling")

        csv_files = sorted(act_dir.glob("*.csv"))
        print(f"  {activity_code}: {len(csv_files)} files → {target_class}")

        for csv_path in csv_files:
            stem = csv_path.stem  # e.g. "1_FOL_1" or "sub01_FOL_trial1"
            parts = stem.split("_")

            raw = read_mobiact_csv(csv_path)
            if not raw:
                skipped += 1
                continue

            raw = decimate(raw, DECIMATE)
            imu_samples = [r[0] for r in raw]
            labels      = [r[1] for r in raw]

            if len(imu_samples) < WINDOW_SIZE:
                skipped += 1
                continue

            cls_dir = output_path / target_class
            cls_dir.mkdir(parents=True, exist_ok=True)

            # Sanitise stem for filename: keep alphanumerics and underscores
            safe_stem = "".join(c if c.isalnum() or c == "_" else "_" for c in stem)
            base = f"mobiact_{safe_stem}"

            if is_fall:
                labeled_idx = [i for i, lbl in enumerate(labels) if lbl == activity_code]
                if not labeled_idx:
                    # No annotation found — skip rather than mislabel
                    skipped += 1
                    continue
                windows = extract_fall_windows(imu_samples, labeled_idx)
                for win_idx, window in enumerate(windows):
                    write_magicwand_window(window, cls_dir / f"{base}_W{win_idx:03d}.txt")
                    stats[target_class] += 1
            else:
                # ADL: slide windows over entire file
                win_idx = 0
                for start in range(0, len(imu_samples) - WINDOW_SIZE + 1, STEP_SIZE):
                    window = imu_samples[start:start + WINDOW_SIZE]
                    write_magicwand_window(window, cls_dir / f"{base}_W{win_idx:03d}.txt")
                    stats["not_falling"] += 1
                    win_idx += 1

    return stats, skipped


def main():
    base_path  = Path(__file__).parent
    data_root  = base_path / "MobiActData"
    output_path = base_path / "processed"

    if not data_root.exists():
        print(f"Error: {data_root} not found.")
        print("Download MobiAct v2.0 from https://bmi.hmu.gr/the-mobiact-dataset-v2-0/")
        print(f"and extract to: {data_root}")
        return

    print(f"Processing MobiAct from {data_root}")
    print(f"  Decimation: every {DECIMATE}nd sample ({SOURCE_HZ}Hz -> {TARGET_HZ}Hz)")
    print(f"  Window: {WINDOW_SIZE} samples, step: {STEP_SIZE}")
    print()

    stats, skipped = process_dataset(data_root, output_path)

    print("\n=== MobiAct Processing Complete ===")
    total = sum(stats.values())
    for cls, count in stats.items():
        print(f"  {cls}: {count} windows")
    print(f"  Total: {total} windows")
    if skipped:
        print(f"  Skipped (no annotation / too short): {skipped} files")
    print(f"\nOutput: {output_path}")
    print("\nNext: run data_prepare_fall_detection.py to rebuild complete_data, then retrain.")


if __name__ == "__main__":
    main()
