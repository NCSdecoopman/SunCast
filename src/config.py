"""
Configuration for DEM processing pipeline
"""
from pathlib import Path

# Project root directory
PROJECT_ROOT = Path(__file__).parent.parent

# Input data paths
DEM_FILE = PROJECT_ROOT / "data" / "raw" / "dem" / "dem.tif"
DEPT_SHAPEFILE = PROJECT_ROOT / "data" / "raw" / "departements" / "depts.shp"

# Output directory
OUTPUT_DIR = PROJECT_ROOT / "data" / "processed"
RESULTS_DIR = PROJECT_ROOT / "data" / "results"

# Target department codes
TARGET_DEPARTMENTS = ["38", "73", "74"]

# Processing parameters
COMPRESSION = "LZW"  # Compression method for output GeoTIFF
PREDICTOR = 2  # Predictor for better compression of elevation data
TILED = True  # Use tiled format for better performance
BLOCKSIZE = 512  # Tile size

# Department names for logging (optional)
DEPT_NAMES = {
    "38": "Isère",
    "73": "Savoie",
    "74": "Haute-Savoie"
}
