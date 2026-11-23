#!/usr/bin/env python3
"""
Cricket Swing Detection Model Training
=======================================
Trains a Temporal Convolutional Network (TCN) for on-device swing phase classification,
contact time regression, shot type classification, and bat speed estimation.

Datasets used:
- CricketVision (video + pose): https://github.com/dvanderhaar/uj-aqa-cricketvision
- Cricket Shot Classification: https://huggingface.co/datasets/rokmr/cricket-shot
- Cricket Dataset (Kaggle): https://www.kaggle.com/datasets/sureshmaheshwari021/cricket-dataset
- AthletePose3D: https://github.com/calvinyeungck/AthletePose3D
- CMU MoCap: http://mocap.cs.cmu.edu/ (cricket motions)

Model Architecture:
- Input: 2-second window @ 100Hz = 200 timesteps × 9 channels (accel x,y,z + gyro x,y,z + mag x,y,z)
- Backbone: TCN with dilated convolutions (receptive field covers full window)
- Heads: Multi-task (phase classification, contact regression, shot type, bat speed, wrist rotation)
- Output: CoreML (.mlmodel) for iOS, TFLite (.tflite) for Android
"""

import os
import json
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader, random_split
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
import coremltools as ct
import tensorflow as tf
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import warnings
warnings.filterwarnings('ignore')

# ============================================================================
# CONFIGURATION
# ============================================================================

CONFIG = {
    # Data
    'window_size': 200,           # 2 seconds @ 100Hz
    'num_channels': 9,            # accel(3) + gyro(3) + mag(3)
    'sampling_rate': 100,         # Hz

    # Model
    'tcn_channels': [64, 128, 256, 256],
    'kernel_size': 3,
    'dropout': 0.2,
    'num_phases': 7,              # Idle, Backswing, Transition, Downswing, Contact, FollowThrough, Recovery
    'num_shot_types': 12,         # Defensive, Drive, Cut, Pull, Hook, Sweep, Reverse, Ramp, LoftedDrive, LoftedOn, LateCut, Glance

    # Training
    'batch_size': 64,
    'learning_rate': 1e-3,
    'weight_decay': 1e-4,
    'epochs': 100,
    'patience': 15,
    'device': 'cuda' if torch.cuda.is_available() else 'cpu',

    # Loss weights
    'phase_weight': 1.0,
    'contact_weight': 2.0,
    'shot_weight': 1.5,
    'speed_weight': 1.0,
    'wrist_weight': 0.5,

    # Export
    'coreml_min_deployment_target': 'iOS15',
    'tflite_optimize': True,
}

# Shot type labels (must match mobile app)
SHOT_TYPES = [
    'Defensive', 'StraightDrive', 'CoverDrive', 'SquareDrive',
    'Cut', 'LateCut', 'Pull', 'Hook',
    'Sweep', 'ReverseSweep', 'Ramp', 'LoftedDrive',
    'LoftedOnSide', 'Glance', 'Flick', 'Block'
]

SWING_PHASES = [
    'Idle', 'Backswing', 'Transition', 'Downswing',
    'Contact', 'FollowThrough', 'Recovery'
]


# ============================================================================
# MODEL ARCHITECTURE
# ============================================================================

class Chomp1d(nn.Module):
    """Remove padding from the end of temporal dimension"""
    def __init__(self, chomp_size):
        super().__init__()
        self.chomp_size = chomp_size

    def forward(self, x):
        return x[:, :, :-self.chomp_size].contiguous()


