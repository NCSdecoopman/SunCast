#!/usr/bin/env python3
"""
Solar Time Calculation - Direct Parquet Generation
Streams binary data from C++ calculator and writes partitioned Parquet files.
"""
import subprocess
import struct
import sys
import logging
import time
import json
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
from pathlib import Path

from ..config import (
    OUTPUT_DIR,
    TARGET_DEPARTMENTS,
    DEPT_NAMES
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger(__name__)

# Paths
BUILD_DIR = Path(__file__).parent.parent.parent / "build"
SOLAR_CALCULATOR_BIN = BUILD_DIR / "solar_calculator"
PARQUET_DIR = Path(__file__).parent.parent.parent / "data" / "parquet"

def read_exact(proc, size):
    """Read exact number of bytes from process stdout"""
    data = proc.stdout.read(size)
    if len(data) < size:
        raise EOFError(f"Expected {size} bytes, got {len(data)}")
    return data

def process_department(dept_code, year=2025, threads=96):
    """Process a single department using streaming"""
    dept_name = DEPT_NAMES.get(dept_code, dept_code)
    input_file = OUTPUT_DIR / f"dem_dept_{dept_code}.tif"
    
    if not input_file.exists():
        logger.warning(f"Input file not found: {input_file}")
        return False

    logger.info(f"Processing Department {dept_code} ({dept_name})")
    
    # Create partition directory
    partition_dir = PARQUET_DIR / f"dept={dept_code}"
    partition_dir.mkdir(parents=True, exist_ok=True)
    parquet_file = partition_dir / "data.parquet"
    metadata_file = partition_dir / "metadata.json"
    
    cmd = [
        str(SOLAR_CALCULATOR_BIN),
        "--input", str(input_file),
        "--stream",
        "--year", str(year),
        "--threads", str(threads)
    ]
    
    logger.info(f"Launching C++ process: {' '.join(cmd)}")
    
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,  # Pass stderr to console for progress
        bufsize=1024*1024   # 1MB buffer
    )
    
    try:
        # Read Header
        # [Magic: 5][Width: 4][Height: 4][Days: 4][GeoTransform: 48]
        magic = read_exact(proc, 5)
        if magic != b"SOLAR":
            raise ValueError(f"Invalid magic bytes: {magic}")
            
        width = struct.unpack('i', read_exact(proc, 4))[0]
        height = struct.unpack('i', read_exact(proc, 4))[0]
        days_in_year = struct.unpack('i', read_exact(proc, 4))[0]
        geo_transform = struct.unpack('6d', read_exact(proc, 48))
        
        logger.info(f"Metadata received: {width}x{height}, {days_in_year} days")
        
        # Save metadata
        with open(metadata_file, 'w') as f:
            json.dump({
                "width": width,
                "height": height,
                "transform": geo_transform,
                "crs": "EPSG:4326"
            }, f, indent=2)
            
        # Define Parquet Schema
        schema = pa.schema([
            ('day', pa.int32()),
            ('sunrise', pa.list_(pa.int16())),
            ('sunset', pa.list_(pa.int16()))
        ])
        
        # Open Parquet Writer
        writer = pq.ParquetWriter(parquet_file, schema, compression='snappy')
        
        total_pixels = width * height
        array_bytes = total_pixels * 2  # int16 = 2 bytes
        
        start_time = time.time()
        
        # Read loop
        for _ in range(days_in_year):
            # Read Day ID
            day_id = struct.unpack('i', read_exact(proc, 4))[0]
            
            # Read Sunrise Array
            sunrise_bytes = read_exact(proc, array_bytes)
            sunrise_array = np.frombuffer(sunrise_bytes, dtype=np.int16)
            
            # Read Sunset Array
            sunset_bytes = read_exact(proc, array_bytes)
            sunset_array = np.frombuffer(sunset_bytes, dtype=np.int16)
            
            # Create Table
            batch = pa.RecordBatch.from_arrays(
                [
                    pa.array([day_id], type=pa.int32()),
                    pa.array([sunrise_array], type=pa.list_(pa.int16())),
                    pa.array([sunset_array], type=pa.list_(pa.int16()))
                ],
                schema=schema
            )
            
            writer.write_batch(batch)
            
        writer.close()
        proc.wait()
        
        duration = time.time() - start_time
        logger.info(f"✓ Completed {dept_code} in {duration:.2f}s")
        return True
        
    except Exception as e:
        logger.error(f"Error processing {dept_code}: {e}")
        proc.kill()
        return False

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Run solar calculation for departments")
    parser.add_argument("--dept", type=str, help="Specific department code to process")
    parser.add_argument("--index", type=int, help="Index of department in configuration list")
    args = parser.parse_args()

    logger.info("=" * 60)
    logger.info("Solar Parquet Generator")
    logger.info("=" * 60)
    
    if not SOLAR_CALCULATOR_BIN.exists():
        logger.error("Binary not found. Build first.")
        return 1
        
    # Determine which departments to process
    if args.dept:
        if args.dept not in TARGET_DEPARTMENTS:
            logger.warning(f"Department {args.dept} not in configuration target list, but proceeding anyway.")
        departments_to_process = [args.dept]
    elif args.index is not None:
        if 0 <= args.index < len(TARGET_DEPARTMENTS):
            departments_to_process = [TARGET_DEPARTMENTS[args.index]]
        else:
            logger.error(f"Index {args.index} out of range (0-{len(TARGET_DEPARTMENTS)-1})")
            return 1
    else:
        departments_to_process = TARGET_DEPARTMENTS

    success_count = 0
    total_start = time.time()
    
    for dept in departments_to_process:
        if process_department(dept):
            success_count += 1
            
    total_duration = time.time() - total_start
    logger.info("=" * 60)
    logger.info(f"Finished {success_count}/{len(departments_to_process)} departments in {total_duration:.2f}s")
    
    return 0 if success_count == len(departments_to_process) else 1

if __name__ == "__main__":
    sys.exit(main())
