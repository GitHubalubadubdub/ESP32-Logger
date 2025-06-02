#include <Arduino.h>
#include <Wire.h>    // For I2C communication (IMU, etc.)
#include <SPI.h>     // For SPI communication (SD, TFT)
#include "DataStructures.h"
#include "PSRAMBuffer.h"
#include "SensorModules.h"
#include "DataAcquisitionTask.h"
#include "SDLoggingTask.h"
#include "DisplayTask.h"

// FreeRTOS Task Handles
TaskHandle_t xDataAcquisitionTaskHandle = NULL;
TaskHandle_t xSDLoggingTaskHandle = NULL;
TaskHandle_t xDisplayTaskHandle = NULL;
// Add other task handles if needed (e.g., BLE, WiFi)

// I2C pins for ESP32-S3 Reverse TFT (defined in SensorModules.cpp as well, ensure consistency or centralize)
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// Prioritize a common Wire.begin if multiple modules use it.
void initializeI2C() {
    Serial.println("Initializing I2C bus...");
    // Initialize I2C with specified pins
    // Wire.begin(SDA, SCL, frequency);
    // For the Feather S3 TFT, default I2C pins are 8 (SDA) and 9 (SCL)
    if (Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("I2C initialized successfully.");
    } else {
        Serial.println("I2C initialization failed!");
        // Handle error - perhaps by halting or indicating critical failure
    }
}


void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000); // Wait for serial connection
    Serial.println("ESP32-S3 Data Logger Initializing...");

    // Initialize I2C bus first as IMU (and other potential sensors) will need it
    initializeI2C();

    // Initialize PSRAM Buffer
    if (!initializePSRAMBuffer()) {
        Serial.println("CRITICAL: PSRAM Buffer initialization failed! Halting.");
        // Display error on TFT if possible, then halt or enter safe mode
        // For now, we'll just loop indefinitely.
        while(1) { delay(1000); }
    }

    // Initialize Display
    initializeDisplay(); // Shows "Logger Booting..."
    updateDisplayInfo("Initializing...", 0,0,0,0, false);


    // Initialize Sensors
    updateDisplayInfo("Init IMU...", 0,0,0,0, false);
    if (!initializeIMU()) {
        Serial.println("IMU initialization failed!");
        updateDisplayInfo("IMU Fail", 0,0,0,0, false);
        // Decide if this is critical; for now, continue
    } else {
      Serial.println("IMU Initialized.");
    }

    updateDisplayInfo("Init GPS...", 0,0,0,0, sd_card_initialized); // sd_card_initialized is false here
    if (!initializeGPS()) {
        Serial.println("GPS initialization failed!");
        updateDisplayInfo("GPS Fail", 0,0,0,0, sd_card_initialized);
        // Decide if this is critical
    } else {
      Serial.println("GPS Initialized.");
    }
    
    // Initialize SD Card (after display and other peripherals if they share SPI)
    // The SD card task will manage its state. Call initializeSDCard here,
    // or let the task do it. For now, call it here to see status on display.
    updateDisplayInfo("Init SD...", 0,0,0,0, false);
    sd_card_initialized = initializeSDCard(); // sd_card_initialized is global in SDLoggingTask.cpp
    if (!sd_card_initialized) {
        Serial.println("SD Card initialization failed!");
        updateDisplayInfo("SD Fail", 0,0,0,0, false);
        // Logging will not work, but other functions might continue
    } else {
        Serial.println("SD Card Initialized.");
        updateDisplayInfo("SD OK", 0,0,0,0, true);
    }

    // Initialize BLE Client (if included in this phase)
    // updateDisplayInfo("Init BLE...", 0,0,0,0, sd_card_initialized);
    // if (!initializeBLEClient()) {
    //     Serial.println("BLE Client initialization failed!");
    //     updateDisplayInfo("BLE Fail", 0,0,0,0, sd_card_initialized);
    // }

    // Create FreeRTOS Tasks
    Serial.println("Creating FreeRTOS tasks...");
    updateDisplayInfo("Starting Tasks...", 0,0,0,0, sd_card_initialized);

    // Data Acquisition Task (High Priority)
    xTaskCreatePinnedToCore(
        dataAcquisitionTask,        // Task function
        "DataAcquisitionTask",      // Name of the task
        8192,                       // Stack size in words
        NULL,                       // Task input parameter
        5,                          // Priority of the task (configMAX_PRIORITIES - 1 for highest)
        &xDataAcquisitionTaskHandle, // Task handle
        1                           // Core where the task should run (e.g., Core 1 for ESP32 dual-core)
    );

    // SD Logging Task (Lower priority than data acquisition)
    xTaskCreatePinnedToCore(
        sdLoggingTask,              // Task function
        "SDLoggingTask",            // Name of the task
        4096,                       // Stack size in words
        NULL,                       // Task input parameter
        2,                          // Priority of the task
        &xSDLoggingTaskHandle,      // Task handle
        0                           // Core where the task should run (e.g., Core 0)
    );

    // Display Update Task (Low Priority)
    xTaskCreatePinnedToCore(
        displayTask,                // Task function
        "DisplayTask",              // Name of the task
        4096,                       // Stack size in words
        NULL,                       // Task input parameter
        1,                          // Priority of the task
        &xDisplayTaskHandle,        // Task handle
        0                           // Core where the task should run (e.g., Core 0)
    );
    
    // TODO: Create BLE Manager Task
    // TODO: Create WiFi Handler Task (on demand)

    initializeDataAcquisition(); // Starts the 200Hz mechanism (e.g. timer)

    Serial.println("All tasks created. Scheduler starting.");
    updateDisplayInfo("Running...", 0,0,0,0, sd_card_initialized);
    // FreeRTOS scheduler is started automatically by the Arduino framework for ESP32
    // No need to call vTaskStartScheduler() explicitly
}

void loop() {
    // Main loop is typically kept empty or used for very low priority tasks
    // when using FreeRTOS. The tasks handle all the work.
    processGPS(); // Call GPS processing frequently from a low-priority context like loop() or its own low-prio task
                  // This is important as GPS data comes in via UART and needs polling/parsing.
    vTaskDelay(pdMS_TO_TICKS(1000)); // Keep loop() from starving idle task, not strictly necessary if tasks are well-behaved.
    // Serial.println("Main loop heartbeat...");
}
