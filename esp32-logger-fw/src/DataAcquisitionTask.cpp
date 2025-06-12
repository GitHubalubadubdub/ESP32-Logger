#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "LogDataStructure.h" // Created in src, assuming compiler path includes src
#include "gps_data.h"         // For g_gpsData
#include "shared_state.h"     // For is_recording
#include "ble_data_types.h"   // For BlePowerMetricsData_t

// Assuming g_powerMetricsData might be part of shared_state.h or another specific BLE header.
// If BleData.h is a specific file, it should be included here.
// For now, we'll use the placeholder structure from the example if not found in shared_state.h

// --- Placeholder for actual data structures (if not in included headers) ---
// Ensure these are declared volatile if accessed by ISRs or other tasks directly,
// or use appropriate mutexes/semaphores for thread-safe access.

// g_gpsData is expected to be defined in gps_data.h (or sourced through it)
extern GpsData g_gpsData; // Ensure this matches the definition in gps_data.h

// is_recording is expected to be defined in shared_state.h
extern volatile bool is_recording;

// TODO: This definition of g_powerMetricsData is a temporary measure to resolve linking.
// It should ideally be defined in the .cpp file responsible for managing BLE data
// (e.g., BleManagerTask.cpp or similar) and declared extern here.
BlePowerMetricsData_t g_powerMetricsData = {0.0f, 0, 0.0f}; // Initialize to default values
// --- End Placeholder ---


// Declare the queue handle as extern, it will be defined elsewhere (e.g., main.cpp)
extern QueueHandle_t xLoggingQueue;

// Task function
void dataAcquisitionTask(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(5); // 200Hz (5ms period)
    TickType_t xLastWakeTime = xTaskGetTickCount();
    LogRecordV1 logRecord;

    while (1) {
        // Wait for the next cycle.
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (is_recording) {
            // Populate timestamp
            logRecord.timestamp_us = esp_timer_get_time();

            // Populate GPS data
            // Consider mutex locks if g_gpsData can be written by another task/ISR concurrently
            logRecord.latitude = g_gpsData.latitude;
            logRecord.longitude = g_gpsData.longitude;
            logRecord.altitude_m = 0.0f; // Placeholder, original field missing in GpsData
            logRecord.speed_mps = g_gpsData.speed_mps;
            logRecord.heading_deg = 0.0f; // Placeholder, original field missing in GpsData
            logRecord.satellites = g_gpsData.satellites;
            logRecord.fix_quality = g_gpsData.fix_quality;

            // Populate BLE Power Meter data
            // Consider mutex locks if g_powerMetricsData can be written by another task/ISR concurrently
            logRecord.power_w = g_powerMetricsData.power_w;
            logRecord.cadence_rpm = g_powerMetricsData.cadence_rpm;
            logRecord.left_right_balance = g_powerMetricsData.left_right_balance;

            // Populate IMU data with placeholders
            logRecord.accel_x_g = 0.0f; // Or NAN
            logRecord.accel_y_g = 0.0f; // Or NAN
            logRecord.accel_z_g = 0.0f; // Or NAN
            logRecord.gyro_x_dps = 0.0f; // Or NAN
            logRecord.gyro_y_dps = 0.0f; // Or NAN
            logRecord.gyro_z_dps = 0.0f; // Or NAN

            // Send to the queue
            // The last parameter is the block time. 0 means don't block if the queue is full.
            if (xQueueSend(xLoggingQueue, &logRecord, (TickType_t)0) != pdPASS) {
                // Handle queue full error, e.g., log an error message via Serial
                Serial.println("DataAcquisitionTask: Failed to send to logging queue (full?)");
            }
        }
    }
}
