#include <Arduino.h>
#include "config.h"
#include "DataAcquisitionTask.h"
#include "SdLoggingTask.h"
#include "DisplayUpdateTask.h"
#include "BleManagerTask.h"
#include "DataBuffer.h" // Needed for psramDataBuffer definition
// #include "WifiHandlerTask.h" // Uncomment when ready

// Global variable definitions
DataBuffer<LogRecordV1> psramDataBuffer(PSRAM_BUFFER_SIZE_RECORDS);
SystemState currentSystemState = STATE_INITIALIZING; // Define currentSystemState here

void setup() {
    Serial.begin(115200);
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 2000)); // Wait for serial connection (2s timeout)
    Serial.println("ESP32 Data Logger Starting...");

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
    xTaskCreatePinnedToCore(displayUpdateTask, "DisplayTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(bleManagerTask, "BLETask", 8192, NULL, 4, NULL, 1); 
    // xTaskCreatePinnedToCore(wifiHandlerTask, "WiFiTask", 4096, NULL, 3, NULL, 1);

    Serial.println("Setup complete. Tasks will start soon.");
}

void loop() {
    vTaskDelay(portMAX_DELAY); // Sleep indefinitely
}
