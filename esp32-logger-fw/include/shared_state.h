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

// Global recording state
extern volatile bool is_recording; // True when logging data to SD card

// SD Card Status
typedef enum {
    SD_NOT_INITIALIZED,
    SD_OK,
    SD_FULL,
    SD_ERROR_INIT,
    SD_ERROR_OPEN,
    SD_ERROR_WRITE,
    SD_NOT_PRESENT // Could be same as SD_ERROR_INIT but more specific
} SdCardStatus_t;
extern volatile SdCardStatus_t g_sdCardStatus;
