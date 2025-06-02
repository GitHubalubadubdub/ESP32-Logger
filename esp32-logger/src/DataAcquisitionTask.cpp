#include "DataAcquisitionTask.h"
#include "PSRAMBuffer.h" // To write data to buffer
#include "SensorModules.h" // To get data from sensors
#include <Arduino.h> // Required for FreeRTOS.h
#include <FreeRTOS.h> // Required for task utilities

// Task handle for the data acquisition task
TaskHandle_t xDataAcquisitionTaskHandle = NULL;

// Timer handle for 200Hz triggering (if using a software timer)
TimerHandle_t xDataTimer = NULL;

volatile bool data_acquisition_active = false;

// This function will be called by the timer or an ISR
void IRAM_ATTR onTimer() {
    // Give a semaphore or set a flag to unblock dataAcquisitionTask
    // For simplicity here, we'll use a flag and the task will poll it,
    // but a semaphore would be more efficient.
    // This is a placeholder for the actual trigger mechanism.
    // A hardware timer ISR would be best for 200Hz.
}

void initializeDataAcquisition() {
    Serial.println("Initializing Data Acquisition...");
    // TODO: Setup a hardware timer for precise 200Hz triggering
    // For now, this is a placeholder.
    // Example: xDataTimer = xTimerCreate("DataTimer", pdMS_TO_TICKS(5), pdTRUE, (void *)0, onTimer);
    // xTimerStart(xDataTimer, 0);
    data_acquisition_active = true; // Placeholder
    Serial.println("Data Acquisition initialized.");
}

void dataAcquisitionTask(void *pvParameters) {
    Serial.println("Data Acquisition Task started");
    LogRecordV1 current_record;

    // Initialize last known values (or set to NAN/default)
    // These would be updated by sensor modules when new data is available
    current_record.gps_latitude = NAN;
    current_record.gps_longitude = NAN;
    current_record.gps_altitude = NAN;
    current_record.gps_speed_mps = NAN;
    current_record.gps_sats = 0;
    current_record.gps_fix_type = 0;
    current_record.power_watts = 0;
    current_record.cadence_rpm = 0;
    // ... initialize other fields ...
    for(int i=0; i<8; ++i) current_record.analog_ch[i] = NAN;


    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5); // 200Hz

    while (data_acquisition_active) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Precise 200Hz loop

        current_record.system_timestamp_ms = millis(); // Or use FreeRTOS tick count or RTC

        // --- GPS Data ---
        // In a real scenario, GPS data is updated by its own task/ISR
        // Here, we just use the last known value or fetch if available
        // getLatestGPSData(&current_record); // Example function call

        // --- Power Meter Data ---
        // BLE data also comes asynchronously
        // getLatestPowerData(&current_record); // Example function call

        // --- IMU Data ---
        // IMU should be read directly here for 200Hz
        readIMU(&current_record.imu_accel_x_mps2, &current_record.imu_accel_y_mps2, &current_record.imu_accel_z_mps2,
                &current_record.imu_gyro_x_radps, &current_record.imu_gyro_y_radps, &current_record.imu_gyro_z_radps);

        // --- Analog Channels (Placeholder) ---
        // For now, they remain NAN or zero as initialized

        // Write to PSRAM Buffer
        if (!writeToPSRAMBuffer(&current_record)) {
            Serial.println("Error: Failed to write to PSRAM Buffer!");
            // Handle buffer full or error condition
        }
        // Serial.print("."); // Debug: indicate a record was "processed"
    }
    vTaskDelete(NULL); // Clean up task if loop exits
}
