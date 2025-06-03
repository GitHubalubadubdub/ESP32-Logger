#include <Arduino.h>
#include "config.h" // This should now include types.h and FreeRTOS headers
#include "DisplayUpdateTask.h"
#include "BleManagerTask.h" // Include BLE Manager Task header
#include "gps_handler.h"    // For gpsTask
#include "gps_data.h"       // For g_gpsDataMutex


// Global variable definitions
SystemState currentSystemState = STATE_INITIALIZING; // Define currentSystemState here
PowerCadenceData g_powerCadenceData;
SemaphoreHandle_t g_dataMutex;
// g_gpsDataMutex is defined in gps_handler.cpp, declared extern in gps_data.h
// GpsData g_gpsData is defined in gps_handler.cpp, declared extern in gps_data.h


void setup() {
    Serial.begin(115200);
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 2000)); // Wait for serial connection (2s timeout)
    Serial.println("ESP32 Data Logger Starting...");

    // Initialize Mutexes
    g_dataMutex = xSemaphoreCreateMutex();
    if (g_dataMutex == NULL) {
        Serial.println("Failed to create data mutex!");
        while(1); // Halt execution
    } else {
        Serial.println("Data mutex created successfully.");
    }

    g_gpsDataMutex = xSemaphoreCreateMutex();
    if (g_gpsDataMutex == NULL) {
        Serial.println("Failed to create GPS data mutex!");
        while(1); // Halt execution
    } else {
        Serial.println("GPS data mutex created successfully.");
    }

    // Initialize PSRAM if available
    #if CONFIG_SPIRAM_SUPPORT
    if (psramFound()) {
        Serial.println("PSRAM found");
        if(!psramDataBuffer.initialize()){ 
            Serial.println("PSRAM Buffer initialization failed!");
            // Handle error - perhaps by halting or indicating via LED
            // For now, continue, but logging/data storage will fail.
            // Consider setting an error state: currentSystemState = STATE_PSRAM_ERROR; (new state needed)
        } else {
           Serial.println("PSRAM Buffer initialized.");
        }
    } else {
        Serial.println("PSRAM not found!"); // Simplified message
        // Handle error - PSRAM is critical for the buffer
        // currentSystemState = STATE_PSRAM_ERROR; 
    }
    #else
    Serial.println("PSRAM support not compiled.");
    // PSRAM is critical, if not compiled in, this is a configuration error for this app.
    // currentSystemState = STATE_PSRAM_ERROR;
    #endif

    // Create FreeRTOS Tasks
    // Priority reminder: Higher number = higher priority
    // Core 0 for time-critical tasks if any, Core 1 for others / comms
    // xTaskCreatePinnedToCore(dataAcquisitionTask, "DataAcqTask", 4096, NULL, 5, NULL, 0);
    // xTaskCreatePinnedToCore(sdLoggingTask, "SDLogTask", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(displayUpdateTask, "DisplayTask", 4096, NULL, 2, NULL, 0); // Uses g_dataMutex & g_gpsDataMutex
    xTaskCreatePinnedToCore(bleManagerTask, "BLETask", 8192, NULL, 4, NULL, 1);    // Uses g_dataMutex
    xTaskCreatePinnedToCore(gpsTask, "GPSTask", 4096, NULL, 3, NULL, 1);           // Uses g_gpsDataMutex
    Serial.println("GPS Task creation attempted."); // Confirmation message
    // xTaskCreatePinnedToCore(wifiHandlerTask, "WiFiTask", 4096, NULL, 3, NULL, 1);

    Serial.println("Setup complete. All tasks created. System should be running.");
}

void loop() {
    vTaskDelay(portMAX_DELAY); // Sleep indefinitely
}
