# Lint as: python3
# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
# pylint: disable=g-bad-import-order
"""Load data from the specified paths and format them for training."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json

import numpy as np
import tensorflow as tf

from data_augmentation import augment_data

LABEL_NAME = "gesture"
DATA_NAME = "accel_ms2_xyz"  # ax, ay, az, gx, gy, gz


class DataLoader(object):
  """Loads data and prepares for training."""
  def __init__(self, train_data_path, valid_data_path, test_data_path,
               seq_length):
    self.dim = 6  # ax, ay, az, gx, gy, gz
    self.seq_length = seq_length
    self.label2id = {"not_falling": 0, "falling_light": 1, "falling_regular": 2, "falling_extreme": 3, "falling_misc": 4}
    self.train_data, self.train_label, self.train_len = self.get_data_file(
        train_data_path, "train")
    self.valid_data, self.valid_label, self.valid_len = self.get_data_file(
        valid_data_path, "valid")
    self.test_data, self.test_label, self.test_len = self.get_data_file(
        test_data_path, "test")

    # Compute normalization stats from training data only (avoid data leakage)
    self.norm_mean, self.norm_std = self._compute_norm_stats(self.train_data)
    self.train_data = self._normalize(self.train_data)
    self.valid_data = self._normalize(self.valid_data)
    self.test_data = self._normalize(self.test_data)
    self._save_norm_stats()

  def _compute_norm_stats(self, data):
    """Compute per-axis mean and std from a list of variable-length samples."""
    all_timesteps = []
    for sample in data:
      all_timesteps.extend(sample)
    arr = np.array(all_timesteps, dtype=np.float32)  # (total_timesteps, 6)
    mean = arr.mean(axis=0)
    std = arr.std(axis=0) + 1e-8  # epsilon avoids division by zero
    print(f"Norm stats — mean: {mean.round(2)}, std: {std.round(2)}")
    return mean, std

  def _normalize(self, data):
    """Z-score normalize each sample using training mean/std."""
    normalized = []
    for sample in data:
      arr = np.array(sample, dtype=np.float32)
      arr = (arr - self.norm_mean) / self.norm_std
      normalized.append(arr.tolist())
    return normalized

  def _save_norm_stats(self, path="norm_stats.json"):
    """Save normalization stats to JSON — must be hardcoded in ESP32-S3 firmware."""
    stats = {
        "axes": ["ax", "ay", "az", "gx", "gy", "gz"],
        "mean": self.norm_mean.tolist(),
        "std": self.norm_std.tolist(),
        "note": "Z-score: normalized = (raw - mean) / std. Hardcode these in firmware."
    }
    with open(path, "w") as f:
      json.dump(stats, f, indent=2)
    print(f"Normalization stats saved to {path} — copy values into firmware header.")

  def get_data_file(self, data_path, data_type):
    """Get train, valid and test data from files."""
    data = []
    label = []
    print(f"Loading {data_type} data from {data_path}...")
    with open(data_path, "r") as f:
      lines = f.readlines()
      total_lines = len(lines)
      print(f"  Found {total_lines} samples to load")
      for idx, line in enumerate(lines):  # pylint: disable=unused-variable
        if idx % 500 == 0 and idx > 0:
          print(f"  Loaded {idx}/{total_lines} samples ({idx*100//total_lines}%)")
        dic = json.loads(line)
        data.append(dic[DATA_NAME])
        label.append(dic[LABEL_NAME])
    if data_type == "train":
      print(f"  Augmenting {data_type} data...")
      data, label = augment_data(data, label)
    length = len(label)
    print(data_type + "_data_length:" + str(length))
    return data, label, length

  def pad(self, data, seq_length, dim):
    """Get neighbour padding."""
    noise_level = 10
    padded_data = []
    # Before- Neighbour padding
    tmp_data = (np.random.rand(seq_length, dim) - 0.5) * noise_level + data[0]
    tmp_data[(seq_length -
              min(len(data), seq_length)):] = data[:min(len(data), seq_length)]
    padded_data.append(tmp_data)
    # After- Neighbour padding
    tmp_data = (np.random.rand(seq_length, dim) - 0.5) * noise_level + data[-1]
    tmp_data[:min(len(data), seq_length)] = data[:min(len(data), seq_length)]
    padded_data.append(tmp_data)
    return padded_data

  def format_support_func(self, padded_num, length, data, label):
    """Support function for format.(Helps format train, valid and test.)"""
    # Add 2 padding, initialize data and label
    print(f"  Formatting {length} samples with padding...")
    length *= padded_num
    features = np.zeros((length, self.seq_length, self.dim))
    labels = np.zeros(length)
    # Get padding for train, valid and test
    original_length = len(data)
    for idx, (data, label) in enumerate(zip(data, label)):
      if idx % 500 == 0 and idx > 0:
        print(f"    Formatted {idx}/{original_length} samples ({idx*100//original_length}%)")
      padded_data = self.pad(data, self.seq_length, self.dim)
      for num in range(padded_num):
        features[padded_num * idx + num] = padded_data[num]
        labels[padded_num * idx + num] = self.label2id[label]
    print(f"  Creating TensorFlow dataset...")
    # Turn into tf.data.Dataset
    dataset = tf.data.Dataset.from_tensor_slices(
        (features, labels.astype("int32")))
    return length, dataset

  def format(self):
    """Format data(including padding, etc.) and get the dataset for the model."""
    padded_num = 2
    self.train_len, self.train_data = self.format_support_func(
        padded_num, self.train_len, self.train_data, self.train_label)
    self.valid_len, self.valid_data = self.format_support_func(
        padded_num, self.valid_len, self.valid_data, self.valid_label)
    self.test_len, self.test_data = self.format_support_func(
        padded_num, self.test_len, self.test_data, self.test_label)