class TemporalBlock(nn.Module):
    """Dilated causal convolution block for TCN"""
    def __init__(self, in_channels, out_channels, kernel_size, stride, dilation, padding, dropout=0.2):
        super().__init__()
        self.conv1 = nn.Conv1d(in_channels, out_channels, kernel_size,
                               stride=stride, padding=padding, dilation=dilation)
        self.chomp1 = Chomp1d(padding)
        self.relu1 = nn.ReLU()
        self.dropout1 = nn.Dropout(dropout)

        self.conv2 = nn.Conv1d(out_channels, out_channels, kernel_size,
                               stride=stride, padding=padding, dilation=dilation)
        self.chomp2 = Chomp1d(padding)
        self.relu2 = nn.ReLU()
        self.dropout2 = nn.Dropout(dropout)

        self.net = nn.Sequential(
            self.conv1, self.chomp1, self.relu1, self.dropout1,
            self.conv2, self.chomp2, self.relu2, self.dropout2
        )

        self.downsample = nn.Conv1d(in_channels, out_channels, 1) if in_channels != out_channels else None
        self.relu = nn.ReLU()
        self.init_weights()

    def init_weights(self):
        for m in [self.conv1, self.conv2]:
            nn.init.kaiming_normal_(m.weight, mode='fan_out', nonlinearity='relu')
        if self.downsample is not None:
            nn.init.kaiming_normal_(self.downsample.weight, mode='fan_out', nonlinearity='relu')

    def forward(self, x):
        out = self.net(x)
        res = x if self.downsample is None else self.downsample(x)
        return self.relu(out + res)


class TemporalConvNet(nn.Module):
    """TCN backbone with exponentially increasing dilation"""
    def __init__(self, num_inputs, num_channels, kernel_size=3, dropout=0.2):
        super().__init__()
        layers = []
        num_levels = len(num_channels)

        for i in range(num_levels):
            dilation = 2 ** i
            in_channels = num_inputs if i == 0 else num_channels[i - 1]
            out_channels = num_channels[i]
            padding = (kernel_size - 1) * dilation

            layers.append(TemporalBlock(in_channels, out_channels, kernel_size,
                                        stride=1, dilation=dilation, padding=padding, dropout=dropout))

        self.network = nn.Sequential(*layers)
        self.output_channels = num_channels[-1]

    def forward(self, x):
        return self.network(x)


class CricketSwingModel(nn.Module):
    """Multi-task cricket swing detection model"""
    def __init__(self, config):
        super().__init__()
        self.config = config

        # TCN Backbone
        self.tcn = TemporalConvNet(
            num_inputs=config['num_channels'],
            num_channels=config['tcn_channels'],
            kernel_size=config['kernel_size'],
            dropout=config['dropout']
        )

        feature_dim = self.tcn.output_channels

        # Task-specific heads
        # 1. Swing phase classification (per timestep)
        self.phase_head = nn.Sequential(
            nn.Conv1d(feature_dim, 128, 1),
            nn.ReLU(),
            nn.Dropout(config['dropout']),
            nn.Conv1d(128, config['num_phases'], 1)
        )

        # 2. Contact time regression (single value per window)
        self.contact_head = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(feature_dim, 128),
            nn.ReLU(),
            nn.Dropout(config['dropout']),
            nn.Linear(128, 1),
            nn.Sigmoid()  # Output 0-1 (relative position in window)
        )

        # 3. Shot type classification (single value per window)
        self.shot_head = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(feature_dim, 128),
            nn.ReLU(),
            nn.Dropout(config['dropout']),
            nn.Linear(128, config['num_shot_types'])
        )

        # 4. Bat speed regression (peak speed in window)
        self.speed_head = nn.Sequential(
            nn.AdaptiveMaxPool1d(1),
            nn.Flatten(),
            nn.Linear(feature_dim, 64),
            nn.ReLU(),
            nn.Dropout(config['dropout']),
            nn.Linear(64, 1),
            nn.ReLU()  # Speed is positive
        )

        # 5. Wrist rotation at contact (degrees)
        self.wrist_head = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(feature_dim, 64),
            nn.ReLU(),
            nn.Dropout(config['dropout']),
            nn.Linear(64, 1)
        )

    def forward(self, x):
        """
        Args:
            x: (batch, channels=9, seq_len=200)
        Returns:
            dict with keys: phase_logits, contact_time, shot_logits, bat_speed, wrist_rotation
        """
        # TCN backbone
        features = self.tcn(x)  # (batch, feature_dim, seq_len)

        # Phase classification (per timestep)
        phase_logits = self.phase_head(features)  # (batch, num_phases, seq_len)

        # Window-level predictions
        contact_time = self.contact_head(features)  # (batch, 1)
        shot_logits = self.shot_head(features)      # (batch, num_shot_types)
        bat_speed = self.speed_head(features)       # (batch, 1)
        wrist_rotation = self.wrist_head(features)  # (batch, 1)

        return {
            'phase_logits': phase_logits,
            'contact_time': contact_time.squeeze(-1),
            'shot_logits': shot_logits,
            'bat_speed': bat_speed.squeeze(-1),
            'wrist_rotation': wrist_rotation.squeeze(-1)
        }


