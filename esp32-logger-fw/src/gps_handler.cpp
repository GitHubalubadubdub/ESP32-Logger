#include "gps_handler.h"
#include "shared_state.h" // Added for g_debugSettings
#include "gps_data.h" // For GpsData struct and g_gpsData externs
#include "config.h"   // For GPS_RX_PIN, GPS_TX_PIN if used directly (or through defines below)

#include <Arduino.h>
#include <HardwareSerial.h> // For Serial2
#include <Adafruit_GPS.h>   // Adafruit GPS library

// Define GPS UART settings
// #define GPS_SERIAL_NUM 2 // Using Serial2 - Serial2 is directly used
#define GPS_BAUD_RATE 9600 // Default baud rate, user to verify from datasheet

// Use pins from config.h. These are already defined there.
// #define GPS_RX_PIN_CONFIG GPS_RX_PIN // GPIO18 in config.h
// #define GPS_TX_PIN_CONFIG GPS_TX_PIN // GPIO17 in config.h
// We will use GPS_RX_PIN and GPS_TX_PIN directly from config.h in Serial2.begin()

// Global definitions for this file (g_gpsData and g_gpsDataMutex are declared extern in gps_data.h)
GpsData g_gpsData; // Definition of the global GPS data structure
SemaphoreHandle_t g_gpsDataMutex; // Definition of the global GPS data mutex

Adafruit_GPS GPS(&Serial2); // Adafruit GPS object using Serial2

// Initialization function for GPS module specific commands (e.g., update rate)
// This will be called once from the gpsTask.
static void initializeGpsModule() {
    // Initialize Serial2 for GPS communication
    Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("GPS Handler: Serial2 initialized with pins RX=" + String(GPS_RX_PIN) + ", TX=" + String(GPS_TX_PIN) + " at " + String(GPS_BAUD_RATE) + " baud.");

    // Initialize Adafruit_GPS library
    GPS.begin(GPS_BAUD_RATE); // Initialize library's internal state for the baud rate

    // Configure GPS module
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // Request RMC and GGA sentences
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);   // Set NMEA update rate to 10Hz
    // For other rates: PMTK_SET_NMEA_UPDATE_1HZ, PMTK_SET_NMEA_UPDATE_5HZ, etc.
    // Or use GPS.sendCommand("$PMTK220,100*2F"); for 10Hz (100ms)

    Serial.println("Adafruit_GPS initialized, NMEA output set to RMCGGA, update rate set to 10Hz (attempted).");
    // Add a small delay to allow the module to process commands if needed.
    // delay(100); // Usually not strictly necessary for these commands
}

void gpsTask(void *pvParameters) {
    Serial.println("GPS Task started.");
    initializeGpsModule();

    for (;;) {
        // GPS Task Loop Alive and Serial2 Available messages are general debug, not continuous stream
        // Serial.println("GPS Task Loop Alive");
        // Serial.printf("GPS Serial2 Available (Before Read Loop): %d\n", Serial2.available());

        bool char_read_this_cycle = false;
        while (Serial2.available() > 0) {
            char c = GPS.read();
            char_read_this_cycle = true;
            // Conditional printing for raw NMEA character stream
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.gpsDebugStreamOn) {
                    // Serial.print(c); // Example: If you want to print raw NMEA stream
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
        }

        if (GPS.newNMEAreceived()) {
            // Conditional printing for "newNMEAreceived"
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.gpsDebugStreamOn) {
                    Serial.println("GPS DEBUG: newNMEAreceived() is TRUE.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }

            char *lastNmeaSentence = GPS.lastNMEA();

            // Conditional printing for the NMEA sentence itself
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.gpsDebugStreamOn) {
                    // Serial.print("NMEA: "); Serial.println(lastNmeaSentence);
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }

            if (GPS.parse(lastNmeaSentence)) {
                // Conditional printing for "NMEA sentence PARSED successfully"
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.gpsDebugStreamOn) {
                        Serial.println("GPS DEBUG: NMEA sentence PARSED successfully!");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                if (xSemaphoreTake(g_gpsDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_gpsData.is_valid = GPS.fix;

                    if (g_gpsData.is_valid) {
                        g_gpsData.latitude = GPS.latitudeDegrees;
                        g_gpsData.longitude = GPS.longitudeDegrees;
                        g_gpsData.altitude_meters = GPS.altitude;
                        g_gpsData.speed_mps = GPS.speed * 0.514444f;
                        g_gpsData.satellites = GPS.satellites;
                        g_gpsData.fix_quality = GPS.fixquality;
                    } else {
                        // Keep old data or clear some fields if no fix
                        g_gpsData.latitude = 0.0; // Or keep stale data
                        g_gpsData.longitude = 0.0; // Or keep stale data
                        g_gpsData.altitude_meters = 0.0f;
                        g_gpsData.speed_mps = 0.0f;
                        g_gpsData.satellites = GPS.satellites; // Still useful to know how many sats are visible
                        g_gpsData.fix_quality = 0; // No fix
                    }
                    g_gpsData.last_update_millis = millis();

                    xSemaphoreGive(g_gpsDataMutex);

                    // Conditional printing for parsed GPS data
                    if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                        if (g_debugSettings.gpsDebugStreamOn) {
                            Serial.printf("GPS DEBUG: g_gpsData updated. Fix: %d, Q: %d, Sats: %d, Lat: %f, Lon: %f, Alt: %.1f, Spd: %.1f\n",
                                          (int)GPS.fix, (int)GPS.fixquality, (int)GPS.satellites,
                                          GPS.latitudeDegrees, GPS.longitudeDegrees, GPS.altitude, GPS.speed * 0.514444f);
                        }
                        xSemaphoreGive(g_debugSettingsMutex);
                    }
                } else {
                    // This is an operational message, not a continuous stream, so leave as is or make conditional if desired
                    Serial.println("GPS DEBUG: Failed to take g_gpsDataMutex to update g_gpsData.");
                }
            } else {
                // Conditional printing for "NMEA sentence FAILED to parse"
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.gpsDebugStreamOn) {
                        Serial.println("GPS DEBUG: NMEA sentence FAILED to parse.");
                        // Optional: Print the sentence that failed to parse
                        // Serial.print("Failed NMEA: "); Serial.println(lastNmeaSentence);
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
            }
        }
        // No 'else' here for newNMEAreceived() being false, to reduce log spam.
        // We are primarily interested in what happens when a sentence *is* supposedly received.
        // unsigned long chars_read_in_while_loop = 0; // if you implement counting inside while
        // Serial.printf("GPS Serial2 Available (After Read Loop): %d, Chars Read This Iter: %lu\n", Serial2.available(), chars_read_in_while_loop); // DEBUG

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
