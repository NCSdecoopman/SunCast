#include "ProcessDEM.h"
#include "ogr_spatialref.h"
#include <iostream>
#include <vector>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

DemProcessor::DemProcessor(int numThreads)
    : numThreads_(numThreads), solarCalc_(1.0) {
    // Register GDAL drivers
    GDALAllRegister();
    
#ifdef _OPENMP
    if (numThreads_ > 0) {
        omp_set_num_threads(numThreads_);
    }
    std::cerr << "OpenMP enabled with " << omp_get_max_threads() << " threads" << std::endl;
#else
    std::cout << "OpenMP not available, running single-threaded" << std::endl;
#endif
}

DemProcessor::~DemProcessor() {
}

void DemProcessor::pixelToGeo(const double* geoTransform, int pixelX, int pixelY,
                             double& lon, double& lat) const {
    // GDAL geotransform: [0]=top left x, [1]=w-e pixel resolution, [2]=rotation (0 if north up),
    //                    [3]=top left y, [4]=rotation (0 if north up), [5]=n-s pixel resolution (negative)
    lon = geoTransform[0] + pixelX * geoTransform[1] + pixelY * geoTransform[2];
    lat = geoTransform[3] + pixelX * geoTransform[4] + pixelY * geoTransform[5];
}

GDALDataset* DemProcessor::createOutputDataset(const std::string& outputPath,
                                               int width, int height,
                                               int numBands,
                                               const double* geoTransform,
                                               const char* projection) const {
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) {
        std::cerr << "Error: GTiff driver not available" << std::endl;
        return nullptr;
    }
    
    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", "LZW");
    options = CSLSetNameValue(options, "PREDICTOR", "2");
    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BLOCKXSIZE", "512");
    options = CSLSetNameValue(options, "BLOCKYSIZE", "512");
    options = CSLSetNameValue(options, "BIGTIFF", "IF_NEEDED"); // Important for large files > 4GB
    
    GDALDataset* dataset = driver->Create(outputPath.c_str(), width, height, numBands,
                                          GDT_Float32, options);
    CSLDestroy(options);
    
    if (!dataset) {
        std::cerr << "Error: Failed to create output dataset" << std::endl;
        return nullptr;
    }
    
    dataset->SetGeoTransform(const_cast<double*>(geoTransform));
    dataset->SetProjection(projection);
    
    // Set band descriptions and nodata values
    // Band 1: Jan 1 Sunrise, Band 2: Jan 1 Sunset, Band 3: Jan 2 Sunrise...
    for (int i = 1; i <= numBands; ++i) {
        GDALRasterBand* band = dataset->GetRasterBand(i);
        band->SetNoDataValue(NODATA_VALUE);
        
        int day = (i - 1) / 2 + 1;
        bool isSunrise = (i % 2 != 0);
        
        std::string desc = "Day " + std::to_string(day) + (isSunrise ? " Sunrise" : " Sunset");
        band->SetDescription(desc.c_str());
    }
    
    return dataset;
}

