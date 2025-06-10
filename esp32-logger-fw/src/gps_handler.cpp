#include "gps_handler.h"
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
        // Read all available characters from GPS
        while (Serial2.available() > 0) {
            char c = GPS.read();
            // Optional: For debugging, print characters received from GPS
            // if (GPSECHO) { Serial.print(c); } // Define GPSECHO if needed for debugging
        }

        // Check if a new NMEA sentence has been received and parsed by the library's internal interrupt handler
        if (GPS.newNMEAreceived()) {
            // Attempt to parse the last NMEA sentence stored by the library
            // GPS.parse also clears the newNMEAreceived() flag internally.
            if (GPS.parse(GPS.lastNMEA())) {
                // Successfully parsed a sentence, now update g_gpsData
                if (xSemaphoreTake(g_gpsDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_gpsData.is_valid = GPS.fix; // GPS.fix is a boolean (0 or 1)

                    if (g_gpsData.is_valid) {
                        g_gpsData.latitude = GPS.latitudeDegrees;
                        g_gpsData.longitude = GPS.longitudeDegrees;
                        g_gpsData.altitude_meters = GPS.altitude;       // meters
                        g_gpsData.speed_mps = GPS.speed * 0.514444f;  // Convert knots to m/s
                        g_gpsData.satellites = GPS.satellites;
                        g_gpsData.fix_quality = GPS.fixquality;
                        // fix_quality: 0 = No fix, 1 = GPS fix, 2 = DGPS fix, 3 = PPS fix, etc.
                    } else {
                        // If no fix, set some values to indicate invalidity or zero
                        g_gpsData.latitude = 0.0;
                        g_gpsData.longitude = 0.0;
                        g_gpsData.altitude_meters = 0.0f;
                        g_gpsData.speed_mps = 0.0f;
                        // GPS.satellites might still report count even without fix,
                        // but setting to 0 if !GPS.fix is common.
                        g_gpsData.satellites = GPS.satellites; // Or 0 if preferred when no fix
                        g_gpsData.fix_quality = 0;
                    }
                    g_gpsData.last_update_millis = millis();

                    xSemaphoreGive(g_gpsDataMutex);
                } else {
                    Serial.println("GPS Task: Failed to take mutex to update g_gpsData.");
                }
            }
            // If GPS.parse() returns false, the sentence was not successfully parsed.
            // The newNMEAreceived flag is already handled by GPS.parse() or by accessing GPS.lastNMEA().
        }

        // Task delay.
        // If update rate is 10Hz (100ms), parsing should be quick.
        // A delay of 20-50ms allows other tasks to run without missing GPS updates.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