# ============================================================================
# DATASET
# ============================================================================

class CricketSwingDataset(Dataset):
    """Dataset for cricket swing detection from sensor data"""
    def __init__(self, data_dir: str, window_size: int = 200, stride: int = 50,
                 augment: bool = True, shot_encoder: LabelEncoder = None):
        self.window_size = window_size
        self.stride = stride
        self.augment = augment

        # Load data
        self.samples = self._load_data(data_dir)

        # Shot type encoder
        if shot_encoder is None:
            self.shot_encoder = LabelEncoder()
            all_shots = [s['shot_type'] for s in self.samples if s['shot_type']]
            if all_shots:
                self.shot_encoder.fit(all_shots)
        else:
            self.shot_encoder = shot_encoder

        # Create windows
        self.windows = self._create_windows()

        print(f"Loaded {len(self.windows)} windows from {len(self.samples)} samples")

    def _load_data(self, data_dir: str) -> List[Dict]:
        """Load sensor data from various formats"""
        samples = []
        data_path = Path(data_dir)

        # Support multiple dataset formats
        for ext in ['.json', '.npz', '.csv']:
            for file in data_path.rglob(f'*{ext}'):
                try:
                    if ext == '.json':
                        with open(file) as f:
                            data = json.load(f)
                            if isinstance(data, list):
                                samples.extend(data)
                            else:
                                samples.append(data)
                    elif ext == '.npz':
                        npz = np.load(file, allow_pickle=True)
                        for key in npz.files:
                            samples.append(npz[key].item() if npz[key].ndim == 0 else npz[key])
                    elif ext == '.csv':
                        # Assume CSV with columns: timestamp, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, mag_x, mag_y, mag_z, ...
                        import pandas as pd
                        df = pd.read_csv(file)
                        samples.append(self._csv_to_sample(df))
                except Exception as e:
                    print(f"Warning: Failed to load {file}: {e}")

        return samples

    def _csv_to_sample(self, df) -> Dict:
        """Convert CSV DataFrame to sample format"""
        # Expected columns
        sensor_cols = ['accel_x', 'accel_y', 'accel_z',
                       'gyro_x', 'gyro_y', 'gyro_z',
                       'mag_x', 'mag_y', 'mag_z']
        available = [c for c in sensor_cols if c in df.columns]
        sensor_data = df[available].values.astype(np.float32).T  # (channels, timesteps)

        return {
            'sensor_data': sensor_data,
            'shot_type': df.get('shot_type', [None])[0] if 'shot_type' in df.columns else None,
            'contact_frame': df.get('contact_frame', [None])[0] if 'contact_frame' in df.columns else None,
            'bat_speed': df.get('bat_speed', [None])[0] if 'bat_speed' in df.columns else None,
            'wrist_rotation': df.get('wrist_rotation', [None])[0] if 'wrist_rotation' in df.columns else None,
            'phase_labels': df.get('phase', [None]).values if 'phase' in df.columns else None
        }

    def _create_windows(self) -> List[Dict]:
        """Create sliding windows from continuous sensor data"""
        windows = []

        for sample in self.samples:
            sensor_data = sample['sensor_data']  # (channels, timesteps)
            if sensor_data is None or sensor_data.shape[1] < self.window_size:
                continue

            num_windows = (sensor_data.shape[1] - self.window_size) // self.stride + 1

            for i in range(num_windows):
                start = i * self.stride
                end = start + self.window_size
                window_data = sensor_data[:, start:end]

                # Labels
                phase_labels = None
                if sample['phase_labels'] is not None:
                    phase_labels = sample['phase_labels'][start:end]

                contact_rel = None
                if sample['contact_frame'] is not None:
                    contact_abs = sample['contact_frame']
                    if start <= contact_abs < end:
                        contact_rel = (contact_abs - start) / self.window_size

                windows.append({
                    'data': window_data,
                    'shot_type': sample['shot_type'],
                    'contact_time': contact_rel,
                    'bat_speed': sample['bat_speed'],
                    'wrist_rotation': sample['wrist_rotation'],
                    'phase_labels': phase_labels
                })

        return windows

    def __len__(self):
        return len(self.windows)

    def __getitem__(self, idx):
        window = self.windows[idx]

        # Sensor data: (channels, seq_len)
        x = torch.from_numpy(window['data']).float()

        # Normalize (per channel)
        x = (x - x.mean(dim=1, keepdim=True)) / (x.std(dim=1, keepdim=True) + 1e-8)

        # Data augmentation
        if self.augment:
            x = self._augment(x)

        # Phase labels (per timestep)
        if window['phase_labels'] is not None:
            phase_labels = torch.from_numpy(
                np.array([SWING_PHASES.index(p) if p in SWING_PHASES else 0
                         for p in window['phase_labels']])
            ).long()
        else:
            # Generate pseudo-labels based on acceleration profile
            phase_labels = self._generate_pseudo_phases(x)

        # Contact time (0-1 relative position)
        contact_time = window['contact_time']
        if contact_time is None:
            contact_time = 0.5  # Default to middle
        contact_time = torch.tensor(contact_time, dtype=torch.float32)

        # Shot type
        shot_type = window['shot_type']
        if shot_type and shot_type in self.shot_encoder.classes_:
            shot_label = torch.tensor(self.shot_encoder.transform([shot_type])[0], dtype=torch.long)
        else:
            shot_label = torch.tensor(0, dtype=torch.long)  # Default to first class

        # Bat speed
        bat_speed = window['bat_speed']
        if bat_speed is None:
            bat_speed = 20.0  # Default m/s
        bat_speed = torch.tensor(bat_speed / 50.0, dtype=torch.float32)  # Normalize

        # Wrist rotation
        wrist_rot = window['wrist_rotation']
        if wrist_rot is None:
            wrist_rot = 0.0
        wrist_rot = torch.tensor(wrist_rot / 180.0, dtype=torch.float32)  # Normalize to [-1, 1]

        return {
            'input': x,
            'phase_labels': phase_labels,
            'contact_time': contact_time,
            'shot_label': shot_label,
            'bat_speed': bat_speed,
            'wrist_rotation': wrist_rot
        }

    def _augment(self, x: torch.Tensor) -> torch.Tensor:
        """Data augmentation for sensor data"""
        # Random noise
        if torch.rand(1) < 0.5:
            x = x + torch.randn_like(x) * 0.01

        # Random scaling
        if torch.rand(1) < 0.3:
            scale = 0.9 + torch.rand(1) * 0.2
            x = x * scale

        # Random time warp (simple)
        if torch.rand(1) < 0.2:
            # Simple interpolation-based warping
            indices = torch.linspace(0, x.shape[1] - 1, x.shape[1])
            warp = torch.cumsum(torch.randn(x.shape[1]) * 0.01, dim=0)
            indices = indices + warp
            indices = torch.clamp(indices, 0, x.shape[1] - 1)
            # This is simplified; real implementation would use grid_sample
            pass

        return x

    def _generate_pseudo_phases(self, x: torch.Tensor) -> torch.Tensor:
        """Generate pseudo phase labels from acceleration magnitude"""
        # Acceleration magnitude
        accel = x[:3]  # First 3 channels
        accel_mag = torch.norm(accel, dim=0)

        # Simple threshold-based phase detection
        phases = torch.zeros(x.shape[1], dtype=torch.long)

        # High acceleration = downswing/contact
        # Medium = backswing/followthrough
        # Low = idle

        # Normalize
        accel_norm = (accel_mag - accel_mag.min()) / (accel_mag.max() - accel_mag.min() + 1e-8)

        # Find peak (contact)
        peak_idx = torch.argmax(accel_norm).item()

        # Assign phases relative to peak
        for i in range(x.shape[1]):
            dist = i - peak_idx
            if dist < -30:
                phases[i] = 0  # Idle
            elif dist < -10:
                phases[i] = 1  # Backswing
            elif dist < -3:
                phases[i] = 2  # Transition
            elif dist <= 0:
                phases[i] = 3  # Downswing
            elif dist == 0:
                phases[i] = 4  # Contact
            elif dist < 15:
                phases[i] = 5  # FollowThrough
            else:
                phases[i] = 6  # Recovery

        return phases


