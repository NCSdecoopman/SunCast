#!/usr/bin/env python3
"""
Solar Time Calculation Orchestrator - Full Year
Runs the C++ solar calculator for all departments for the full year 2025
"""
import subprocess
import time
import logging
import sys
from pathlib import Path

from ..config import (
    OUTPUT_DIR,
    RESULTS_DIR,
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

# Path to C++ executable
BUILD_DIR = Path(__file__).parent.parent.parent / "build"
SOLAR_CALCULATOR_BIN = BUILD_DIR / "solar_calculator"


def main():
    """Run solar calculation for all departments"""
    logger.info("=" * 60)
    logger.info("Solar Time Calculation - Full Year 2025")
    logger.info("=" * 60)
    
    if not SOLAR_CALCULATOR_BIN.exists():
        logger.error(f"Binary not found: {SOLAR_CALCULATOR_BIN}")
        logger.error("Please build the project first.")
        return 1
    
    # Create results directory
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    
    # Configuration
    year = 2025
    threads = 96
    timezone = 1.0
    
    logger.info(f"Year: {year}")
    logger.info(f"Threads: {threads}")
    logger.info(f"Timezone: UTC+{timezone}")
    logger.info("=" * 60)
    
    results = []
    total_start_time = time.time()
    
    for dept_code in TARGET_DEPARTMENTS:
        dept_name = DEPT_NAMES.get(dept_code, dept_code)
        
        input_file = OUTPUT_DIR / f"dem_dept_{dept_code}.tif"
        output_file = RESULTS_DIR / f"solar_{year}_dept_{dept_code}.tif"
        
        if not input_file.exists():
            logger.warning(f"Input file not found for dept {dept_code}: {input_file}")
            continue
            
        logger.info(f"\nProcessing Department {dept_code} ({dept_name})")
        logger.info("=" * 60)
        
        cmd = [
            str(SOLAR_CALCULATOR_BIN),
            "--input", str(input_file),
            "--output", str(output_file),
            "--year", str(year),
            "--threads", str(threads),
            "--timezone", str(timezone)
        ]
        
        logger.info(f"Running: {' '.join(cmd)}")
        
        start_time = time.time()
        try:
            # Run C++ executable
            process = subprocess.run(
                cmd,
                check=True,
                capture_output=False,  # Let stdout/stderr go to console
                text=True
            )
            
            duration = time.time() - start_time
            logger.info(f"\n✓ Department {dept_code} completed in {duration:.2f} seconds")
            results.append((dept_code, True, duration, output_file))
            
        except subprocess.CalledProcessError as e:
            duration = time.time() - start_time
            logger.error(f"\n✗ Department {dept_code} failed after {duration:.2f} seconds")
            results.append((dept_code, False, duration, None))
            
    total_duration = time.time() - total_start_time
    
    # Summary
    logger.info("\n" + "=" * 60)
    logger.info("PROCESSING SUMMARY")
    logger.info("=" * 60)
    
    success_count = sum(1 for r in results if r[1])
    logger.info(f"\nTotal departments processed: {len(results)}")
    logger.info(f"Successful: {success_count}")
    logger.info(f"Failed: {len(results) - success_count}")
    logger.info(f"\nTotal time: {total_duration:.2f} seconds")
    
    if success_count > 0:
        logger.info("\nSuccessful outputs:")
        for dept, success, duration, path in results:
            if success:
                logger.info(f"  - Dept {dept}: {path} ({duration:.2f}s)")
                
    logger.info("=" * 60)
    
    return 0 if success_count == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
