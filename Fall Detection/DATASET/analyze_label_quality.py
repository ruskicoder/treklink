"""Dataset label quality analysis and magnitude-based relabeling.

Checks whether fall severity labels match peak acceleration magnitude,
broken down by dataset. Only FallAllD and KFall have reliable magnitude
data — SisFall clips at ±2g, UMAFall is at 10Hz (misses impact spikes).

Usage:
    python analyze_label_quality.py

Outputs:
    - Per-dataset magnitude distributions
    - Clipping rate for SisFall (values near ±19.62 m/s²)
    - Suggested relabel thresholds from FallAllD + KFall only
    - relabeled_complete_data (new file) if thresholds look clean
"""

import json
import math
import os
import numpy as np
from pathlib import Path
from collections import defaultdict

COMPLETE_DATA = Path(__file__).parent.parent / "MagicWand-TFLite-ESP32-MPU6050/train/data/complete_data"
OUTPUT_PATH   = Path(__file__).parent.parent / "MagicWand-TFLite-ESP32-MPU6050/train/data/complete_data_relabeled"

FALL_CLASSES = {"falling_light", "falling_regular", "falling_extreme"}
ALL_CLASSES  = {"not_falling", "falling_light", "falling_regular", "falling_extreme"}

# SisFall ADXL345 ±2g clip limit (m/s²)
SISFALL_CLIP_MS2 = 2.0 * 9.81  # 19.62 m/s²
CLIP_THRESHOLD   = 0.95         # flag if any axis reaches 95% of clip limit

# Biomechanics-informed thresholds for relabeling (in g)
# Based on FallAllD/KFall distributions (set after running analysis)
# These are SUGGESTED — review the distribution output before accepting
RELABEL_T1_G = None  # light/regular boundary (g) — set after analysis
RELABEL_T2_G = None  # regular/extreme boundary (g) — set after analysis


def peak_mag_g(sample_data):
    """Peak acceleration magnitude over the window, in g."""
    arr = np.array(sample_data, dtype=np.float32)  # (128, 6)
    mag = np.linalg.norm(arr[:, 0:3], axis=1)       # m/s²
    return float(mag.max()) / 9.81


def is_clipped_sisfall(sample_data):
    """True if any accelerometer axis is near the ±2g clip limit."""
    arr = np.array(sample_data, dtype=np.float32)
    accel = arr[:, 0:3]
    return bool(np.any(np.abs(accel) >= CLIP_THRESHOLD * SISFALL_CLIP_MS2))


def dataset_of(name):
    return name.split("_")[0] if name else "unknown"


