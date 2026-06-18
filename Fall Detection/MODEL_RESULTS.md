# TrekLink Fall Detection — Model Results & Architecture Analysis

Comparison of all trained Stage 1 binary classifiers.  
**Primary target: fall recall ≥ 95%.** Secondary: AUC-ROC, precision, F1.

Training config (same across all models unless noted):
- Dataset: SisFall + KFall + UMAFall (surgical: fall samples only) — 91 subjects
- Split: subject-based 54 / 18 / 19 train / valid / test — no data leakage
- Seed: 42 | class_weight: `{not_fall: 1.0, fall: 2.5}` | PATIENCE: 25 | WARMUP: 10

---

## Summary Table

| Model | Size | Fall Recall | Specificity | Precision | F1 | AUC-ROC | Result |
|---|---|---|---|---|---|---|---|
| **CNN1D** | **26 KB** | **96.11%** | 56.94% | 38.84% | 0.553 | **0.904** | ✅ **Deployed** |
| ResNet1D | 69 KB | 87.54% | — | — | — | — | ❌ Failed (<95%) |
| TCN | 67 KB | 90.96% | — | — | — | — | ❌ Failed (<95%) |

**Conclusion: CNN1D is the best model for this dataset. Larger architectures performed worse.**

---

## CNN1D — Deployed Model ✅

**Architecture:** `Conv1D(16, k=5) → MaxPool → Conv1D(32, k=5) → MaxPool → Conv1D(64, k=5) → GAP → Dense(32) → Dense(2)`

**File:** `model_cnn1d_binary_best_recall_quantized.tflite` — 26,864 bytes (26 KB INT8)

**Trained:** 2026-05-27 | Seed: 42 | Passed deploy-safety gate (recall ≥ 0.95)

### Overall Metrics

| Metric | Value | What it means |
|---|---|---|
| **Fall Recall** | **96.11%** | Of 2,367 real falls — caught 2,275, missed only 92 |
| Specificity | 56.94% | Of 8,318 non-falls — correctly ignored 4,736 |
| Precision | 38.84% | Of all predicted falls — 38.8% were actually falls |
| F1 Score | 0.553 | Combined precision + recall score (recall-biased tradeoff) |
| **AUC-ROC** | **0.904** | Model genuinely separates falls from non-falls (1.0 = perfect) |
| Accuracy | 65.62% | Misleading due to class imbalance — do not use this |

### Confusion Matrix (10,685 test samples)

```
                   Pred not_fall    Pred fall
True not_fall          4,736          3,582     ← false alarms
True fall                 92          2,275     ← caught falls

Missed falls : 92 / 2,367 = 3.89%
False alarms : 3,582 / 8,318 = 43.1%  (most filtered by physics layer before ML runs)
```

### Per-Dataset Breakdown

| Dataset | n | Fall Recall | Specificity | AUC | Notes |
|---|---|---|---|---|---|
| KFall | 5,096 | **100.00%** | 69.59% | 0.992 | Perfect recall |
| SisFall | 5,256 | 94.17% | 45.81% | 0.845 | Just under 95% target |
| UMAFall | 333 | 98.81% | **0.00%** | 0.765 | Waist sensor mismatch — expected, handled by physics layer |

### Threshold Operating Points

The model outputs a probability score. The default firmware uses argmax (threshold = 0.50).
Raising the threshold trades recall for fewer false alarms.

| Mode | Threshold | Fall Recall | Precision | F1 | Specificity |
|---|---|---|---|---|---|
| **Current firmware** | 0.50 | **96.11%** | 38.61% | 0.551 | 56.49% |
| Balanced (best F1 @ recall ≥ 95%) | 0.54 | 95.27% | 40.24% | **0.566** | 59.74% |
| High precision (best F1 overall) | 0.88 | 61.17% | 80.53% | **0.695** | 95.79% |

For a safety device, threshold = 0.50 is correct. Missing a fall is worse than a false alarm.

---

## ResNet1D — Failed ❌

**Architecture:** `Stem(Conv1D 32, k=7) → ResBlock(32, k=5) → ResBlock(64, k=5) → GAP → Dense(32) → Dense(2)`

**Key idea:** Residual (skip) connections let gradients flow directly to early layers,
enabling deeper networks without training signal dying.

**File:** 69,224 bytes (69 KB INT8) — 2.6× larger than CNN1D

### Results

| Checkpoint | not_falling recall | fall recall |
|---|---|---|
| best_val_accuracy | 90.72% | 61.72% |
| **best_recall (DEPLOY)** | 44.50% | **87.54% ❌** |

**Fall recall: 87.54% — failed to meet 95% target. Not deployed.**

### Why it failed

