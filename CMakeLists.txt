cmake_minimum_required(VERSION 3.15)
project(SolarCalculator CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler optimizations
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall")

# Find OpenMP
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
else()
    message(FATAL_ERROR "OpenMP not found")
endif()

# Find GDAL - prefer system installation
set(CMAKE_PREFIX_PATH "/usr" ${CMAKE_PREFIX_PATH})
find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    message(STATUS "GDAL found: ${GDAL_VERSION}")
    message(STATUS "GDAL include dir: ${GDAL_INCLUDE_DIRS}")
    message(STATUS "GDAL libraries: ${GDAL_LIBRARIES}")
    
    # Use system GDAL library explicitly
    if(EXISTS "/usr/lib/x86_64-linux-gnu/libgdal.so")
        set(GDAL_LIBRARIES "/usr/lib/x86_64-linux-gnu/libgdal.so")
        message(STATUS "Using system GDAL: ${GDAL_LIBRARIES}")
    endif()
else()
    message(FATAL_ERROR "GDAL not found")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/ProcessDEM.cpp
    src/SolarCalculator.cpp
)

# Create executable
add_executable(solar_calculator ${SOURCES})

# Set C++ standard for target
set_target_properties(solar_calculator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(solar_calculator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GDAL_INCLUDE_DIRS}
)

# Compile options
target_compile_options(solar_calculator PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-g -Wall>
)

# Link libraries
target_link_libraries(solar_calculator PRIVATE
    ${GDAL_LIBRARIES}
    OpenMP::OpenMP_CXX
)

# Print build configuration
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Solar Calculator Build Configuration")
message(STATUS "========================================")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()
message(STATUS "OpenMP: Enabled")
message(STATUS "GDAL: ${GDAL_VERSION}")
message(STATUS "========================================")
message(STATUS "")
