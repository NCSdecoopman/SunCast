#include <iostream>
#include <string>
#include <cstdlib>
#include "ProcessDEM.h"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << "\nOPTIONS:" << std::endl;
    std::cout << "  --input PATH        Input DEM GeoTIFF file (required)" << std::endl;
    std::cout << "  --output PATH       Output solar times GeoTIFF file (required)" << std::endl;
    std::cout << "  --year YYYY         Year for calculation (default: 2025)" << std::endl;
    std::cout << "  --threads N         Number of threads (default: 96)" << std::endl;
    std::cout << "  --timezone OFFSET   Timezone offset from UTC in hours (default: 1.0)" << std::endl;
    std::cout << "  --help              Show this help message" << std::endl;
    std::cout << "\nEXAMPLE:" << std::endl;
    std::cout << "  " << programName << " --input dem.tif --output solar.tif --year 2025 --threads 96" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string inputPath;
    std::string outputPath;
    bool streamMode = false;
    int year = 2025;
    int numThreads = 96;
    double timezoneOffset = 1.0;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        }
        else if (arg == "--stream") {
            streamMode = true;
        }
        else if (arg == "--year" && i + 1 < argc) {
            year = std::atoi(argv[++i]);
            if (year < 1900 || year > 2100) {
                std::cerr << "Error: Year must be between 1900 and 2100" << std::endl;
                return 1;
            }
        }
        else if (arg == "--threads" && i + 1 < argc) {
            numThreads = std::atoi(argv[++i]);
            if (numThreads < 1) {
                std::cerr << "Error: Number of threads must be at least 1" << std::endl;
                return 1;
            }
        }
        else if (arg == "--timezone" && i + 1 < argc) {
            timezoneOffset = std::atof(argv[++i]);
        }
        else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (inputPath.empty()) {
        std::cerr << "Error: Input file is required (--input)" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    if (!streamMode && outputPath.empty()) {
        std::cerr << "Error: Output file is required (--output) unless in --stream mode" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Process DEM
    DemProcessor processor(numThreads);
    bool success;
    
    if (streamMode) {
        // In stream mode, we don't print configuration to stdout to avoid corrupting the stream
        // We can print to stderr
        std::cerr << "Starting binary stream for " << inputPath << " (Year " << year << ")" << std::endl;
        success = processor.streamBinaryOutput(inputPath, year, timezoneOffset);
    } else {
        std::cout << "========================================" << std::endl;
        std::cout << "Solar Time Calculation" << std::endl;
        std::cout << "High-performance sunrise/sunset computation" << std::endl;
        std::cout << "========================================\n" << std::endl;

        // Display configuration
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Input:    " << inputPath << std::endl;
        std::cout << "  Output:   " << outputPath << std::endl;
        std::cout << "  Year:     " << year << std::endl;
        std::cout << "  Threads:  " << numThreads << std::endl;
        std::cout << "  Timezone: UTC" << (timezoneOffset >= 0 ? "+" : "") << timezoneOffset << std::endl;
        std::cout << std::endl;
        
        success = processor.processDEM(inputPath, outputPath, year, timezoneOffset);
    }
    
    if (success) {
        if (!streamMode) std::cout << "\n✓ Processing completed successfully!" << std::endl;
        return 0;
    } else {
        std::cerr << "\n✗ Processing failed!" << std::endl;
        return 1;
    }
}
