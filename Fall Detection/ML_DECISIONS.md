# TrekLink Fall Detection — ML Design Decisions

Answers the "why did you do it this way" questions for the Stage 1 ML classifier.

---

## 1. Why TFLite?

**TFLite Micro is the only viable ML framework for a microcontroller with no OS.**

The ESP32-S3 has 512 KB SRAM and runs bare-metal (no Linux, no heap allocator, no dynamic memory). TFLite Micro is built specifically for this constraint:

- **Static memory only** — the tensor arena (`uint8_t s_tensorArena[24576]`) is declared at compile time. No `malloc`, no OS.
- **INT8 quantization** — converts float32 weights to 8-bit integers. Model goes from ~100 KB → 26 KB. Inference goes from ~20 ms → ~3 ms.
- **No OS dependency** — runs directly on FreeRTOS or bare metal.
- **Espressif officially supports it** — `espressif/esp-tflite-micro` is maintained by the same team that makes the chip.

### Why not the alternatives?

| Framework | Why not |
|---|---|
| **ESP-DL** (Espressif's own) | Locked to Espressif's layer format, less community support, no INT8 pipeline as clean as TFLite |
| **ONNX Runtime** | Minimum ~2 MB RAM. Does not fit. |
| **PyTorch Mobile** | Minimum ~5 MB RAM. Does not fit. |
| **Edge Impulse** | Generates TFLite Micro code anyway. Useful for prototyping but we'd end up at the same place with less control. |
| **Raw C inference** | Possible but you'd be hand-writing matrix multiply, Conv1D, quantisation logic. TFLite Micro already does this, correctly, for free. |

**Short answer: TFLite Micro is the industry standard for "run a neural network on a microcontroller." There is no serious alternative.**

---

## 2. Why CNN? Why not XGBoost or copy a paper model?

### Why not just copy a paper model?

You cannot copy weights from a published model and deploy it. Here is why:

- **Sensor placement differs.** A model trained on wrist data will fail on hip/chest data. The acceleration signature of a fall looks completely different depending on where the sensor is.
- **Sampling rate differs.** Most papers use 100–200 Hz. We use 50 Hz. The time axis is different — the same physical fall lasts a different number of samples.
- **Sensor hardware differs.** ICM-20948 at ±16 g / ±2000 dps has different scale and noise floor than whatever the paper used.
- **Normalization differs.** Every model bakes in the training distribution (our `norm_stats_binary.json`). A foreign model's weights assume a completely different mean and std.

You can copy the **architecture** (which we did — CNN1D is standard) but you must train on your own data with your own sensor configuration.

### Why CNN over XGBoost?

XGBoost (and Random Forest, SVM, etc.) are **classical ML** — they need **hand-crafted features** fed as input:

```
mean_ax, std_ax, max_ax, min_ax, peak_g, rms_gyro, fft_bin_3, ...
```

CNN learns features from **raw time series** automatically. No feature engineering step.

| | XGBoost | CNN1D |
|---|---|---|
| Input | ~20–50 hand-crafted features | Raw 128 × 6 window |
| Feature engineering | Manual, domain expertise required | None — learned automatically |
| Model size on MCU | Potentially smaller (trees → C code via `m2cgen`) | 26 KB INT8 |
| Accuracy on this task | Competitive (~93–95% recall in papers) | 96.1% recall |
| Generalises to new datasets | Worse (features are dataset-specific) | Better |
| Training effort | Feature selection is manual work | End-to-end |

**XGBoost would work.** It is not wrong. We chose CNN because:
1. No feature engineering step to maintain
2. Slightly better recall in practice on multi-dataset training
3. TFLite Micro pipeline is already set up — switching to XGBoost would require a different deployment path (`m2cgen` → C code) with more firmware integration work

### Why CNN over LSTM?

LSTM has better memory of long-term temporal patterns but:
- 3–5× heavier than CNN1D at equivalent accuracy
- Slower inference (sequential, hard to parallelise on MCU)
- For a fixed 128-sample window (2.56 s) the temporal advantage of LSTM is minimal — CNN captures the same patterns via receptive field

---

## 3. Any other way to do this?

Yes. Several approaches exist at different points in the complexity spectrum.

### Option A — Pure physics, no ML
Use only the existing state machine: freefall threshold + impact spike + inactivity timer.
- **Pros:** Zero inference cost, fully deterministic, no training required
- **Cons:** Higher false positive rate (stairs, jumping, car bumps can all trigger it). No semantic understanding of what a "fall pattern" looks like.
- **When to use it:** If you want zero ML complexity and can tolerate more false alarms.

### Option B — Classical ML with feature engineering (XGBoost / SVM / Random Forest)
Extract ~30–50 features from the window (RMS, peak-G, spectral energy, jerk, etc.), train a classifier.
- **Pros:** Smaller model, explainable, fast inference, can be compiled to raw C with `m2cgen`
- **Cons:** Feature engineering is manual and fragile — adding a new dataset may require new features
- **When to use it:** If you need a fully auditable model (medical certification) or the CNN is too large

### Option C — Edge Impulse
Drag-and-drop TinyML platform. Upload CSV data, pick architecture, it auto-generates optimised TFLite Micro C code.
- **Pros:** Fast iteration, hardware-specific optimisation (CMSIS-NN for ESP32), built-in data management
- **Cons:** Vendor lock-in, less control over architecture and training loop, costs money at scale
- **When to use it:** Rapid prototyping or if the team has no ML background

### Option D — Pre-trained model fine-tuning
Take a published fall detection model (e.g., from the SisFall or KFall papers), replace the last layer, fine-tune on TrekLink data.
- **Pros:** Less data needed, faster convergence
- **Cons:** Original model must have same input format (same Hz, same axes, same sensor placement). Most published models do not release weights. Same deployment constraints as above.

### Option E — Server-side inference
Stream raw IMU data over BLE/WiFi/Meshtastic to a phone or server, run inference there.
- **Pros:** Unlimited model size and accuracy, easy to update
- **Cons:** Latency (a fall is over in < 2 s — you cannot afford a network round-trip), requires connectivity (TrekLink operates in wilderness with no internet), power cost of continuous radio

**We rejected Option E immediately.** Latency and connectivity make it non-viable for a wearable safety device.

---

## 4. Any other model to replace this with?

Yes. In order of how much work the swap would take:

### Drop-in replacements (same TFLite pipeline, just swap architecture)

| Model | Recall (expected) | Size | Inference | Notes |
|---|---|---|---|---|
| **CNN1D (current)** | 96.1% | 26 KB | ~3 ms | Baseline |
| **Deeper CNN1D** (more filters) | +1–2% | ~50 KB | ~6 ms | Just add Conv1D layers, retrain |
| **TCN** (Temporal Convolutional Network) | +1–3% | ~60–80 KB | ~8 ms | Dilated convolutions, better long-range patterns |
| **Bidirectional LSTM** | +1–2% | ~120 KB | ~20 ms | Heavier, slower, but better temporal memory |
| **ResNet1D** (residual blocks) | +2–4% | ~80 KB | ~10 ms | Skip connections prevent gradient issues in deeper nets |

All of these use the same TFLite Micro pipeline. Only the Keras model definition changes. Training, quantisation, and firmware integration remain identical.

### Bigger swap (different pipeline)

| Approach | Recall (expected) | Why better | Effort |
|---|---|---|---|
| **XGBoost + m2cgen** | ~93–95% | Smaller, auditable | Medium — feature engineering + new deployment path |
| **Edge Impulse auto-generated** | ~95–97% | Hardware-optimised CMSIS-NN, sometimes beats manual TFLite | Low — but vendor lock-in |
| **Ensemble (CNN + physics score)** | +2–4% | Combines ML probability with physics features | Medium — firmware logic change |

### What is NOT worth replacing

- **TFLite Micro** — no viable alternative at this MCU size class. Not worth changing.
- **INT8 quantisation** — float32 would be 4× larger and slower for no accuracy gain on this task.
- **Binary classification** — multiclass (light/regular/extreme) was tested and performs worse because the dataset labels are inconsistent across sources. The firmware severity system handles grading better than ML can.

---

## 5. What can be improved? (Same model, same framework)

### Biggest gains

**1. Better / more data** — the single highest-impact improvement.
- Current: SisFall (23 subjects) + FallAllD (30 subjects) + KFall (38 subjects) = 91 subjects, ~10K test samples
- Problem: all three datasets use hip/waist sensors. TrekLink is worn on the body but mounting position varies.
- Fix: collect 20–30 real TrekLink drop-test recordings with the actual device at the actual worn position, add them to training. Lab-collected data with matched sensor placement consistently boosts recall by 2–5% in published literature.

**2. Sliding window ensemble**
- Current: single window (most recent 128 samples after impact) → one prediction
- Improved: run inference on 3 overlapping windows (shifted by 32 samples each), take majority vote or max fall-score
- Expected gain: +1–2% recall, +3–5% precision (reduces single-window noise)
- Cost: 3× inference time (~9 ms) — still well within the 1-second window we have

**3. Data augmentation improvements**
- Current: doubles fall class with noise + shift
- Add: time-warping (stretch/compress the temporal axis), axis rotation (simulate different wear angles), magnitude scaling (±10% amplitude)
- Expected gain: +1–2% recall, better generalisation to new subjects

**4. Threshold calibration per device**
- Current: threshold = 0.50 (argmax) for all devices
- Better: run evaluate_model.py on a few real TrekLink recordings, pick threshold from the sweep table for best recall ≥ 95%
- The sweep table shows threshold 0.54 gives recall=95.3% with slightly better precision (40.2% vs 38.6%)

**5. Post-fall activity check**
- Already implemented (inactivity timer) but could feed into ML: if person is moving normally 3 s after the impact window, downweight the ML fall score
- This is a firmware logic improvement, not ML

### Diminishing returns (not worth the effort right now)

- Hyperparameter search (learning rate, batch size, filter sizes) — expected gain < 1%
- Bigger model (more parameters) — already hitting a data ceiling, more parameters won't help without more data
- Better optimiser (AdamW, LAMB) — marginal

---

## 6. What can be replaced / swapped for higher results — and should you?

### Swap 1 — Dataset (HIGHEST IMPACT, STRONGLY RECOMMENDED)
**Replace: the training dataset.  
Swap in: real TrekLink recordings.**

The current datasets (SisFall, FallAllD, KFall) were collected with research sensors at fixed lab positions. TrekLink is a wearable at a real-world position. Every percentage point of misalignment between lab sensor position and actual wear position costs recall.

- Collecting 30 subjects × 5 fall types × 3 trials = 450 recordings, takes one afternoon with volunteers
- Expected recall improvement: +2–5% recall, +10–20% precision (dramatic precision improvement because the data matches the deployment hardware exactly)
- **Should you?** Yes, eventually. This is the correct long-term path.

---

### Swap 2 — Architecture: CNN1D → ResNet1D or TCN (MEDIUM IMPACT, LOW RISK)
**Replace: flat CNN1D.  
Swap in: residual CNN or Temporal Convolutional Network.**

Residual connections (ResNet-style) let you go deeper without gradient vanishing. TCN uses dilated convolutions for larger receptive field at same parameter count.

- Same TFLite Micro pipeline — only the Keras model definition changes
- Model grows from 26 KB → ~60–80 KB (still fits easily in 4 MB flash)
- Expected gain: +1–3% recall, better generalisation
- **Should you?** Yes, worth trying on the next retrain. Low risk.
- **Should you not?** If flash space is tight (it isn't currently), or if the team can't maintain a more complex architecture.

---

### Swap 3 — Framework: TFLite Micro → Edge Impulse (LOW IMPACT, HIGH LOCK-IN)
**Replace: manual TFLite pipeline.  
Swap in: Edge Impulse auto-generated code.**

Edge Impulse uses CMSIS-NN (hardware-accelerated neural network ops on ARM/Xtensa) and auto-tunes the architecture. Sometimes beats hand-tuned TFLite by 1–2%.

- **Should you?** Only if the team lacks time to maintain the training pipeline and wants a GUI.
- **Should you not?** Vendor lock-in. If Edge Impulse changes pricing or discontinues ESP32-S3 support, you're stuck. We own the current pipeline entirely.

---

### Swap 4 — Single model → Ensemble (MEDIUM IMPACT, MEDIUM EFFORT)
**Replace: one CNN1D prediction.  
Swap in: CNN1D + physics score combined.**

The firmware already computes peak-G, freefall duration, jerk, gyro-at-impact. These are exactly the features XGBoost-style models use. Combining ML probability with a lightweight physics score (weighted sum) creates an ensemble that is more robust than either alone.

```
final_score = 0.7 × ml_fall_probability + 0.3 × normalised_physics_score
```

- **Should you?** Yes, this is the most practically achievable next improvement without new data.
- **Should you not?** Adds firmware complexity. The physics score weights need calibration per device mount position.

---

### Swap 5 — Binary → Multiclass ML (NOT RECOMMENDED)
**Replace: fall / not_fall.  
Swap in: not_falling / falling_light / falling_regular / falling_extreme.**

Tried. Falls apart because SisFall, FallAllD, and KFall label the same physical fall differently. "Falling_light" in one dataset = "falling_regular" in another. Label noise destroys multiclass accuracy.

The firmware severity system (bitmask + lethal meter) does a better job of grading severity than ML can with inconsistently labelled data.

- **Should you?** Only if you collect your own dataset with consistent labels.
- **Should you not?** With current data, recall on extreme falls drops to ~78%. Not acceptable for a safety device.

---

## Summary

| Change | Impact | Effort | Recommended? |
|---|---|---|---|
| Collect real TrekLink recordings | Very high | Medium | ✅ Yes — do this |
| ResNet1D / TCN architecture | Medium | Low | ✅ Yes — next retrain |
| Sliding window ensemble | Medium | Low (firmware) | ✅ Yes |
| Physics + ML ensemble score | Medium | Medium (firmware) | ✅ Yes |
| Edge Impulse swap | Low | Low | ⚠️ Vendor lock-in risk |
| XGBoost swap | Low | Medium | ⚠️ No clear benefit |
| Multiclass ML | Negative | High | ❌ No — use firmware severity |
| Bigger model (more params) | Low | Low | ❌ Data ceiling, won't help |