bool DemProcessor::streamBinaryOutput(const std::string& inputPath,
                                      int year,
                                      double timezoneOffset) {
    // Open input DEM
    GDALDataset* inputDataset = (GDALDataset*)GDALOpen(inputPath.c_str(), GA_ReadOnly);
    if (!inputDataset) {
        std::cerr << "Error: Failed to open input file: " << inputPath << std::endl;
        return false;
    }
    
    int width = inputDataset->GetRasterXSize();
    int height = inputDataset->GetRasterYSize();
    int totalPixels = width * height;
    
    // Get geotransform
    double geoTransform[6];
    inputDataset->GetGeoTransform(geoTransform);
    
    // Read DEM data into memory (Float32)
    GDALRasterBand* demBand = inputDataset->GetRasterBand(1);
    float* demData = new float[totalPixels];
    
    CPLErr err = demBand->RasterIO(GF_Read, 0, 0, width, height,
                                   demData, width, height, GDT_Float32, 0, 0);
    
    if (err != CE_None) {
        std::cerr << "Error: Failed to read DEM data" << std::endl;
        delete[] demData;
        GDALClose(inputDataset);
        return false;
    }
    
    float demNodata = static_cast<float>(demBand->GetNoDataValue());
    
    // Determine number of days
    bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int daysInYear = isLeap ? 366 : 365;
    
    // Allocate output buffers (Int16)
    int16_t* sunriseBuffer = new int16_t[totalPixels];
    int16_t* sunsetBuffer = new int16_t[totalPixels];
    
    SolarCalculator calc(timezoneOffset);
    
    // Output raster dimensions first (metadata)
    // Using cerr for metadata to keep stdout clean for binary data?
    // No, let's put metadata in the stream header.
    // Header: [Magic: "SOLAR"][Width: int32][Height: int32][Days: int32]
    const char magic[] = "SOLAR";
    std::cout.write(magic, 5);
    std::cout.write(reinterpret_cast<const char*>(&width), sizeof(int32_t));
    std::cout.write(reinterpret_cast<const char*>(&height), sizeof(int32_t));
    std::cout.write(reinterpret_cast<const char*>(&daysInYear), sizeof(int32_t));
    std::cout.write(reinterpret_cast<const char*>(geoTransform), 6 * sizeof(double));
    std::cout.flush();
    
    // Loop over days
    int currentDayOfYear = 0;
    for (int m = 1; m <= 12; ++m) {
        int daysInMonth = 31;
        if (m == 4 || m == 6 || m == 9 || m == 11) daysInMonth = 30;
        else if (m == 2) daysInMonth = isLeap ? 29 : 28;
        
        for (int d = 1; d <= daysInMonth; ++d) {
            currentDayOfYear++;
            
            // Parallel calculation for this day
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < totalPixels; ++i) {
                float elevation = demData[i];
                
                if (std::isnan(elevation) || elevation == demNodata || elevation == 0.0f) {
                    sunriseBuffer[i] = -1;
                    sunsetBuffer[i] = -1;
                } else {
                    int y = i / width;
                    int x = i % width;
                    double lon, lat;
                    pixelToGeo(geoTransform, x, y, lon, lat);
                    
                    double sunrise = calc.calculateSunrise(lat, lon, elevation, year, m, d);
                    double sunset = calc.calculateSunset(lat, lon, elevation, year, m, d);
                    
                    // Convert to minutes (Int16)
                    // Handle NoData (-9999) from calculator
                    if (sunrise < 0) sunriseBuffer[i] = -1;
                    else sunriseBuffer[i] = static_cast<int16_t>(std::round(sunrise * 60.0));
                    
                    if (sunset < 0) sunsetBuffer[i] = -1;
                    else sunsetBuffer[i] = static_cast<int16_t>(std::round(sunset * 60.0));
                }
            }
            
            // Write binary block for this day
            // [DayID: int32][SunriseArray][SunsetArray]
            std::cout.write(reinterpret_cast<const char*>(&currentDayOfYear), sizeof(int32_t));
            std::cout.write(reinterpret_cast<const char*>(sunriseBuffer), totalPixels * sizeof(int16_t));
            std::cout.write(reinterpret_cast<const char*>(sunsetBuffer), totalPixels * sizeof(int16_t));
            std::cout.flush();
            
            // Progress to stderr to avoid corrupting stdout
            if (currentDayOfYear % 10 == 0) {
                std::cerr << "Processed day " << currentDayOfYear << "/" << daysInYear << std::endl;
            }
        }
    }
    
    delete[] demData;
    delete[] sunriseBuffer;
    delete[] sunsetBuffer;
    GDALClose(inputDataset);
    
    return true;
}

