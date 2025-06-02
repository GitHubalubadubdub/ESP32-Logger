#ifndef BLE_MANAGER_TASK_H
#define BLE_MANAGER_TASK_H

#include <Arduino.h>

// Functions to be called by other tasks to get BLE data
uint16_t getPower();
uint8_t getCadence();
String getBLEStatus(); // e.g., "Scanning", "Connected", "Disconnected"

void bleManagerTask(void *pvParameters);

#endif // BLE_MANAGER_TASK_H
