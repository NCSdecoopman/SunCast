#include "SolarCalculator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SolarCalculator::SolarCalculator(double timezoneOffset)
    : timezoneOffset_(timezoneOffset) {}

double SolarCalculator::calculateSunrise(double latitude, double longitude, double elevation,
                                        int year, int month, int day) const {
    return calculateSolarTime(latitude, longitude, elevation, year, month, day, true);
}

double SolarCalculator::calculateSunset(double latitude, double longitude, double elevation,
                                       int year, int month, int day) const {
    return calculateSolarTime(latitude, longitude, elevation, year, month, day, false);
}

double SolarCalculator::julianDay(int year, int month, int day) const {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + (A / 4);
    
    double JD = std::floor(365.25 * (year + 4716)) + 
                std::floor(30.6001 * (month + 1)) + 
                day + B - 1524.5;
    
    return JD;
}

double SolarCalculator::julianCentury(double jd) const {
    return (jd - 2451545.0) / 36525.0;
}

double SolarCalculator::sunGeomMeanLongitude(double t) const {
    double L0 = 280.46646 + t * (36000.76983 + t * 0.0003032);
    while (L0 > 360.0) L0 -= 360.0;
    while (L0 < 0.0) L0 += 360.0;
    return L0;
}

double SolarCalculator::sunGeomMeanAnomaly(double t) const {
    return 357.52911 + t * (35999.05029 - 0.0001537 * t);
}

double SolarCalculator::earthOrbitEccentricity(double t) const {
    return 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
}

double SolarCalculator::sunEquationOfCenter(double t) const {
    double m = sunGeomMeanAnomaly(t);
    double mrad = m * M_PI / 180.0;
    
    double sinm = std::sin(mrad);
    double sin2m = std::sin(2.0 * mrad);
    double sin3m = std::sin(3.0 * mrad);
    
    return sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) +
           sin2m * (0.019993 - 0.000101 * t) +
           sin3m * 0.000289;
}

double SolarCalculator::sunTrueLongitude(double t) const {
    return sunGeomMeanLongitude(t) + sunEquationOfCenter(t);
}

double SolarCalculator::sunApparentLongitude(double t) const {
    double o = sunTrueLongitude(t);
    return o - 0.00569 - 0.00478 * std::sin((125.04 - 1934.136 * t) * M_PI / 180.0);
}

double SolarCalculator::meanObliquityOfEcliptic(double t) const {
    return 23.0 + (26.0 + ((21.448 - t * (46.815 + t * (0.00059 - t * 0.001813)))) / 60.0) / 60.0;
}

double SolarCalculator::obliquityCorrection(double t) const {
    double e0 = meanObliquityOfEcliptic(t);
    double omega = 125.04 - 1934.136 * t;
    return e0 + 0.00256 * std::cos(omega * M_PI / 180.0);
}

double SolarCalculator::sunDeclination(double t) const {
    double e = obliquityCorrection(t);
    double lambda = sunApparentLongitude(t);
    
    double sint = std::sin(e * M_PI / 180.0) * std::sin(lambda * M_PI / 180.0);
    return std::asin(sint) * 180.0 / M_PI;
}

double SolarCalculator::equationOfTime(double t) const {
    double epsilon = obliquityCorrection(t);
    double l0 = sunGeomMeanLongitude(t);
    double e = earthOrbitEccentricity(t);
    double m = sunGeomMeanAnomaly(t);
    
    double y = std::tan((epsilon / 2.0) * M_PI / 180.0);
    y *= y;
    
    double sin2l0 = std::sin(2.0 * l0 * M_PI / 180.0);
    double sinm = std::sin(m * M_PI / 180.0);
    double cos2l0 = std::cos(2.0 * l0 * M_PI / 180.0);
    double sin4l0 = std::sin(4.0 * l0 * M_PI / 180.0);
    double sin2m = std::sin(2.0 * m * M_PI / 180.0);
    
    double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 -
                   0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
    
    return 4.0 * Etime * 180.0 / M_PI; // in minutes
}

double SolarCalculator::hourAngleSunrise(double latitude, double declination, double elevation) const {
    double latRad = latitude * M_PI / 180.0;
    double declRad = declination * M_PI / 180.0;
    
    // Atmospheric refraction correction for elevation
    double elevationCorrection = -2.076 * std::sqrt(elevation) / 60.0;
    double zenith = 90.0 + SOLAR_DEPRESSION + elevationCorrection;
    
    double cosHA = (std::cos(zenith * M_PI / 180.0) / (std::cos(latRad) * std::cos(declRad))) -
                   std::tan(latRad) * std::tan(declRad);
    
    if (cosHA > 1.0) {
        // Sun never rises
        return -1.0;
    }
    if (cosHA < -1.0) {
        // Sun never sets
        return -2.0;
    }
    
    return std::acos(cosHA) * 180.0 / M_PI;
}

double SolarCalculator::calculateSolarTime(double latitude, double longitude, double elevation,
                                          int year, int month, int day, bool isSunrise) const {
    double jd = julianDay(year, month, day);
    double t = julianCentury(jd);
    
    double eqTime = equationOfTime(t);
    double declination = sunDeclination(t);
    double ha = hourAngleSunrise(latitude, declination, elevation);
    
    if (ha < 0.0) {
        // Sun never rises or sets
        return -9999.0;
    }
    
    // Calculate solar noon
    double solarNoon = (720.0 - 4.0 * longitude - eqTime) / 60.0;
    
    // Calculate sunrise or sunset
    double solarTime;
    if (isSunrise) {
        solarTime = solarNoon - ha * 4.0 / 60.0;
    } else {
        solarTime = solarNoon + ha * 4.0 / 60.0;
    }
    
    // Convert to local time
    solarTime += timezoneOffset_;
    
    // Ensure valid range [0, 24)
    while (solarTime < 0.0) solarTime += 24.0;
    while (solarTime >= 24.0) solarTime -= 24.0;
    
    return solarTime;
}
