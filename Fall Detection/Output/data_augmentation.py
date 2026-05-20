"""Data augmentation for fall detection training.

Augments ALL fall classes (not not_falling) with modest 2x multiplication
so compute_class_weight still sees correct relative class frequencies.
Uses numpy vectorized ops only — no slow Python loops.
"""

import numpy as np

FALL_CLASSES = {"falling_light", "falling_regular", "falling_extreme", "falling_misc"}


def augment_data(original_data, original_label):
    new_data, new_label = [], []

    for data, label in zip(original_data, original_label):
        arr = np.array(data, dtype=np.float32)  # (128, 6)
        new_data.append(data)
        new_label.append(label)

        if label not in FALL_CLASSES:
            continue

        # 1x Gaussian noise — std=50 ≈ 2.5% of raw ADC range ±2000
        noisy = arr + np.random.randn(*arr.shape).astype(np.float32) * 50.0
        new_data.append(noisy.tolist())
        new_label.append(label)

        # 1x scale variation — simulate sensor gain ±10%
        scale = np.random.uniform(0.9, 1.1)
        new_data.append((arr * scale).tolist())
        new_label.append(label)

    return new_data, new_label