# ============================================================================
# LOSS FUNCTIONS
# ============================================================================

class MultiTaskLoss(nn.Module):
    """Combined loss for multi-task learning"""
    def __init__(self, config):
        super().__init__()
        self.config = config
        self.phase_criterion = nn.CrossEntropyLoss(ignore_index=-1)
        self.contact_criterion = nn.MSELoss()
        self.shot_criterion = nn.CrossEntropyLoss()
        self.speed_criterion = nn.MSELoss()
        self.wrist_criterion = nn.MSELoss()

    def forward(self, preds, targets):
        # Phase loss (per timestep)
        phase_loss = self.phase_criterion(
            preds['phase_logits'].transpose(1, 2),  # (batch, seq, classes)
            targets['phase_labels']
        )

        # Contact time loss
        contact_loss = self.contact_criterion(
            preds['contact_time'],
            targets['contact_time']
        )

        # Shot type loss
        shot_loss = self.shot_criterion(
            preds['shot_logits'],
            targets['shot_label']
        )

        # Bat speed loss
        speed_loss = self.speed_criterion(
            preds['bat_speed'],
            targets['bat_speed']
        )

        # Wrist rotation loss
        wrist_loss = self.wrist_criterion(
            preds['wrist_rotation'],
            targets['wrist_rotation']
        )

        # Weighted sum
        total_loss = (
            self.config['phase_weight'] * phase_loss +
            self.config['contact_weight'] * contact_loss +
            self.config['shot_weight'] * shot_loss +
            self.config['speed_weight'] * speed_loss +
            self.config['wrist_weight'] * wrist_loss
        )

        return {
            'total': total_loss,
            'phase': phase_loss,
            'contact': contact_loss,
            'shot': shot_loss,
            'speed': speed_loss,
            'wrist': wrist_loss
        }


