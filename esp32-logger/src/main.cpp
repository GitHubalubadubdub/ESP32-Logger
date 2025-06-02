#include <Arduino.h>
#include "config.h"
#include "DataAcquisitionTask.h"
#include "SdLoggingTask.h"
#include "DisplayUpdateTask.h"
// #include "BleManagerTask.h" // Uncomment when ready
// #include "WifiHandlerTask.h" // Uncomment when ready

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000); // Wait for serial connection
    Serial.println("ESP32 Data Logger Starting...");

    // Initialize PSRAM if available
    #if CONFIG_SPIRAM_SUPPORT
    if (psramFound()) {
        Serial.println("PSRAM initialized");
    } else {
        Serial.println("PSRAM not found or initialization failed!");
    }
    #endif

    // Create FreeRTOS Tasks
    // xTaskCreatePinnedToCore(dataAcquisitionTask, "DataAcqTask", 4096, NULL, 5, NULL, 0); // Core 0 for time-critical
    // xTaskCreatePinnedToCore(sdLoggingTask, "SDLogTask", 4096, NULL, 3, NULL, 1);       // Core 1 for other tasks
    // xTaskCreatePinnedToCore(displayUpdateTask, "DisplayTask", 4096, NULL, 2, NULL, 1);
    // xTaskCreatePinnedToCore(bleManagerTask, "BLETask", 4096, NULL, 4, NULL, 1);
    // xTaskCreatePinnedToCore(wifiHandlerTask, "WiFiTask", 4096, NULL, 3, NULL, 1);

    Serial.println("Setup complete. Tasks will start soon.");
}

void loop() {
    // Main loop is not used with FreeRTOS tasks handling everything.
    vTaskDelay(portMAX_DELAY); // Sleep indefinitely
}
