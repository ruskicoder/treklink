#!/usr/bin/env python3
"""
Preprocess SisFall dataset for MagicWand training
Organizes data into 5 classes:
1. not_falling - Normal activities (ADLs)
2. falling_light - Trips, stumbles
3. falling_regular - Standard falls (forward, backward, lateral)
4. falling_extreme - High impact falls
5. falling_misc - Edge cases (syncope, recovery attempts)
"""

import os
import shutil
from pathlib import Path

# Define the classification mapping based on SisFall activity codes
ACTIVITY_MAPPING = {
    # NOT FALLING - Activities of Daily Living (ADLs)
    'not_falling': [
        'D01',  # Walking slowly
        'D02',  # Walking quickly
        'D03',  # Jogging slowly
        'D04',  # Jogging quickly
        'D05',  # Walking upstairs/downstairs slowly
        'D06',  # Walking upstairs/downstairs quickly
        'D07',  # Sit in half height chair slowly
        'D08',  # Sit in half height chair quickly
        'D09',  # Sit in low height chair slowly
        'D10',  # Sit in low height chair quickly
        'D12',  # Sitting, lying slowly, sit again
        'D13',  # Sitting, lying quickly, sit again
        'D14',  # On back, change to lateral, back again
        'D15',  # Standing, bending at knees, getting up
        'D16',  # Standing, bending without bending knees, getting up
        'D17',  # Get into car, remain seated, get out
        'D19',  # Gently jump without falling
    ],
    
    # FALLING LIGHT - Trips, stumbles, near-falls
    'falling_light': [
        'D18',  # Stumble while walking
        'F04',  # Fall forward while walking caused by a trip
    ],
    
    # FALLING REGULAR - Standard falls
    'falling_regular': [
        'F01',  # Fall forward while walking caused by a slip
        'F02',  # Fall backward while walking caused by a slip
        'F03',  # Lateral fall while walking caused by a slip
        'F05',  # Fall forward when trying to get up
        'F06',  # Fall backward when trying to get up
        'F07',  # Lateral fall when trying to get up
        'F08',  # Fall forward while sitting
        'F09',  # Fall backward while sitting
        'F10',  # Lateral fall while sitting
    ],
    
    # FALLING EXTREME - High impact, severe falls
    'falling_extreme': [
        'F11',  # Fall while walking downstairs
        'F12',  # Fall while walking upstairs
        'F13',  # Fall forward using hands to dampen fall
        'F14',  # Fall forward using knees to dampen fall
        'F15',  # Fall backward using hands to dampen fall
    ],
    
    # FALLING MISC - Edge cases
    'falling_misc': [
        'D11',  # Sitting, trying to get up, collapse into chair (recovery attempt)
    ]
}

def create_output_structure(base_path):
    """Create output directory structure"""
    output_path = base_path / 'processed'
    for class_name in ACTIVITY_MAPPING.keys():
        class_dir = output_path / class_name
        class_dir.mkdir(parents=True, exist_ok=True)
    return output_path

def convert_to_magicwand_format(input_file, output_file):
    """
    Convert SisFall format to MagicWand format
    SisFall: ADXL345_x, ADXL345_y, ADXL345_z, ITG3200_x, ITG3200_y, ITG3200_z, MMA8451Q_x, MMA8451Q_y, MMA8451Q_z
    MagicWand expects: ax, ay, az, gx, gy, gz (using ADXL345 accel + ITG3200 gyro)
    """
    with open(input_file, 'r') as f_in:
        lines = f_in.readlines()
    
    # Add separator at start (MagicWand format)
    output_lines = ['-.-.-\n']
    
    for line in lines:
        line = line.strip()
        if not line:
            continue
        
        # Remove any trailing semicolons and parse the 9 values
        line = line.rstrip(';')
        values = [int(x.strip()) for x in line.split(',')]
        if len(values) != 9:
            continue
        
        # Extract ADXL345 (accel) and ITG3200 (gyro)
        ax, ay, az = values[0], values[1], values[2]
        gx, gy, gz = values[3], values[4], values[5]
        
        # Format as MagicWand expects
        output_lines.append(f'{ax},{ay},{az},{gx},{gy},{gz}\n')
    
    # Add blank line at end
    output_lines.append('\n')
    
    with open(output_file, 'w') as f_out:
        f_out.writelines(output_lines)

def process_dataset(sisfall_path, output_path):
    """Process all files and organize by class"""
    stats = {class_name: 0 for class_name in ACTIVITY_MAPPING.keys()}
    
    # Iterate through all subject folders
    for subject_dir in sisfall_path.iterdir():
        if not subject_dir.is_dir() or subject_dir.name.startswith('.'):
            continue
        
        print(f'Processing {subject_dir.name}...')
        
        # Process each file in the subject directory
        for data_file in subject_dir.glob('*.txt'):
            # Extract activity code from filename (e.g., D01_SA01_R01.txt -> D01)
            activity_code = data_file.stem.split('_')[0]
            
            # Find which class this activity belongs to
            target_class = None
            for class_name, codes in ACTIVITY_MAPPING.items():
                if activity_code in codes:
                    target_class = class_name
                    break
            
            if target_class is None:
                print(f'  Warning: Unknown activity code {activity_code} in {data_file.name}')
                continue
            
            # Create output filename
            output_filename = f'output_{target_class}_{data_file.stem}.txt'
            output_file = output_path / target_class / output_filename
            
            # Convert and save
            convert_to_magicwand_format(data_file, output_file)
            stats[target_class] += 1
    
    return stats

def main():
    # Paths
    base_path = Path(__file__).parent
    sisfall_path = base_path / 'sisfallData'
    
    if not sisfall_path.exists():
        print(f'Error: SisFall dataset not found at {sisfall_path}')
        return
    
    print('Creating output directory structure...')
    output_path = create_output_structure(base_path)
    
    print('Processing dataset...')
    stats = process_dataset(sisfall_path, output_path)
    
    print('\n=== Processing Complete ===')
    print(f'Output directory: {output_path}')
    print('\nFiles per class:')
    for class_name, count in stats.items():
        print(f'  {class_name}: {count} files')
    
    print('\n=== Class Descriptions ===')
    print('not_falling: Normal daily activities (walking, sitting, stairs, etc.)')
    print('falling_light: Stumbles and trips')
    print('falling_regular: Standard falls (slips, falls from sitting/standing)')
    print('falling_extreme: High-impact falls (stairs, using hands/knees to dampen)')
    print('falling_misc: Edge cases (collapse into chair, recovery attempts)')

if __name__ == '__main__':
    main()
