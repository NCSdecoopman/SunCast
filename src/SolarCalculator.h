#ifndef SOLAR_CALCULATOR_H
#define SOLAR_CALCULATOR_H

#include <cmath>
#include <ctime>

/**
 * SolarCalculator class
 * 
 * Implements NOAA solar position algorithms for calculating
 * sunrise and sunset times at a given location and date.
 * 
 * Thread-safe for use in OpenMP parallel loops.
 */
class SolarCalculator {
public:
    /**
     * Constructor
     * @param timezoneOffset Timezone offset from UTC in hours (e.g., 1.0 for CET)
     */
    explicit SolarCalculator(double timezoneOffset = 1.0);
    
    /**
     * Calculate sunrise time
     * @param latitude Latitude in degrees (-90 to 90)
     * @param longitude Longitude in degrees (-180 to 180)
     * @param elevation Elevation above sea level in meters
     * @param year Year (e.g., 2025)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @return Sunrise time in decimal hours (local time), or -9999.0 if no sunrise
     */
    double calculateSunrise(double latitude, double longitude, double elevation,
                           int year, int month, int day) const;
    
    /**
     * Calculate sunset time
     * @param latitude Latitude in degrees (-90 to 90)
     * @param longitude Longitude in degrees (-180 to 180)
     * @param elevation Elevation above sea level in meters
     * @param year Year (e.g., 2025)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @return Sunset time in decimal hours (local time), or -9999.0 if no sunset
     */
    double calculateSunset(double latitude, double longitude, double elevation,
                          int year, int month, int day) const;

private:
    double timezoneOffset_;  // Timezone offset from UTC in hours
    
    // Solar depression angle for sunrise/sunset (degrees below horizon)
    static constexpr double SOLAR_DEPRESSION = 0.833;
    
    /**
     * Calculate Julian day number
     */
    double julianDay(int year, int month, int day) const;
    
    /**
     * Calculate Julian century from Julian day
     */
    double julianCentury(double jd) const;
    
    /**
     * Calculate geometric mean longitude of sun (degrees)
     */
    double sunGeomMeanLongitude(double t) const;
    
    /**
     * Calculate geometric mean anomaly of sun (degrees)
     */
    double sunGeomMeanAnomaly(double t) const;
    
    /**
     * Calculate eccentricity of Earth's orbit
     */
    double earthOrbitEccentricity(double t) const;
    
    /**
     * Calculate equation of center for sun
     */
    double sunEquationOfCenter(double t) const;
    
    /**
     * Calculate true longitude of sun (degrees)
     */
    double sunTrueLongitude(double t) const;
    
    /**
     * Calculate apparent longitude of sun (degrees)
     */
    double sunApparentLongitude(double t) const;
    
    /**
     * Calculate mean obliquity of ecliptic (degrees)
     */
    double meanObliquityOfEcliptic(double t) const;
    
    /**
     * Calculate corrected obliquity of ecliptic (degrees)
     */
    double obliquityCorrection(double t) const;
    
    /**
     * Calculate sun's declination (degrees)
     */
    double sunDeclination(double t) const;
    
    /**
     * Calculate equation of time (minutes)
     */
    double equationOfTime(double t) const;
    
    /**
     * Calculate hour angle for sunrise/sunset (degrees)
     */
    double hourAngleSunrise(double latitude, double declination, double elevation) const;
    
    /**
     * Calculate sunrise/sunset time
     * @param isSunrise true for sunrise, false for sunset
     */
    double calculateSolarTime(double latitude, double longitude, double elevation,
                             int year, int month, int day, bool isSunrise) const;
};

#endif // SOLAR_CALCULATOR_H
