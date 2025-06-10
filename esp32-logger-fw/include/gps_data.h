#ifndef GPS_DATA_H
#define GPS_DATA_H

#include <Arduino.h> // For uintX_t types, bool, unsigned long
#include <FreeRTOS.h>
#include <semphr.h> // For SemaphoreHandle_t

// Example shared GPS data structure
struct GpsData {
    double latitude = 0.0;
    double longitude = 0.0;
    float altitude_meters = 0.0;
    float speed_mps = 0.0;
    uint32_t satellites = 0;
    uint8_t fix_quality = 0; // 0: No fix, 1: GPS, 2: DGPS, etc. (based on NMEA)
    bool is_valid = false;    // True if data is recent and has a fix
    unsigned long last_update_millis = 0;
};

extern GpsData g_gpsData;
extern SemaphoreHandle_t g_gpsDataMutex;

#endif // GPS_DATA_H