# ============================================================================
# TRAINING
# ============================================================================

def train_model(config, data_dir, output_dir):
    """Main training loop"""
    device = torch.device(config['device'])
    print(f"Using device: {device}")

    # Dataset
    full_dataset = CricketSwingDataset(data_dir, window_size=config['window_size'], augment=True)

    # Split
    train_size = int(0.8 * len(full_dataset))
    val_size = len(full_dataset) - train_size
    train_dataset, val_dataset = random_split(full_dataset, [train_size, val_size])

    # Validation dataset without augmentation
    val_dataset.dataset.augment = False

    train_loader = DataLoader(train_dataset, batch_size=config['batch_size'],
                              shuffle=True, num_workers=4, pin_memory=True)
    val_loader = DataLoader(val_dataset, batch_size=config['batch_size'],
                            shuffle=False, num_workers=4, pin_memory=True)

    # Model
    model = CricketSwingModel(config).to(device)

    # Count parameters
    total_params = sum(p.numel() for p in model.parameters())
    trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print(f"Total parameters: {total_params:,}")
    print(f"Trainable parameters: {trainable_params:,}")

    # Loss and optimizer
    criterion = MultiTaskLoss(config)
    optimizer = torch.optim.AdamW(model.parameters(),
                                  lr=config['learning_rate'],
                                  weight_decay=config['weight_decay'])
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min',
                                                            factor=0.5, patience=5, verbose=True)

    # Training loop
    best_val_loss = float('inf')
    patience_counter = 0

    history = {'train_loss': [], 'val_loss': [], 'train_losses': [], 'val_losses': []}

    for epoch in range(config['epochs']):
        # Train
        model.train()
        train_losses = {k: 0.0 for k in ['total', 'phase', 'contact', 'shot', 'speed', 'wrist']}

        for batch in train_loader:
            inputs = batch['input'].to(device)
            targets = {k: v.to(device) for k, v in batch.items() if k != 'input'}

            optimizer.zero_grad()
            outputs = model(inputs)
            losses = criterion(outputs, targets)

            losses['total'].backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()

            for k, v in losses.items():
                train_losses[k] += v.item()

        # Average
        for k in train_losses:
            train_losses[k] /= len(train_loader)

        # Validate
        model.eval()
        val_losses = {k: 0.0 for k in ['total', 'phase', 'contact', 'shot', 'speed', 'wrist']}

        with torch.no_grad():
            for batch in val_loader:
                inputs = batch['input'].to(device)
                targets = {k: v.to(device) for k, v in batch.items() if k != 'input'}

                outputs = model(inputs)
                losses = criterion(outputs, targets)

                for k, v in losses.items():
                    val_losses[k] += v.item()

        for k in val_losses:
            val_losses[k] /= len(val_loader)

        # Logging
        print(f"Epoch {epoch+1}/{config['epochs']}")
        print(f"  Train: total={train_losses['total']:.4f}, "
              f"phase={train_losses['phase']:.4f}, contact={train_losses['contact']:.4f}, "
              f"shot={train_losses['shot']:.4f}, speed={train_losses['speed']:.4f}, "
              f"wrist={train_losses['wrist']:.4f}")
        print(f"  Val:   total={val_losses['total']:.4f}, "
              f"phase={val_losses['phase']:.4f}, contact={val_losses['contact']:.4f}, "
              f"shot={val_losses['shot']:.4f}, speed={val_losses['speed']:.4f}, "
              f"wrist={val_losses['wrist']:.4f}")

        # History
        history['train_loss'].append(train_losses['total'])
        history['val_loss'].append(val_losses['total'])
        history['train_losses'].append(train_losses)
        history['val_losses'].append(val_losses)

        # Learning rate scheduling
        scheduler.step(val_losses['total'])

        # Early stopping
        if val_losses['total'] < best_val_loss:
            best_val_loss = val_losses['total']
            patience_counter = 0
            # Save best model
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'config': config,
                'shot_encoder': full_dataset.shot_encoder,
                'val_loss': best_val_loss,
                'history': history
            }, Path(output_dir) / 'best_model.pth')
            print(f"  Saved best model (val_loss={best_val_loss:.4f})")
        else:
            patience_counter += 1
            if patience_counter >= config['patience']:
                print(f"Early stopping at epoch {epoch+1}")
                break

    # Load best model for export
    checkpoint = torch.load(Path(output_dir) / 'best_model.pth', map_location=device)
    model.load_state_dict(checkpoint['model_state_dict'])

    return model, full_dataset.shot_encoder, history