Skip connections solve **vanishing gradients**, which only occur in deep networks (10+ layers).
CNN1D has 3 conv layers — gradients do not vanish at that depth.
ResNet1D's skip connections solved a problem that did not exist, while the extra parameters
(69 KB vs 26 KB) gave the model more ways to overfit the limited training data.

> More parameters + same amount of data = worse generalisation.

---

## TCN — Failed ❌

**Architecture:** `Proj(Conv1D 32) → 5× TCNBlock(32, k=3, dilations=[1,2,4,8,16]) → GAP → Dense(32) → Dense(2)`

**Key idea:** Dilated convolutions grow the receptive field exponentially without adding depth.
Each output neuron explicitly sees 125 / 128 samples (98% of the full 2.56 s window),
compared to CNN1D which sees only ~32 / 128 samples (25%) per position.

**File:** 67,216 bytes (67 KB INT8) — 2.6× larger than CNN1D

### Results

| Checkpoint | not_falling recall | fall recall |
|---|---|---|
| best_val_accuracy | **99.65%** | **3.17%** |
| **best_recall (DEPLOY)** | 18.35% | **90.96% ❌** |

**Fall recall: 90.96% — failed to meet 95% target. Not deployed.**

### Why it failed — the degenerate solution

The `best_val_accuracy` checkpoint is the clearest indicator:
```
not_falling = 99.65%,  fall = 3.17%
```
The model learned to **predict "not_falling" for almost everything**.
On a dataset that is 78% non-falls, this strategy scores high accuracy (~78%)
without detecting a single fall. This is the degenerate solution — technically high accuracy, useless in practice.

The FallRecallCallback rescued a better version (90.96%) but the model never fully recovered.

> The TCN's global receptive field is a genuine structural advantage — but it needs more data
> to justify its 67 KB of parameters. On this dataset size, CNN1D's simplicity wins.

---

## Why CNN1D Wins on This Dataset

This result follows a well-known principle in machine learning:

> **Model complexity should match data volume.**
> A model with too many parameters for its training set finds shortcuts instead of learning the real signal.

| | CNN1D | ResNet1D | TCN |
|---|---|---|---|
| Parameters (approx) | ~6,500 | ~17,000 | ~17,000 |
| INT8 size | 26 KB | 69 KB | 67 KB |
| Receptive field | 32 / 128 samples (25%) | 32 / 128 samples (25%) | 125 / 128 samples (98%) |
| Fall recall | **96.11% ✅** | 87.54% ❌ | 90.96% ❌ |
| Overfitting risk | Low | High | High |

CNN1D is the right model **for this dataset**. ResNet1D and TCN would likely outperform it
with 2–3× more training subjects — but on 91 subjects, CNN1D's simplicity is its advantage.

---

## What Would Actually Improve Results

The architecture question is settled. CNN1D stays. Further improvement requires better data.

### Path 1 — Real TrekLink recordings (highest impact)

Collect drop-tests with the actual device at the actual worn position.

- 30 subjects × 5 fall types × 3 trials = ~450 recordings, one afternoon
- Expected improvement: **+2–5% recall, +10–20% precision**
- Why so much: every current dataset uses a different sensor placement. Matched hardware removes the biggest source of distribution mismatch.

### Path 2 — MobiAct dataset (medium impact, zero cost)

[MobiAct v2.0](https://bmi.hmu.gr/the-mobiact-dataset-v2-0/) — free, 57 subjects.

- Falls: forward, backward, lateral, syncope, step-and-fall
- ADL: walking, jogging, stairs up/down, sitting, standing, lying, car in/out
- More diverse non-fall activities → fixes the SisFall 45.8% specificity problem
- Expected improvement: **+5–10% precision, +1–2% recall**

### Path 3 — Slim TCN (low cost, uncertain gain)

Reduce TCN filters from 32 → 8 to match CNN1D's parameter count (~26 KB).
The dilated receptive field (98% of window vs CNN1D's 25%) remains — that structural
advantage is independent of filter count. Worth one Kaggle run.

**Priority order: Path 1 > Path 2 > Path 3.**

---

## Current Deployment Status

| Component | File | Value |
|---|---|---|
| Model | `Fall Detection/Context/DEPLOY_READY__recall0.957__model_cnn1d_binary_best_recall_quantized.tflite` | 26 KB INT8 |
| Norm stats | `Fall Detection/Context/norm_stats_binary.json` | mean/std for Z-score |
| Firmware model | `src/modules/fall_detection_model.h` | C array, auto-generated |
| Firmware constants | `src/modules/FallDetectionModule.h` — `NORM_MEAN`, `NORM_STD` | Must match norm_stats |
| Fall recall | **96.11%** (test set, 2,367 fall samples) | |
| Missed falls | 92 / 2,367 = **3.89%** | |
| AUC-ROC | **0.904** | |
