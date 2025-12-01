#!/usr/bin/env python3
"""
DEM Extraction using gdalwarp
More reliable than rasterio masking for complex reprojections
"""
import subprocess
import logging
import sys
from pathlib import Path

import geopandas as gpd

from ..config import (
    DEM_FILE,
    DEPT_SHAPEFILE,
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


def main():
    """Extract DEM for target departments using gdalwarp"""
    logger.info("=" * 60)
    logger.info("DEM Extraction using gdalwarp")
    logger.info("=" * 60)
    
    # Create output directory
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Load department boundaries
    logger.info(f"Loading department boundaries from {DEPT_SHAPEFILE}")
    gdf = gpd.read_file(DEPT_SHAPEFILE)
    
    # Set CRS if missing
    if gdf.crs is None:
        logger.warning("No CRS found, setting to Lambert II Extended (EPSG:27572)")
        gdf = gdf.set_crs("EPSG:27572")
    
    # Find department code field
    dept_field = 'NUM_DEP'
    if dept_field not in gdf.columns:
        logger.error(f"Field {dept_field} not found. Available: {gdf.columns.tolist()}")
        return 1
    
    # Convert to string
    gdf[dept_field] = gdf[dept_field].astype(str)
    
    # Filter target departments
    target_depts = gdf[gdf[dept_field].isin(TARGET_DEPARTMENTS)]
    logger.info(f"Found {len(target_depts)} target departments")
    
    # Process each department
    for idx, row in target_depts.iterrows():
        dept_code = row[dept_field]
        dept_name = DEPT_NAMES.get(dept_code, dept_code)
        geometry = row.geometry
        
        logger.info(f"\nProcessing department {dept_code} ({dept_name})")
        
        # Create temporary shapefile for this department
        temp_shp = OUTPUT_DIR / f"temp_dept_{dept_code}.shp"
        gpd.GeoDataFrame([row], crs=gdf.crs).to_file(temp_shp)
        
        # Output file
        output_file = OUTPUT_DIR / f"dem_dept_{dept_code}.tif"
        
        # Use gdalwarp to clip and reproject
        cmd = [
            "gdalwarp",
            "-cutline", str(temp_shp),
            "-crop_to_cutline",
            "-dstnodata", "0",
            "-co", "COMPRESS=LZW",
            "-co", "PREDICTOR=2",
            "-co", "TILED=YES",
            "-co", "BLOCKXSIZE=512",
            "-co", "BLOCKYSIZE=512",
            "-overwrite",
            str(DEM_FILE),
            str(output_file)
        ]
        
        logger.info(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            logger.error(f"gdalwarp failed: {result.stderr}")
            continue
        
        logger.info(f"✓ Created {output_file}")
        
        # Clean up temporary files
        for ext in ['.shp', '.shx', '.dbf', '.prj', '.cpg']:
            temp_file = OUTPUT_DIR / f"temp_dept_{dept_code}{ext}"
            if temp_file.exists():
                temp_file.unlink()
    
    logger.info("\n" + "=" * 60)
    logger.info("Extraction complete!")
    logger.info("=" * 60)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
