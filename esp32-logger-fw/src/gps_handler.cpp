#include "gps_handler.h"
#include "gps_data.h" // For GpsData struct and g_gpsData externs
#include "config.h"   // For GPS_RX_PIN, GPS_TX_PIN if used directly (or through defines below)

#include <Arduino.h>
#include <HardwareSerial.h> // For Serial2
#include <TinyGPS++.h>    // GPS parsing library

// Define GPS UART settings
#define GPS_SERIAL_NUM 2 // Using Serial2
#define GPS_BAUD_RATE 9600 // Default baud rate, user to verify from datasheet

// Use pins from config.h. These are already defined there.
// #define GPS_RX_PIN_CONFIG GPS_RX_PIN // GPIO18 in config.h
// #define GPS_TX_PIN_CONFIG GPS_TX_PIN // GPIO17 in config.h
// We will use GPS_RX_PIN and GPS_TX_PIN directly from config.h in Serial2.begin()

// Global definitions for this file (g_gpsData and g_gpsDataMutex are declared extern in gps_data.h)
GpsData g_gpsData; // Definition of the global GPS data structure
SemaphoreHandle_t g_gpsDataMutex; // Definition of the global GPS data mutex

TinyGPSPlus gpsParser; // TinyGPS++ object

// Initialization function for GPS module specific commands (e.g., update rate)
// This will be called once from the gpsTask.
static void initializeGpsModule() {
    // Initialize Serial2 for GPS communication
    // ESP32-S3 allows specifying custom RX/TX pins for HardwareSerial
    Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("GPS Handler: Serial2 initialized with pins RX=" + String(GPS_RX_PIN) + ", TX=" + String(GPS_TX_PIN) + " at " + String(GPS_BAUD_RATE) + " baud.");

    // Placeholder: Send command to GPS module to set update frequency to maximum supported.
    // This is an example for a common MediaTek chipset (e.g., MTK3339) to set 10Hz.
    // The actual command and checksum will depend on the specific GPS module used.
    // User needs to verify this from their GPS module's datasheet.
    // Example: Serial2.println("$PMTK220,100*2F"); // 100ms interval = 10Hz
    // Example: Serial2.println("$PGCMD,16,0,0,0,0,0*6A"); // Another potential command type
    Serial.println("GPS Handler: Placeholder for sending update rate configuration command to GPS module.");
    // Add a small delay to allow the module to process the command if sent.
    // delay(100); // Only if a command is actually sent.
}

void gpsTask(void *pvParameters) {
    Serial.println("GPS Task started.");
    initializeGpsModule();

    unsigned long lastSuccessfulFixTime = 0;

    for (;;) {
        bool dataReceived = false;
        while (Serial2.available() > 0) {
            if (gpsParser.encode(Serial2.read())) {
                dataReceived = true; // Mark that we've received and processed data via encode
            }
        }

        // Check if new, valid data has been parsed after processing all available serial characters
        if (dataReceived) { // Process only if gps.encode() returned true for any character (meaning a sentence completed)
            if (xSemaphoreTake(g_gpsDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_gpsData.is_valid = gpsParser.location.isValid() && gpsParser.satellites.isValid() && gpsParser.satellites.value() > 0;

                if (g_gpsData.is_valid) {
                    g_gpsData.latitude = gpsParser.location.lat();
                    g_gpsData.longitude = gpsParser.location.lng();
                    lastSuccessfulFixTime = millis(); // Keep track of last good fix
                }
                // Update other fields even if fix is not valid, so "no fix" is current
                g_gpsData.altitude_meters = gpsParser.altitude.isValid() ? gpsParser.altitude.meters() : 0.0f;
                g_gpsData.speed_mps = gpsParser.speed.isValid() ? gpsParser.speed.mps() : 0.0f;
                g_gpsData.satellites = gpsParser.satellites.isValid() ? gpsParser.satellites.value() : 0;

                // Fix quality: 0 = No fix, 1 = GPS fix (SPS), 2 = DGPS fix, ...
                // TinyGPS++ doesn't directly give NMEA GxGGA's fix quality field easily.
                // We infer: 0 if not valid location. 1 if valid location and basic GPS.
                // If HDOP is available and good, could indicate a better quality fix (like DGPS or RTK if module supports).
                // For simplicity: 0 if not valid, 1 if valid. User can refine.
                if (!g_gpsData.is_valid) {
                    g_gpsData.fix_quality = 0; // No fix
                } else {
                    if (gpsParser.hdop.isValid() && gpsParser.hdop.value() < 150 && gpsParser.hdop.value() > 0) { // hdop is in 0.01 units
                        g_gpsData.fix_quality = 2; // Good fix (like DGPS, assuming low HDOP means differential or better)
                    } else {
                        g_gpsData.fix_quality = 1; // Standard GPS fix
                    }
                }

                g_gpsData.last_update_millis = millis(); // Timestamp of when g_gpsData was last updated

                xSemaphoreGive(g_gpsDataMutex);
            } else {
                Serial.println("GPS Task: Failed to take mutex to update g_gpsData.");
            }
        }

        // If no valid fix for a while, explicitly mark data as invalid
        if (g_gpsData.is_valid && (millis() - lastSuccessfulFixTime > 5000)) { // 5 seconds timeout
             if (xSemaphoreTake(g_gpsDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_gpsData.is_valid = false;
                g_gpsData.fix_quality = 0;
                Serial.println("GPS Task: Marking GPS data as invalid due to timeout since last fix.");
                xSemaphoreGive(g_gpsDataMutex);
             }
        }

        // Task delay. GPS data comes at its own rate (e.g., 1Hz or 10Hz).
        // This delay is for the FreeRTOS scheduler to allow other tasks to run.
        // A short delay is fine as the main work is event-driven by Serial2.available().
        vTaskDelay(pdMS_TO_TICKS(20)); // Yield for other tasks, e.g., 50Hz loop
    }
}