bool DemProcessor::processDEM(const std::string& inputPath,
                             const std::string& outputPath,
                             int year,
                             double timezoneOffset) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Processing: " << inputPath << std::endl;
    std::cout << "Year: " << year << " (Full Year)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Open input DEM
    GDALDataset* inputDataset = (GDALDataset*)GDALOpen(inputPath.c_str(), GA_ReadOnly);
    if (!inputDataset) {
        std::cerr << "Error: Failed to open input file: " << inputPath << std::endl;
        return false;
    }
    
    int width = inputDataset->GetRasterXSize();
    int height = inputDataset->GetRasterYSize();
    
    std::cout << "DEM dimensions: " << width << " x " << height << " pixels" << std::endl;
    
    // Get geotransform and projection
    double geoTransform[6];
    inputDataset->GetGeoTransform(geoTransform);
    const char* projection = inputDataset->GetProjectionRef();
    
    // Determine number of days in year (handle leap years)
    bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int daysInYear = isLeap ? 366 : 365;
    int numBands = daysInYear * 2;
    
    std::cout << "Days in year: " << daysInYear << std::endl;
    std::cout << "Output bands: " << numBands << std::endl;
    
    // Create output dataset
    GDALDataset* outputDataset = createOutputDataset(outputPath, width, height, numBands,
                                                      geoTransform, projection);
    if (!outputDataset) {
        GDALClose(inputDataset);
        return false;
    }
    
    // Initialize solar calculator
    SolarCalculator calc(timezoneOffset);
    
    // Process in blocks to manage memory
    // Block size 512x512 is standard for tiled GeoTIFF
    int blockXSize = 512;
    int blockYSize = 512;
    
    // Buffer for one block of DEM data
    float* demBlock = new float[blockXSize * blockYSize];
    
    // Buffer for one block of output data (all bands)
    // Size: 512 * 512 * 730 * 4 bytes ~= 765 MB
    // This is allocated once and reused
    float* outputBlock = new float[blockXSize * blockYSize * numBands];
    
    GDALRasterBand* demBand = inputDataset->GetRasterBand(1);
    float demNodata = static_cast<float>(demBand->GetNoDataValue());
    
    int totalBlocks = ((width + blockXSize - 1) / blockXSize) * ((height + blockYSize - 1) / blockYSize);
    int processedBlocks = 0;
    
    std::cout << "\nProcessing blocks..." << std::endl;
    
    // Loop over blocks
    for (int y = 0; y < height; y += blockYSize) {
        for (int x = 0; x < width; x += blockXSize) {
            int currentBlockX = std::min(blockXSize, width - x);
            int currentBlockY = std::min(blockYSize, height - y);
            
            // Read DEM block
            CPLErr err = demBand->RasterIO(GF_Read, x, y, currentBlockX, currentBlockY,
                                         demBlock, currentBlockX, currentBlockY, GDT_Float32,
                                         0, 0);
            
            if (err != CE_None) {
                std::cerr << "Error reading DEM block at " << x << "," << y << std::endl;
                continue;
            }
            
            // Process pixels in block
            // Parallelize over pixels within the block
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < currentBlockY * currentBlockX; ++i) {
                int localY = i / currentBlockX;
                int localX = i % currentBlockX;
                int globalX = x + localX;
                int globalY = y + localY;
                
                float elevation = demBlock[i];
                
                // Base index in output buffer for this pixel
                // Output buffer layout: [pixel 0 all bands][pixel 1 all bands]...
                // Wait, GDAL RasterIO expects: [band 1 all pixels][band 2 all pixels]... if using band sequential
                // OR [pixel 0 band 1][pixel 0 band 2]... if using pixel interleaved
                // Let's use standard band sequential for the buffer to match GDAL default expectation for multi-band write
                // Buffer layout: [Band 1 (all pixels)][Band 2 (all pixels)]...
                
                if (std::isnan(elevation) || elevation == demNodata) {
                    for (int b = 0; b < numBands; ++b) {
                        outputBlock[b * (currentBlockX * currentBlockY) + i] = NODATA_VALUE;
                    }
                } else {
                    double lon, lat;
                    pixelToGeo(geoTransform, globalX, globalY, lon, lat);
                    
                    // Calculate for all days
                    int currentDayOfYear = 0;
                    for (int m = 1; m <= 12; ++m) {
                        int daysInMonth = 31;
                        if (m == 4 || m == 6 || m == 9 || m == 11) daysInMonth = 30;
                        else if (m == 2) daysInMonth = isLeap ? 29 : 28;
                        
                        for (int d = 1; d <= daysInMonth; ++d) {
                            currentDayOfYear++;
                            
                            double sunrise = calc.calculateSunrise(lat, lon, elevation, year, m, d);
                            double sunset = calc.calculateSunset(lat, lon, elevation, year, m, d);
                            
                            // Band indices (0-based for buffer)
                            int sunriseBandIdx = (currentDayOfYear - 1) * 2;
                            int sunsetBandIdx = sunriseBandIdx + 1;
                            
                            outputBlock[sunriseBandIdx * (currentBlockX * currentBlockY) + i] = static_cast<float>(sunrise);
                            outputBlock[sunsetBandIdx * (currentBlockX * currentBlockY) + i] = static_cast<float>(sunset);
                        }
                    }
                }
            }
            
            // Write output block to all bands
            // We use RasterIO on the dataset to write all bands at once
            // Note: We need to construct the band list
            std::vector<int> bandList(numBands);
            for(int i=0; i<numBands; ++i) bandList[i] = i + 1;
            
            err = outputDataset->RasterIO(GF_Write, x, y, currentBlockX, currentBlockY,
                                        outputBlock, currentBlockX, currentBlockY, GDT_Float32,
                                        numBands, bandList.data(),
                                        0, 0, 0); // Default strides for band sequential
            
            if (err != CE_None) {
                std::cerr << "Error writing output block at " << x << "," << y << std::endl;
            }
            
            processedBlocks++;
            std::cout << "\rProcessed block " << processedBlocks << "/" << totalBlocks << std::flush;
        }
    }
    
    std::cout << "\n\nWriting metadata and closing..." << std::endl;
    
    delete[] demBlock;
    delete[] outputBlock;
    
    GDALClose(inputDataset);
    GDALClose(outputDataset);
    
    std::cout << "✓ Output saved to: " << outputPath << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return true;
}