# ============================================================================
# EXPORT TO COREML (iOS)
# ============================================================================

def export_coreml(model, shot_encoder, config, output_path):
    """Export model to CoreML format for iOS"""
    model.eval()
    model.to('cpu')

    # Example input
    example_input = torch.randn(1, config['num_channels'], config['window_size'])

    # Trace the model
    traced_model = torch.jit.trace(model, example_input)

    # Convert to CoreML
    mlmodel = ct.convert(
        traced_model,
        inputs=[ct.TensorType(name="input", shape=example_input.shape)],
        outputs=[
            ct.TensorType(name="phase_logits"),
            ct.TensorType(name="contact_time"),
            ct.TensorType(name="shot_logits"),
            ct.TensorType(name="bat_speed"),
            ct.TensorType(name="wrist_rotation")
        ],
        minimum_deployment_target=config['coreml_min_deployment_target'],
        compute_units=ct.ComputeUnit.ALL
    )

    # Add metadata
    mlmodel.short_description = "Cricket Swing Detection Model"
    mlmodel.version = "1.0"
    mlmodel.author = "Cricket Simulator"
    mlmodel.license = "Proprietary"

    # Input/Output descriptions
    mlmodel.input_description["input"] = "Sensor data: 9 channels × 200 timesteps (accel, gyro, mag @ 100Hz)"
    mlmodel.output_description["phase_logits"] = "Swing phase per timestep: 7 classes × 200 timesteps"
    mlmodel.output_description["contact_time"] = "Predicted contact time (0-1 relative to window)"
    mlmodel.output_description["shot_logits"] = "Shot type classification: 12 classes"
    mlmodel.output_description["bat_speed"] = "Peak bat speed (normalized, multiply by 50 for m/s)"
    mlmodel.output_description["wrist_rotation"] = "Wrist rotation at contact (normalized, multiply by 180 for degrees)"

    # Save
    mlmodel.save(output_path)
    print(f"CoreML model saved to {output_path}")

    return mlmodel


