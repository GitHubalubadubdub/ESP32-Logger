#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>

struct DebugSettings {
    bool gpsDebugStreamOn = false; // Default to off
    bool bleDebugStreamOn = false; // Default to off for specific BLE debugs
    bool otherDebugStreamOn = false; // Default to off for other generic debug messages
    bool bleActivityStreamOn = false; // Default to off for verbose BLE activity stream
};

extern volatile DebugSettings g_debugSettings;
extern SemaphoreHandle_t g_debugSettingsMutex;
