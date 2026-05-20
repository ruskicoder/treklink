"""Build and train neural networks for fall detection.

5-class fall detection:
0 - not_falling
1 - falling_light
2 - falling_regular
3 - falling_extreme
4 - falling_misc
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import datetime
import os
from data_load import DataLoader

import numpy as np
import tensorflow as tf
from keras.callbacks import ModelCheckpoint
from sklearn.utils.class_weight import compute_class_weight

logdir = "logs/scalars/" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
tensorboard_callback = tf.keras.callbacks.TensorBoard(log_dir=logdir)

NUM_CLASSES = 5  # 5 fall detection classes


def reshape_function(data, label):
    reshaped_data = tf.reshape(data, [-1, 6, 1])
    return reshaped_data, label


def calculate_model_size(model):
    print(model.summary())
    try:
        var_sizes = []
        for v in model.trainable_variables:
            shape_size = np.prod(list(map(int, v.shape)))
            # Try to get dtype size in multiple ways
            dtype_size = 4  # Default to 4 bytes (float32)
            
            if hasattr(v.dtype, 'itemsize'):
                dtype_size = v.dtype.itemsize
            elif hasattr(v.dtype, 'size'):
                dtype_size = v.dtype.size
            elif str(v.dtype) == 'float32':
                dtype_size = 4
            elif str(v.dtype) == 'float64':
                dtype_size = 8
            elif str(v.dtype) == 'int32':
                dtype_size = 4
            elif str(v.dtype) == 'int64':
                dtype_size = 8
                
            var_sizes.append(shape_size * dtype_size)
        print("Model size:", sum(var_sizes) / 1024, "KB")
    except Exception as e:
        print(f"Could not calculate model size: {e}")
        print("Continuing with training...")


def build_cnn(seq_length):
    """Builds a convolutional neural network in Keras.

    Input shape: (seq_length, 6, 1) — 6 axes: ax, ay, az, gx, gy, gz.
    Target size: < 50 KB quantized (ref paper A1: 9,623 bytes achieved).
    """
    model = tf.keras.Sequential([
        # (batch, 128, 6, 1) → (batch, 128, 6, 8)
        tf.keras.layers.Conv2D(8, (4, 3), padding="same", activation="relu",
                               input_shape=(seq_length, 6, 1)),
        # (batch, 128, 6, 8) → (batch, 42, 2, 8)
        tf.keras.layers.MaxPool2D((3, 3)),
        tf.keras.layers.Dropout(0.1),
        # (batch, 42, 2, 8) → (batch, 42, 2, 16)
        tf.keras.layers.Conv2D(16, (4, 1), padding="same", activation="relu"),
        # (batch, 42, 2, 16) → (batch, 14, 2, 16)
        tf.keras.layers.MaxPool2D((3, 1), padding="same"),
        tf.keras.layers.Dropout(0.1),
        tf.keras.layers.Flatten(),       # (batch, 448)
        tf.keras.layers.Dense(64, activation="relu"),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(NUM_CLASSES, activation="softmax"),
    ])
    model_path = os.path.join("./netmodels", "CNN_fall_detection")
    print("Built CNN for fall detection.")
    if not os.path.exists(model_path):
        os.makedirs(model_path)
    if os.path.exists(os.path.join(model_path, 'weights.h5')):
        print('Loading previous weights')
        model.load_weights(os.path.join(model_path, 'weights.h5'))
    return model, model_path


def build_lstm(seq_length):
    """Builds a Bidirectional LSTM in Keras. Slower on MCU than CNN — use CNN first."""
    model = tf.keras.Sequential([
        tf.keras.layers.Bidirectional(
            tf.keras.layers.LSTM(22),
            input_shape=(seq_length, 6)),  # 6 axes: ax, ay, az, gx, gy, gz
        tf.keras.layers.Dense(NUM_CLASSES, activation="softmax"),
    ])
    model_path = os.path.join("./netmodels", "LSTM_fall_detection")
    print("Built LSTM for fall detection.")
    if not os.path.exists(model_path):
        os.makedirs(model_path)
    return model, model_path


def load_data(train_data_path, valid_data_path, test_data_path, seq_length):
    data_loader = DataLoader(train_data_path,
                             valid_data_path,
                             test_data_path,
                             seq_length=seq_length)
    data_loader.format()

    # Compute class weights from augmented training labels.
    # falling_regular (2) and falling_extreme (3) get extra boost —
    # a missed lethal fall is far worse than a false alarm.
    label_ids = np.array([data_loader.label2id[l] for l in data_loader.train_label])
    base_weights = compute_class_weight("balanced", classes=np.arange(NUM_CLASSES), y=label_ids)
    class_weight_dict = {i: float(w) for i, w in enumerate(base_weights)}
    class_weight_dict[2] *= 2.0  # falling_regular
    class_weight_dict[3] *= 2.5  # falling_extreme
    print("\nClass weights:", {k: f"{v:.2f}" for k, v in class_weight_dict.items()})

    return data_loader.train_len, data_loader.train_data, data_loader.valid_len, \
        data_loader.valid_data, data_loader.test_len, data_loader.test_data, class_weight_dict


def build_net(args, seq_length):
    if args.model == "CNN":
        model, model_path = build_cnn(seq_length)
    elif args.model == "LSTM":
        model, model_path = build_lstm(seq_length)
    else:
        print("Please input correct model name (CNN or LSTM)")
        exit(1)
    return model, model_path


CLASS_NAMES = ["not_falling", "falling_light", "falling_regular", "falling_extreme", "falling_misc"]


def eval_net(model, test_data, test_labels):
    print("********** Evaluation test data **********")
    loss, acc = model.evaluate(test_data)
    pred = np.argmax(model.predict(test_data), axis=1)
    confusion = tf.math.confusion_matrix(labels=tf.constant(test_labels),
                                         predictions=tf.constant(pred),
                                         num_classes=NUM_CLASSES)
    print("\nConfusion Matrix (rows=true, cols=predicted):")
    print("  " + "  ".join(f"{n[:6]:>6}" for n in CLASS_NAMES))
    cm = confusion.numpy()
    for i, row in enumerate(cm):
        print(f"{CLASS_NAMES[i][:6]:>6}  " + "  ".join(f"{v:>6}" for v in row))

    print(f"\nOverall — Loss: {loss:.4f}, Accuracy: {acc:.4f}")
    print("\nPer-class Sensitivity (Recall) — PRIORITY METRIC:")
    for i, name in enumerate(CLASS_NAMES):
        tp = cm[i, i]
        total = cm[i, :].sum()
        sensitivity = tp / total if total > 0 else 0.0
        flag = " <<< TARGET > 95%" if i in (2, 3) else ""
        print(f"  {name}: {sensitivity:.4f}{flag}")


def prepare_and_save_tflite_nets(model, filename, filename_quantized, rep_data=None):
    """Convert model to TFLite float32 and full INT8 quantized versions.

    rep_data: numpy array of shape (N, seq_length, 6, 1) used for INT8 calibration.
              Required for full integer quantization — without it INT8 accuracy degrades.
    """
    # Float32 baseline
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()
    open(filename, "wb").write(tflite_model)

    # Full INT8 quantization with representative dataset calibration.
    # This is required for TFLite Micro on ESP32-S3 — dynamic range only is insufficient.
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    if rep_data is not None:
        def representative_dataset():
            for i in range(min(200, len(rep_data))):
                sample = rep_data[i:i+1].astype(np.float32)
                yield [sample]
        converter.representative_dataset = representative_dataset
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8
        print("Full INT8 quantization with representative dataset.")
    else:
        print("Warning: no rep_data — falling back to dynamic range quantization.")
        print("  Pass rep_data for proper INT8 required by TFLite Micro on ESP32-S3.")

    tflite_model = converter.convert()
    open(filename_quantized, "wb").write(tflite_model)

    basic_model_size = os.path.getsize(filename)
    quantized_model_size = os.path.getsize(filename_quantized)
    print(f"Float32 model: {basic_model_size:,} bytes")
    print(f"INT8 quantized: {quantized_model_size:,} bytes  (target < 50,000)")
    print(f"Size reduction: {basic_model_size - quantized_model_size:,} bytes ({(1 - quantized_model_size/basic_model_size)*100:.1f}%)")


def train_net(
        model,
        model_path,
        train_data,
        valid_len,
        valid_data,
        test_len,
        test_data,
        kind,
        class_weight_dict,
        weights_metric="val_accuracy"):
    """Trains the model."""
    calculate_model_size(model)
    epochs = 100
    batch_size = 64
    model.compile(optimizer="adam",
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])

    if kind == "CNN":
        train_data = train_data.map(reshape_function)
        valid_data = valid_data.map(reshape_function)
        test_data = test_data.map(reshape_function)

    # Collect test labels and a calibration batch for INT8 quantization before batching.
    test_labels = np.zeros(test_len)
    rep_samples = []
    for idx, (features, label) in enumerate(test_data):
        test_labels[idx] = label.numpy()
        if len(rep_samples) < 200:
            rep_samples.append(features.numpy())
    rep_data = np.stack(rep_samples, axis=0) if rep_samples else None  # (N, seq, 6, 1) for CNN

    train_data = train_data.batch(batch_size).repeat()
    valid_data = valid_data.batch(batch_size)
    test_data = test_data.batch(batch_size)

    chckpt_path = os.path.join(model_path, 'weights_best_' + weights_metric + '.weights.h5')
    mode = "min" if weights_metric in ("val_loss", "loss") else "max"
    chckpt_fct = ModelCheckpoint(chckpt_path, monitor=weights_metric,
                                 save_best_only=True, mode=mode, save_weights_only=True)

    model.fit(train_data,
              epochs=epochs,
              validation_data=valid_data,
              steps_per_epoch=1000,
              validation_steps=int((valid_len - 1) / batch_size + 1),
              class_weight=class_weight_dict,
              callbacks=[tensorboard_callback, chckpt_fct])

    print("*" * 80)
    print("Model after last epoch:")
    eval_net(model, test_data, test_labels)
    model.save(os.path.join(model_path, 'weights.h5'))
    prepare_and_save_tflite_nets(
        model, "model_fall_detection.tflite",
        "model_fall_detection_quantized.tflite", rep_data)

    print("*" * 80)
    print("Model of epoch with best " + weights_metric + ":")
    model.load_weights(chckpt_path)
    eval_net(model, test_data, test_labels)
    model.save(os.path.join(model_path, 'weights_best_' + weights_metric + '.weights.h5'))
    prepare_and_save_tflite_nets(
        model,
        f"model_fall_detection_best_{weights_metric}.tflite",
        f"model_fall_detection_quantized_best_{weights_metric}.tflite",
        rep_data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", "-m", required=True, choices=["CNN", "LSTM"], 
                        help="Model type: CNN or LSTM")
    parser.add_argument("--person", "-p", default="false", 
                        help="Use person-based split (true/false)")
    parser.add_argument("--weights_metric", default="val_accuracy", 
                        choices=["val_accuracy", "val_loss", "accuracy", "loss"],
                        help="Metric to use for saving best weights")
    args = parser.parse_args()

    seq_length = 128

    print("Start to load data...")
    if args.person == "true":
        train_len, train_data, valid_len, valid_data, test_len, test_data, class_weight_dict = \
            load_data("./person_split/train", "./person_split/valid",
                      "./person_split/test", seq_length)
    else:
        train_len, train_data, valid_len, valid_data, test_len, test_data, class_weight_dict = \
            load_data("./data/train", "./data/valid", "./data/test", seq_length)

    print("Start to build net...")
    model, model_path = build_net(args, seq_length)

    print("Start training...")
    train_net(model, model_path, train_data, valid_len, valid_data,
              test_len, test_data, args.model, class_weight_dict, args.weights_metric)

    print("Training finished!")
