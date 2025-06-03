#ifndef BLE_MANAGER_TASK_H
#define BLE_MANAGER_TASK_H

#include <Arduino.h>
#include <NimBLEDevice.h> // Full NimBLE device support
#include "config.h"       // For g_powerCadenceData and g_dataMutex
// types.h is likely included via config.h now

// UUIDs for Cycling Power Service and Characteristic
#define CYCLING_POWER_SERVICE_UUID "00001818-0000-1000-8000-00805f9b34fb"
#define CYCLING_POWER_MEASUREMENT_UUID "00002a63-0000-1000-8000-00805f9b34fb"

// Functions to be called by other tasks (if still needed, review their usage)
// uint16_t getPower(); // Will be replaced by g_powerCadenceData
// uint8_t getCadence(); // Will be replaced by g_powerCadenceData
// String getBLEStatus(); // Might be useful, or handled differently

// New functions for device discovery and selection (if used by this direct connection approach, otherwise remove)
// void startBleScan();
// int getDiscoveredDeviceCount();
// NimBLEAdvertisedDevice* getDiscoveredDevice(int index);
// void setSelectedDeviceIndex(int index);
// int getSelectedDeviceIndex();
// bool connectToSelectedDevice();

void bleManagerTask(void *pvParameters);

#endif // BLE_MANAGER_TASK_H
