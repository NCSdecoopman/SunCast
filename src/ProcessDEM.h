#ifndef PROCESS_DEM_H
#define PROCESS_DEM_H

#include <string>
#include <memory>
#include "gdal_priv.h"
#include "SolarCalculator.h"

/**
 * DemProcessor class
 * 
 * Processes DEM files to calculate sunrise and sunset times
 * for each pixel using parallel computing with OpenMP.
 */
class DemProcessor {
public:
    /**
     * Constructor
     * @param numThreads Number of OpenMP threads to use
     */
    explicit DemProcessor(int numThreads = 96);
    
    /**
     * Destructor
     */
    ~DemProcessor();
    
    /**
     * Process a DEM file and stream binary data to stdout
     * Format: [int32 day][int32 count][int16 sunrise_array][int16 sunset_array]
     */
    bool streamBinaryOutput(const std::string& inputPath,
                           int year,
                           double timezoneOffset = 1.0);

    /**
     * Process a DEM file to calculate solar times for the full year
     * @param inputPath Path to input DEM GeoTIFF
     * @param outputPath Path for output solar times GeoTIFF
     * @param year Year for calculation
     * @param timezoneOffset Timezone offset from UTC (default: 1.0 for CET)
     * @return true if successful, false otherwise
     */
    bool processDEM(const std::string& inputPath,
                   const std::string& outputPath,
                   int year,
                   double timezoneOffset = 1.0);

private:
    int numThreads_;
    SolarCalculator solarCalc_;
    
    static constexpr float NODATA_VALUE = -9999.0f;
    
    /**
     * Convert pixel coordinates to geographic coordinates
     * @param geoTransform GDAL geotransform array
     * @param pixelX Pixel X coordinate
     * @param pixelY Pixel Y coordinate
     * @param lon Output longitude
     * @param lat Output latitude
     */
    void pixelToGeo(const double* geoTransform, int pixelX, int pixelY,
                   double& lon, double& lat) const;
    
    /**
     * Create output dataset with proper metadata
     */
    GDALDataset* createOutputDataset(const std::string& outputPath,
                                     int width, int height,
                                     int numBands,
                                     const double* geoTransform,
                                     const char* projection) const;
};

#endif // PROCESS_DEM_H
