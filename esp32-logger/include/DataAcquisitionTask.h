#ifndef DATA_ACQUISITION_TASK_H
#define DATA_ACQUISITION_TASK_H

#include <Arduino.h>
#include "types.h" // For LogRecordV1

void dataAcquisitionTask(void *pvParameters);

// Functions for sensor initialization (to be called from this task or setup)
bool initializeGPS();
bool initializeIMU();
// bool initializePowerMeterBLE(); // Future

// Functions to get latest data from sensors
// These would update a shared structure or be directly read by the acq task
// GpsData getLatestGpsData();
// ImuData getLatestImuData();
// PowerData getLatestPowerData();

#endif // DATA_ACQUISITION_TASK_H
