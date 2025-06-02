#ifndef SENSOR_MODULES_H
#define SENSOR_MODULES_H

#include <Arduino.h>
#include "DataStructures.h" // For LogRecordV1 access if needed for direct filling

// IMU Functions
bool initializeIMU();
bool readIMU(float* ax, float* ay, float* az, float* gx, float* gy, float* gz);

// GPS Functions
bool initializeGPS();
void processGPS(); // This would be called frequently to parse UART data

// BLE Functions (Power Meter) - More complex, likely its own class/task eventually
bool initializeBLEClient();
void scanAndConnectBLE();
// void getLatestGPSData(LogRecordV1* record); // Example of how data might be fetched
// void getLatestPowerData(LogRecordV1* record); // Example


#endif // SENSOR_MODULES_H
