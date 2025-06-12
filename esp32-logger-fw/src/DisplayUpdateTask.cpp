#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7789.h" // Correct driver for ESP32-S3 Reverse TFT Feather
#include "shared_state.h"    // For is_recording, g_sdCardStatus, SdCardStatus_t
#include "config.h"          // May contain TFT pins if not using board defaults

// TFT object - declared as extern, assumed to be defined and initialized in main.cpp
// For Adafruit ESP32-S3 Reverse TFT Feather, specific pins are usually handled by BSP
// or a constructor that knows the board variant.
// Example: Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
// Or for integrated displays: Adafruit_ST7789 tft = Adafruit_ST7789(TFT_WIDTH, TFT_HEIGHT, TFT_SPI_PORT ...);
// For this board, it's often simpler:
extern Adafruit_ST7789 tft;


// Screen state
unsigned long recording_session_start_time = 0; // Time current recording session started
bool was_recording_last_cycle = false;       // To detect changes in is_recording state for screen clearing

// Helper to format time into HH:MM:SS
void formatElapsedTime(unsigned long ms, char* buf, size_t buf_len) {
    unsigned long total_seconds = ms / 1000;
    unsigned long hours = total_seconds / 3600;
    unsigned long minutes = (total_seconds % 3600) / 60;
    unsigned long seconds = total_seconds % 60;
    snprintf(buf, buf_len, "%02lu:%02lu:%02lu", hours, minutes, seconds);
}

void displayUpdateTask(void *pvParameters) {
    // TFT initialization (tft.begin(), tft.setRotation()) is assumed to be done in main.cpp's setup().
    // If not, it should be done here, guarded by a flag.

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(333); // Update rate: ~3Hz (333ms)

    // One-time setup on task start: ensure screen is clear and set base text properties
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextColor(ST77XX_WHITE); // Default text color
    tft.setTextSize(2);           // Default text size

    Serial.println("DisplayUpdateTask: Started and initialized.");

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        tft.fillScreen(ST77XX_CYAN); // Fill entire screen with CYAN
        // Serial.println("DisplayUpdateTask: fillScreen(CYAN) executed"); // Optional

        // --- Basic Drawing Test (Now Commented Out) ---
        // tft.fillRect(0, 0, 20, 20, ST77XX_RED);    // Red square at top-left
        // tft.fillRect(tft.width()-20, 0, 20, 20, ST77XX_GREEN); // Green square at top-right
        // tft.fillRect(0, tft.height()-20, 20, 20, ST77XX_BLUE);   // Blue square at bottom-left
        // --- End Basic Drawing Test ---

        // --- Detect recording state change for screen refresh ---
        // if (is_recording != was_recording_last_cycle) {
        //     Serial.printf("DisplayUpdateTask: Recording state changed from %d to %d. Clearing screen.\n", was_recording_last_cycle, is_recording);
        //     tft.fillScreen(ST77XX_BLACK); // Clear screen when recording state changes
        //     if (is_recording) {
        //         recording_session_start_time = millis();
        //     } else {
        //         recording_session_start_time = 0; // Reset if stopping
        //     }
        //     was_recording_last_cycle = is_recording;
        // }

        // --- Draw Logging Screen Elements ---
        // int16_t y_cursor = 15; // Initial Y position
        // const int16_t x_indent = 10;
        // const int16_t line_height = 20; // Approx height for size 2 text

        // // 1. Display Recording Status
        // tft.setCursor(x_indent, y_cursor);
        // tft.setTextSize(2);
        // if (is_recording) {
        //     tft.setTextColor(ST77XX_RED);
        //     tft.print("RECORDING ");
        //     // Draw a filled red circle symbol for recording
        //     // Adjust coordinates to be next to "RECORDING" text
        //     // Get current text bounds to position circle better if needed.
        //     // For simplicity, fixed offset:
        //     tft.fillCircle(x_indent + 135, y_cursor + 7, 7, ST77XX_RED);
        // } else {
        //     tft.setTextColor(ST77XX_GREEN);
        //     tft.print("STOPPED");
        // }
        // y_cursor += line_height + 5; // Move to next line

        // // 2. Display Elapsed Time
        // tft.setCursor(x_indent, y_cursor);
        // tft.setTextColor(ST77XX_WHITE);
        // tft.setTextSize(2);
        // tft.print("Time: ");
        // char timeBuf[10]; // HH:MM:SS + null
        // if (is_recording) { // Only show running time if actually recording
        //     formatElapsedTime(millis() - recording_session_start_time, timeBuf, sizeof(timeBuf));
        // } else {
        //     strcpy(timeBuf, "00:00:00");
        // }
        // tft.print(timeBuf);
        // y_cursor += line_height + 5;

        // // 3. Display SD Card Status
        // tft.setCursor(x_indent, y_cursor);
        // tft.setTextSize(2);
        // tft.print("SD: ");
        // // Determine color and text for SD status
        // uint16_t sd_status_color = ST77XX_WHITE;
        // String sd_status_text = "Unknown";

        // switch (g_sdCardStatus) {
        //     case SD_OK:
        //         sd_status_color = ST77XX_GREEN;
        //         sd_status_text = "OK";
        //         break;
        //     case SD_FULL:
        //         sd_status_color = ST77XX_YELLOW;
        //         sd_status_text = "Full!";
        //         break;
        //     case SD_ERROR_INIT:
        //         sd_status_color = ST77XX_RED;
        //         sd_status_text = "Init Err";
        //         break;
        //     case SD_ERROR_OPEN:
        //          sd_status_color = ST77XX_RED;
        //         sd_status_text = "Open Err";
        //         break;
        //     case SD_ERROR_WRITE:
        //          sd_status_color = ST77XX_RED;
        //         sd_status_text = "Write Err";
        //         break;
        //     case SD_NOT_PRESENT:
        //         sd_status_color = ST77XX_RED;
        //         sd_status_text = "No Card";
        //         break;
        //     case SD_NOT_INITIALIZED:
        //         sd_status_color = ST77XX_ORANGE; // Orange for not yet initialized or failed init
        //         sd_status_text = "No Init";
        //         break;
        //     default: // Should not happen
        //         sd_status_color = ST77XX_MAGENTA;
        //         sd_status_text = "ERR N/A";
        // }
        // tft.setTextColor(sd_status_color);
        // tft.print(sd_status_text);
        // y_cursor += line_height + 5;

        // // Add more display elements here if needed (e.g., GPS fix, BLE status, battery)
        // // Example: GPS Status (placeholder)
        // // tft.setCursor(x_indent, y_cursor);
        // // tft.setTextColor(ST77XX_WHITE);
        // // tft.print("GPS: No Fix"); // Replace with actual GPS status from g_gpsData
        // // y_cursor += line_height + 5;

        // // Example: BLE Power (placeholder)
        // // tft.setCursor(x_indent, y_cursor);
        // // tft.setTextColor(ST77XX_WHITE);
        // // tft.print("PWR: ---W"); // Replace with actual power from g_powerMetricsData
        // // y_cursor += line_height + 5;
    }
}
