#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>

struct DebugSettings {
    bool gpsDebugStreamOn = false; // Default to off
    bool bleDebugStreamOn = false; // Default to off
};

extern DebugSettings g_debugSettings;
extern SemaphoreHandle_t g_debugSettingsMutex;
