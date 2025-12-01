"""
DEM Processing Script
Extracts DEM data for specified French departments
"""
import logging
import sys
from pathlib import Path

import geopandas as gpd
import rasterio
from rasterio.mask import mask
from rasterio.warp import calculate_default_transform, reproject, Resampling

from ..config import (
    DEM_FILE,
    DEPT_SHAPEFILE,
    OUTPUT_DIR,
    TARGET_DEPARTMENTS,
    COMPRESSION,
    PREDICTOR,
    TILED,
    BLOCKSIZE,
    DEPT_NAMES
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)


def validate_inputs():
    """Validate that all required input files exist"""
    if not DEM_FILE.exists():
        raise FileNotFoundError(f"DEM file not found: {DEM_FILE}")
    
    if not DEPT_SHAPEFILE.exists():
        raise FileNotFoundError(f"Department shapefile not found: {DEPT_SHAPEFILE}")
    
    logger.info(f"✓ Input files validated")


def load_department_boundaries():
    """Load department boundaries from shapefile"""
    logger.info(f"Loading department boundaries from {DEPT_SHAPEFILE}")
    gdf = gpd.read_file(DEPT_SHAPEFILE)
    
    # If CRS is not defined, assume Lambert-93 (EPSG:2154) for French departments
    if gdf.crs is None:
        logger.warning("No CRS found in shapefile. Setting to Lambert-93 (EPSG:2154)")
        gdf = gdf.set_crs("EPSG:2154")
    
    # Log available columns to help identify the department code field
    logger.info(f"Available columns: {list(gdf.columns)}")
    logger.info(f"First few rows:\n{gdf.head()}")
    
    return gdf


def get_department_code_field(gdf):
    """Identify the field containing department codes"""
    # Common field names for department codes
    possible_fields = ['NUM_DEP', 'CODE_DEPT', 'DEPT', 'CODE', 'NUM_DEPT', 'DEP', 'INSEE_DEP']
    
    for field in possible_fields:
        if field in gdf.columns:
            logger.info(f"Using field '{field}' for department codes")
            return field
    
    # If none found, log columns and raise error
    logger.error(f"Could not find department code field. Available columns: {list(gdf.columns)}")
    raise ValueError("Department code field not found in shapefile")


def filter_departments(gdf, dept_field):
    """Filter GeoDataFrame to only include target departments"""
    # Convert department codes in GDF to string for comparison
    gdf[dept_field] = gdf[dept_field].astype(str)
    
    # Filter for target departments
    filtered = gdf[gdf[dept_field].isin(TARGET_DEPARTMENTS)]
    
    if len(filtered) == 0:
        raise ValueError(f"No departments found with codes {TARGET_DEPARTMENTS}")
    
    logger.info(f"Found {len(filtered)} target departments")
    return filtered


def clip_dem_for_department(dept_code, geometry, src_raster):
    """
    Clip DEM raster to department boundary
    
    Args:
        dept_code: Department code (e.g., "38")
        geometry: Department geometry (polygon)
        src_raster: Open rasterio dataset
    """
    dept_name = DEPT_NAMES.get(dept_code, dept_code)
    logger.info(f"Processing department {dept_code} ({dept_name})")
    
    # Clip the raster to the geometry
    try:
        out_image, out_transform = mask(
            src_raster,
            [geometry],
            crop=True,
            all_touched=True,
            nodata=src_raster.nodata
        )
        
        # Update metadata
        out_meta = src_raster.meta.copy()
        out_meta.update({
            "driver": "GTiff",
            "height": out_image.shape[1],
            "width": out_image.shape[2],
            "transform": out_transform,
            "compress": COMPRESSION,
            "predictor": PREDICTOR,
            "tiled": TILED,
            "blockxsize": BLOCKSIZE,
            "blockysize": BLOCKSIZE
        })
        
        # Write output file
        output_file = OUTPUT_DIR / f"dem_dept_{dept_code}.tif"
        with rasterio.open(output_file, "w", **out_meta) as dest:
            dest.write(out_image)
        
        logger.info(f"✓ Created {output_file}")
        logger.info(f"  Dimensions: {out_image.shape[2]} x {out_image.shape[1]} pixels")
        
        return output_file
        
    except Exception as e:
        logger.error(f"Error processing department {dept_code}: {e}")
        raise


def main():
    """Main processing function"""
    try:
        logger.info("=" * 60)
        logger.info("DEM Processing for Alpine Departments")
        logger.info("=" * 60)
        
        # Validate inputs
        validate_inputs()
        
        # Create output directory
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        logger.info(f"Output directory: {OUTPUT_DIR}")
        
        # Load department boundaries
        dept_gdf = load_department_boundaries()
        
        # Identify department code field
        dept_field = get_department_code_field(dept_gdf)
        
        # Filter for target departments
        target_depts = filter_departments(dept_gdf, dept_field)
        
        # Open DEM raster
        logger.info(f"Opening DEM file: {DEM_FILE}")
        with rasterio.open(DEM_FILE) as src:
            logger.info(f"DEM CRS: {src.crs}")
            logger.info(f"DEM bounds: {src.bounds}")
            logger.info(f"DEM shape: {src.width} x {src.height} pixels")
            logger.info(f"DEM resolution: {src.res}")
            
            # Reproject department boundaries to match DEM CRS if needed
            if target_depts.crs != src.crs:
                logger.info(f"Reprojecting departments from {target_depts.crs} to {src.crs}")
                target_depts = target_depts.to_crs(src.crs)
            
            # Process each department
            output_files = []
            for idx, row in target_depts.iterrows():
                dept_code = str(row[dept_field])
                geometry = row.geometry
                
                output_file = clip_dem_for_department(dept_code, geometry, src)
                output_files.append(output_file)
        
        # Summary
        logger.info("=" * 60)
        logger.info("Processing complete!")
        logger.info(f"Created {len(output_files)} DEM files:")
        for f in output_files:
            logger.info(f"  - {f}")
        logger.info("=" * 60)
        
    except Exception as e:
        logger.error(f"Processing failed: {e}")
        raise


if __name__ == "__main__":
    main()
