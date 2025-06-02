#ifndef BLE_MANAGER_TASK_H
#define BLE_MANAGER_TASK_H

#include <Arduino.h>
#include <NimBLEAdvertisedDevice.h> // Required for NimBLEAdvertisedDevice
#include <vector>                   // Required for std::vector

// Functions to be called by other tasks
uint16_t getPower();
uint8_t getCadence();
String getBLEStatus(); 

// New functions for device discovery and selection
void startBleScan(); // Renamed for clarity, initiates a new scan
int getDiscoveredDeviceCount();
NimBLEAdvertisedDevice* getDiscoveredDevice(int index); // Returns a pointer, callee should not delete
void setSelectedDeviceIndex(int index);
int getSelectedDeviceIndex();
bool connectToSelectedDevice(); // Initiates connection to the selected device

void bleManagerTask(void *pvParameters);

#endif // BLE_MANAGER_TASK_H
