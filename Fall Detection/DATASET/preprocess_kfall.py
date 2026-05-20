#!/usr/bin/env python3
"""
Preprocess KFall dataset for TrekLink training.

Dataset location: Fall Detection/DATASET/KFall Dataset/
Structure:
  sensor_data/SA{XX}/S{XX}T{TT}R{RR}.csv  -- 100Hz IMU
  label_data/SA{XX}_label.xlsx             -- onset/impact frame per fall trial

CSV columns: TimeStamp(s), FrameCounter, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, EulerX, EulerY, EulerZ
  AccX/Y/Z: g units (at rest |acc| ≈ 1g)  -> multiply by 9.81 for m/s²
  GyrX/Y/Z: °/s  (at rest ≈ 0.4°/s; peak during fall ~300°/s confirming °/s not rad/s)
           -> multiply by π/180 for rad/s
  EulerX/Y/Z: dropped (not available on MPU6050 firmware)

Task IDs (T{XX} in filename):
  T01–T19: ADL activities -> not_falling
  T20–T34: Falls F01–F15 -> use label xlsx for onset/impact frame windowing
  T35–T36: Additional ADL -> not_falling

Fall windowing: extract 1–2 windows centered on the fall event using onset/impact annotations.
ADL windowing: slide 128-sample windows over entire file, 50% overlap.

Output: Fall Detection/DATASET/processed/<class>/kfall_<name>.txt
"""

import csv
import math
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

SOURCE_HZ  = 100
TARGET_HZ  = 50
DECIMATE   = SOURCE_HZ // TARGET_HZ  # = 2

WINDOW_SIZE = 128
STEP_SIZE   = 64

G       = 9.81
DEG2RAD = math.pi / 180.0

# Task IDs that are ADL recordings (entire file = not_falling)
ADL_TASK_IDS = set(range(1, 20)) | {35, 36}

# Fall task ID -> our severity class (physics-based: fall height + controlledness)
FALL_TASK_MAP = {
    20: "falling_regular",  # F01: Forward fall trying to sit — chair height, partial control
    21: "falling_regular",  # F02: Backward fall trying to sit
    22: "falling_regular",  # F03: Lateral fall trying to sit
    23: "falling_regular",  # F04: Forward fall trying to get up
    24: "falling_regular",  # F05: Lateral fall trying to get up
    25: "falling_regular",  # F06: Forward fall while seated, fainting — already low height
    26: "falling_regular",  # F07: Lateral fall while seated, fainting
    27: "falling_regular",  # F08: Backward fall while seated, fainting
    28: "falling_extreme",  # F09: Vertical fall while walking, fainting — standing height + dead weight
    29: "falling_regular",  # F10: Fall walking, dampened by table — table absorbs most impact
    30: "falling_light",    # F11: Walking trip — partial control, aligns with SisFall F04 / FallAllD 101
    31: "falling_regular",  # F12: Jogging trip — higher kinetic energy than walking trip
    32: "falling_regular",  # F13: Walking slip forward
    33: "falling_regular",  # F14: Walking slip forward-lateral
    34: "falling_regular",  # F15: Walking slip backward
}


def read_label_xlsx(xlsx_path):
    """Parse KFall label xlsx, return {task_id: {trial_id: (onset_frame, impact_frame)}}.

    Xlsx has no shared-strings table — cells use inline text or numeric values.
    Task code cell format: 'F01 (20)' where 20 is the numeric task ID.
    """
    labels = {}
    with zipfile.ZipFile(xlsx_path) as z:
        xml_data = z.read('xl/worksheets/sheet1.xml')
    root = ET.fromstring(xml_data)
    ns = {'x': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}

    current_task_id = None
    for row in root.findall('.//x:row', ns):
        vals = []
        for cell in row.findall('x:c', ns):
            v = cell.find('x:v', ns)
            t = cell.find('x:is/x:t', ns)
            vals.append(v.text if v is not None else (t.text if t is not None else None))

        if not vals:
            continue

        # Header row — skip
        if vals[0] and 'Task' in str(vals[0]):
            continue

        # New task block: cell 0 contains "F01 (20)" etc.
        if vals[0]:
            raw = str(vals[0]).strip('﻿').strip()
            try:
                current_task_id = int(raw.split('(')[1].split(')')[0])
                labels.setdefault(current_task_id, {})
            except (IndexError, ValueError):
                pass

        if current_task_id is None or len(vals) < 5:
            continue

        try:
            trial_id = int(float(vals[2])) if vals[2] else None
            onset    = int(float(vals[3])) if vals[3] else None
            impact   = int(float(vals[4])) if vals[4] else None
            if trial_id is not None and onset is not None and impact is not None:
                labels[current_task_id][trial_id] = (onset, impact)
        except (ValueError, TypeError):
            continue

    return labels