def main():
    print(f"Loading {COMPLETE_DATA} ...")
    if not COMPLETE_DATA.exists():
        print("ERROR: complete_data not found. Run data_prepare_fall_detection.py first.")
        return

    # Collect per-dataset peak magnitudes
    ds_peaks = defaultdict(lambda: defaultdict(list))  # ds → class → [peak_g]
    ds_clip  = defaultdict(lambda: {"clipped": 0, "total": 0})
    all_data = []

    with open(COMPLETE_DATA) as f:
        for i, line in enumerate(f):
            if i % 10000 == 0 and i > 0:
                print(f"  {i} samples processed...")
            item = json.loads(line)
            all_data.append(item)
            label = item["gesture"]
            name  = item.get("name", "")
            ds    = dataset_of(name)
            peak  = peak_mag_g(item["accel_ms2_xyz"])

            if label in ALL_CLASSES:
                ds_peaks[ds][label].append(peak)

            if ds == "sisfall" and label in FALL_CLASSES:
                ds_clip[ds]["total"] += 1
                if is_clipped_sisfall(item["accel_ms2_xyz"]):
                    ds_clip[ds]["clipped"] += 1

    print(f"\nTotal samples: {len(all_data)}")

    # -- Per-dataset distribution ----------------------------------------------
    print("\n" + "=" * 70)
    print("PEAK ACCELERATION MAGNITUDE BY DATASET AND CLASS (g)")
    print("=" * 70)

    RELIABLE_DATASETS = {"fallalld", "kfall"}
    reliable_peaks = defaultdict(list)

    for ds in sorted(ds_peaks.keys()):
        print(f"\n[{ds.upper()}]")
        is_reliable = ds in RELIABLE_DATASETS
        tag = "" if is_reliable else "  [!] UNRELIABLE (see below)"
        print(f"  Reliability: {'OK' if is_reliable else 'DEGRADED'}{tag}")

        for cls in ["falling_light", "falling_regular", "falling_extreme", "not_falling"]:
            vals = ds_peaks[ds].get(cls, [])
            if not vals:
                continue
            arr = np.array(vals)
            p = np.percentile(arr, [0, 25, 50, 75, 90, 100])
            print(f"  {cls:<20} N={len(vals):>5}  "
                  f"min={p[0]:.2f} P25={p[1]:.2f} med={p[2]:.2f} "
                  f"P75={p[3]:.2f} P90={p[4]:.2f} max={p[5]:.2f}")

            if is_reliable and cls in FALL_CLASSES:
                reliable_peaks[cls].extend(vals)

        if ds == "sisfall" and ds_clip[ds]["total"] > 0:
            rate = ds_clip[ds]["clipped"] / ds_clip[ds]["total"] * 100
            print(f"  [!] Clipping: {ds_clip[ds]['clipped']}/{ds_clip[ds]['total']} "
                  f"fall windows ({rate:.1f}%) have axis at 95%+ of +/-2g limit")

    # -- Reliable dataset combined --------------------------------------------─
    print("\n" + "=" * 70)
    print("RELIABLE DATASETS COMBINED (FallAllD + KFall only)")
    print("=" * 70)

    for cls in ["falling_light", "falling_regular", "falling_extreme"]:
        vals = reliable_peaks.get(cls, [])
        if not vals:
            print(f"  {cls}: NO DATA")
            continue
        arr = np.array(vals)
        p = np.percentile(arr, [0, 10, 25, 50, 75, 90, 100])
        print(f"  {cls:<20} N={len(vals):>5}  "
              f"P10={p[1]:.2f} P25={p[2]:.2f} med={p[3]:.2f} "
              f"P75={p[4]:.2f} P90={p[5]:.2f} max={p[6]:.2f}")

    # -- Overlap analysis on reliable data ------------------------------------─
    print("\n-- Overlap (reliable datasets only) --")
    light   = np.array(reliable_peaks.get("falling_light", []))
    regular = np.array(reliable_peaks.get("falling_regular", []))
    extreme = np.array(reliable_peaks.get("falling_extreme", []))

    if len(light) and len(regular):
        gap_lr = np.percentile(regular, 10) - np.percentile(light, 90)
        print(f"  Light P90={np.percentile(light,90):.2f}g  "
              f"Regular P10={np.percentile(regular,10):.2f}g  "
              f"gap={gap_lr:+.2f}g  ({'CLEAN' if gap_lr > 0 else 'OVERLAP'})")

    if len(regular) and len(extreme):
        gap_re = np.percentile(extreme, 10) - np.percentile(regular, 90)
        print(f"  Regular P90={np.percentile(regular,90):.2f}g  "
              f"Extreme P10={np.percentile(extreme,10):.2f}g  "
              f"gap={gap_re:+.2f}g  ({'CLEAN' if gap_re > 0 else 'OVERLAP'})")

    # -- Suggest thresholds ----------------------------------------------------
    if len(light) and len(regular) and len(extreme):
        t1 = (np.median(light) + np.median(regular)) / 2
        t2 = (np.median(regular) + np.median(extreme)) / 2

        print(f"\n-- Suggested thresholds (midpoint of medians, reliable data) --")
        print(f"  T1 (light/regular): {t1:.2f}g")
        print(f"  T2 (regular/extreme): {t2:.2f}g")

        if t2 > t1:
            print("\n  Thresholds are ordered correctly (T1 < T2). Simulating relabeling...")

            def classify(peak, t1, t2):
                if peak < t1: return "falling_light"
                if peak < t2: return "falling_regular"
                return "falling_extreme"

            for cls, vals in [("falling_light", light), ("falling_regular", regular), ("falling_extreme", extreme)]:
                if len(vals) == 0:
                    continue
                preds = [classify(p, t1, t2) for p in vals]
                correct = sum(1 for p in preds if p == cls)
                print(f"    {cls:<20}: {correct}/{len(vals)} = {100*correct/len(vals):.1f}% correct")

            # -- Write relabeled dataset --------------------------------------─
            print(f"\n-- Writing relabeled complete_data to {OUTPUT_PATH} --")
            print("  Strategy:")
            print("    SisFall + UMAFall fall windows: KEEP original labels (magnitude unreliable)")
            print("    FallAllD + KFall fall windows:  RELABEL by peak magnitude")
            print("    All not_falling windows: KEEP (labels are correct regardless)")

            relabeled_counts = defaultdict(int)
            changed = 0

            with open(OUTPUT_PATH, "w") as out:
                for item in all_data:
                    label = item["gesture"]
                    name  = item.get("name", "")
                    ds    = dataset_of(name)

                    if label in FALL_CLASSES and ds in RELIABLE_DATASETS:
                        peak = peak_mag_g(item["accel_ms2_xyz"])
                        new_label = classify(peak, t1, t2)
                        if new_label != label:
                            item = dict(item)
                            item["gesture"] = new_label
                            item["original_gesture"] = label
                            changed += 1
                    else:
                        new_label = label

                    relabeled_counts[item["gesture"]] += 1
                    out.write(json.dumps(item) + "\n")

            print(f"\n  Changed {changed} labels")
            print(f"  Final class counts: {dict(relabeled_counts)}")
            print(f"  Saved to: {OUTPUT_PATH}")
        else:
            print("\n  [!] T2 < T1 — thresholds are inverted.")
            print("  Reliable data still has overlapping magnitude distributions.")
            print("  Relabeling by peak G is NOT recommended — classes overlap even in clean data.")
            print("  Recommendation: use original scenario-based labels for binary detection only.")
            print("                  Use biomechanics thresholds at INFERENCE TIME for severity.")


if __name__ == "__main__":
    main()