# ============================================================================
# EXPORT TO TFLITE (Android)
# ============================================================================

def export_tflite(model, config, output_path):
    """Export model to TensorFlow Lite format for Android"""
    import tensorflow as tf

    model.eval()
    model.to('cpu')

    # Convert to ONNX first, then to TF, then TFLite
    # Or use torch.export + custom conversion

    # For simplicity, we'll create a TF Keras model with same architecture
    # and copy weights

    # Create TF model
    tf_model = create_tf_model(config)

    # Copy weights from PyTorch to TF
    copy_weights_pytorch_to_tf(model, tf_model)

    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(tf_model)

    if config['tflite_optimize']:
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.target_spec.supported_types = [tf.float16]

    converter.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS,
        tf.lite.OpsSet.SELECT_TF_OPS
    ]

    tflite_model = converter.convert()

    # Save
    with open(output_path, 'wb') as f:
        f.write(tflite_model)

    print(f"TFLite model saved to {output_path} ({len(tflite_model) / 1024:.1f} KB)")

    return tflite_model


def create_tf_model(config):
    """Create equivalent TF Keras model"""
    from tensorflow.keras import layers, Model, Input
    import tensorflow as tf

    inputs = Input(shape=(config['window_size'], config['num_channels']), name='input')
    x = layers.Permute((2, 1))(inputs)  # (batch, channels, seq) for Conv1D

    # TCN blocks
    for i, channels in enumerate(config['tcn_channels']):
        dilation = 2 ** i
        padding = (config['kernel_size'] - 1) * dilation

        # First conv
        x = layers.Conv1D(channels, config['kernel_size'],
                          padding='causal', dilation_rate=dilation)(x)
        x = layers.Activation('relu')(x)
        x = layers.Dropout(config['dropout'])(x)

        # Second conv
        x = layers.Conv1D(channels, config['kernel_size'],
                          padding='causal', dilation_rate=dilation)(x)
        x = layers.Activation('relu')(x)
        x = layers.Dropout(config['dropout'])(x)

        # Residual connection
        if i == 0:
            residual = layers.Conv1D(channels, 1)(inputs)
        else:
            residual = x  # Same channels
        x = layers.Add()([x, residual])
        x = layers.Activation('relu')(x)

    features = x  # (batch, channels, seq)

    # Phase head (per timestep)
    phase = layers.Conv1D(128, 1, activation='relu')(features)
    phase = layers.Dropout(config['dropout'])(phase)
    phase = layers.Conv1D(config['num_phases'], 1, name='phase_logits')(phase)
    phase = layers.Permute((2, 1))(phase)  # (batch, seq, classes)

    # Window-level heads
    pooled_avg = layers.GlobalAveragePooling1D()(features)
    pooled_max = layers.GlobalMaxPooling1D()(features)

    # Contact time
    contact = layers.Dense(128, activation='relu')(pooled_avg)
    contact = layers.Dropout(config['dropout'])(contact)
    contact = layers.Dense(1, activation='sigmoid', name='contact_time')(contact)

    # Shot type
    shot = layers.Dense(128, activation='relu')(pooled_avg)
    shot = layers.Dropout(config['dropout'])(shot)
    shot = layers.Dense(config['num_shot_types'], name='shot_logits')(shot)

    # Bat speed
    speed = layers.Dense(64, activation='relu')(pooled_max)
    speed = layers.Dropout(config['dropout'])(speed)
    speed = layers.Dense(1, activation='relu', name='bat_speed')(speed)

    # Wrist rotation
    wrist = layers.Dense(64, activation='relu')(pooled_avg)
    wrist = layers.Dropout(config['dropout'])(wrist)
    wrist = layers.Dense(1, name='wrist_rotation')(wrist)

    model = Model(inputs=inputs, outputs=[phase, contact, shot, speed, wrist])
    return model