def read_kfall_csv(filepath):
    """Read KFall CSV, convert to [ax_m/s², ay, az, gx_rad/s, gy, gz] per row."""
    samples = []
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                samples.append([
                    float(row['AccX']) * G,   float(row['AccY']) * G,   float(row['AccZ']) * G,
                    float(row['GyrX']) * DEG2RAD, float(row['GyrY']) * DEG2RAD, float(row['GyrZ']) * DEG2RAD,
                ])
            except (KeyError, ValueError):
                continue
    return samples


def decimate(samples, factor):
    return [samples[i] for i in range(0, len(samples), factor)]


def write_magicwand_window(window, output_file):
    with open(output_file, 'w') as f:
        f.write('-.-.-\n')
        for row in window:
            f.write(','.join(f'{v:.6f}' for v in row) + '\n')
        f.write('\n')


def extract_fall_windows(samples, onset_raw, impact_raw):
    """Return 1–2 windows around the fall event.

    Onset/impact are in original 100Hz frames; scale by 1/DECIMATE for 50Hz.
    Primary window: centered on (onset+impact)//2.
    Second window: onset-aligned, added only when fall spans > 64 decimated frames.
    """
    onset  = onset_raw  // DECIMATE
    impact = impact_raw // DECIMATE
    n = len(samples)
    windows = []

    # Primary: center on fall midpoint
    center = (onset + impact) // 2
    start = max(0, min(center - WINDOW_SIZE // 2, n - WINDOW_SIZE))
    if 0 <= start and start + WINDOW_SIZE <= n:
        windows.append(samples[start:start + WINDOW_SIZE])

    # Secondary: onset-anchored window if the fall is long and it differs meaningfully
    if (impact - onset) > WINDOW_SIZE // 2:
        start2 = max(0, min(onset - 16, n - WINDOW_SIZE))
        if start2 + WINDOW_SIZE <= n and abs(start2 - start) > STEP_SIZE:
            windows.append(samples[start2:start2 + WINDOW_SIZE])

    return windows


def process_dataset(sensor_path, label_path, output_path):
    stats = {cls: 0 for cls in ["not_falling", "falling_light", "falling_regular", "falling_extreme"]}

    subject_dirs = sorted(d for d in sensor_path.iterdir() if d.is_dir())
    print(f"Found {len(subject_dirs)} subject folders.")

    for subj_dir in subject_dirs:
        subj_name = subj_dir.name  # e.g. SA06
        label_file = label_path / f"{subj_name}_label.xlsx"
        labels = {}
        if label_file.exists():
            try:
                labels = read_label_xlsx(label_file)
            except Exception as e:
                print(f"  Warning: could not read labels for {subj_name}: {e}")

        for csv_path in sorted(subj_dir.glob('*.csv')):
            stem = csv_path.stem  # S06T20R01
            try:
                t_idx = stem.index('T')
                r_idx = stem.index('R')
                task_id  = int(stem[t_idx + 1:r_idx])
                trial_id = int(stem[r_idx + 1:])
            except (ValueError, AttributeError):
                continue

            if task_id in ADL_TASK_IDS:
                target_class = "not_falling"
            elif task_id in FALL_TASK_MAP:
                target_class = FALL_TASK_MAP[task_id]
            else:
                continue

            samples = decimate(read_kfall_csv(csv_path), DECIMATE)
            if len(samples) < WINDOW_SIZE:
                continue

            cls_dir = output_path / target_class
            cls_dir.mkdir(parents=True, exist_ok=True)
            base = f"kfall_{subj_name}_T{task_id:02d}_R{trial_id:02d}"

            if target_class == "not_falling":
                for win_idx, start in enumerate(range(0, len(samples) - WINDOW_SIZE + 1, STEP_SIZE)):
                    write_magicwand_window(samples[start:start + WINDOW_SIZE],
                                           cls_dir / f"{base}_W{win_idx:03d}.txt")
                    stats["not_falling"] += 1
            else:
                fall_info = labels.get(task_id, {}).get(trial_id)
                if fall_info is None:
                    continue  # no label for this trial — skip rather than mislabel
                for win_idx, window in enumerate(extract_fall_windows(samples, *fall_info)):
                    write_magicwand_window(window, cls_dir / f"{base}_W{win_idx:03d}.txt")
                    stats[target_class] += 1

    return stats


def main():
    base_path   = Path(__file__).parent
    sensor_path = base_path / "KFall Dataset" / "sensor_data"
    label_path  = base_path / "KFall Dataset" / "label_data"
    output_path = base_path / "processed"

    if not sensor_path.exists():
        print(f"Error: {sensor_path} not found.")
        return

    print(f"Processing KFall from {sensor_path}")
    print(f"  Decimation: every {DECIMATE}nd sample ({SOURCE_HZ}Hz -> {TARGET_HZ}Hz)")
    print(f"  Window: {WINDOW_SIZE} samples, step: {STEP_SIZE}")
    print()

    stats = process_dataset(sensor_path, label_path, output_path)

    print("\n=== KFall Processing Complete ===")
    total = sum(stats.values())
    for cls, count in stats.items():
        print(f"  {cls}: {count} windows")
    print(f"  Total: {total} windows")
    print(f"\nOutput: {output_path}")


if __name__ == "__main__":
    main()