def copy_weights_pytorch_to_tf(pt_model, tf_model):
    """Copy weights from PyTorch model to TF model"""
    # This is a simplified version - in practice you'd map each layer
    # For production, use ONNX as intermediate format
    print("Weight copying not fully implemented - use ONNX export for production")


# ============================================================================
# MAIN
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description='Train Cricket Swing Detection Model')
    parser.add_argument('--data-dir', type=str, required=True, help='Path to training data')
    parser.add_argument('--output-dir', type=str, default='./models', help='Output directory')
    parser.add_argument('--epochs', type=int, default=100, help='Number of epochs')
    parser.add_argument('--batch-size', type=int, default=64, help='Batch size')
    parser.add_argument('--device', type=str, default='auto', help='Device (cuda/cpu/auto)')
    args = parser.parse_args()

    # Update config
    config = CONFIG.copy()
    config['epochs'] = args.epochs
    config['batch_size'] = args.batch_size
    if args.device != 'auto':
        config['device'] = args.device

    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Save config
    with open(output_dir / 'config.json', 'w') as f:
        json.dump(config, f, indent=2)

    # Train
    print("Starting training...")
    model, shot_encoder, history = train_model(config, args.data_dir, str(output_dir))

    # Export
    print("\nExporting models...")

    # CoreML for iOS
    try:
        export_coreml(model, shot_encoder, config, str(output_dir / 'SwingDetector.mlmodel'))
    except Exception as e:
        print(f"CoreML export failed: {e}")

    # TFLite for Android
    try:
        export_tflite(model, config, str(output_dir / 'SwingDetector.tflite'))
    except Exception as e:
        print(f"TFLite export failed: {e}")

    # Save PyTorch model
    torch.save(model.state_dict(), output_dir / 'model.pth')

    # Save label encoders
    import joblib
    joblib.dump(shot_encoder, output_dir / 'shot_encoder.pkl')

    # Save history
    with open(output_dir / 'history.json', 'w') as f:
        json.dump(history, f, indent=2)

    print("\nTraining complete!")
    print(f"Models saved to {output_dir}")


if __name__ == '__main__':
    main()