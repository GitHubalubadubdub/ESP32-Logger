#include "DisplayUpdateTask.h"
#include "config.h"       // For SystemState (though less relevant for this specific update)
#include "BleManagerTask.h" // To get BLE data (getPower, getCadence, getBLEStatus)

// TFT Library includes
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> // Driver for the ST7789 display
#include <SPI.h>

// The ESP32-S3 Reverse TFT Feather has specific pins for the TFT.
// Adafruit_ST7789 constructor for this board usually handles these automatically.
// Default pins are typically: CS: D5, DC: D6, RST: D7, SCK: D40, MOSI: D35 ( Feather SPI )
// However, the Reverse TFT has SCK on IO36 and MOSI on IO35.
// The constructor Adafruit_ST7789(int8_t cs, int8_t dc, int8_t rst) is common.
// For ESP32-S3 Feather with ST7789:
// TFT_CS  (GPIO_NUM_7 for older feathers, check schematic for S3 Reverse)
// TFT_DC  (GPIO_NUM_39 for older feathers, check schematic for S3 Reverse)
// TFT_RST (GPIO_NUM_48 for older feathers, check schematic for S3 Reverse)
// The Adafruit ESP32-S3 Reverse TFT Feather specific pins are:
// CS:  GPIO39 (labelled D6 on silkscreen but this is TFT_CS)
// DC:  GPIO40 (labelled D5 on silkscreen but this is TFT_DC)
// RST: GPIO41 (labelled D4 on silkscreen but this is TFT_RST)
// MOSI: GPIO35
// SCK:  GPIO36
// BL:   GPIO42 (Backlight)

// For Adafruit ESP32-S3 Reverse TFT Feather, the constructor might be simpler if pins are fixed
// or use the generic constructor if pins need to be specified.
// Let's use the specific constructor if available, or a common one.
// The library typically uses TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST defines if set.
// Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Using the Arduino ESP32 S3 board definition pins for "TFT Feather Wing"
// which may align with the Reverse TFT Feather or require specific constructor.
// The Adafruit_GFX library for ESP32-S3 often uses these defines:
// TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST, TFT_BL
// For the Reverse TFT Feather, the pins are somewhat fixed.
// Let's use the constructor that doesn't require MOSI/SCLK if it uses hardware SPI by default.

// Pins for Adafruit ESP32-S3 Reverse TFT Feather:
#define TFT_CS        39 // D6 on silk, but is CS
#define TFT_DC        40 // D5 on silk, but is DC
#define TFT_RST       41 // D4 on silk, but is RST
#define TFT_BL        42 // Backlight pin, not defined in original example but needed
// MOSI: 35, SCK: 36 (Hardware SPI pins for HSPI/SPI3 on ESP32-S3)


Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Global system state variable (already defined in skeleton)
extern SystemState currentSystemState; 

void displayUpdateTask(void *pvParameters) {
    Serial.println("Display Update Task started");

    if (!initializeDisplay()) {
        Serial.println("Display Initialization Failed!");
        // No display, but other tasks can continue
        // Optionally, put the task to sleep indefinitely or terminate it
        vTaskDelete(NULL); // End this task if display is critical and failed
        return;
    }
    currentSystemState = STATE_LOGGING; // Example state

    for (;;) {
        updateDisplay();
        // Update display at a reasonable rate (e.g., 2-4Hz)
        // BLE data updates at 1Hz from power meter typically.
        vTaskDelay(pdMS_TO_TICKS(500)); // Update display every 500ms (2Hz)
    }
}

bool initializeDisplay() {
    pinMode(TFT_BL, OUTPUT); // Backlight pin
    digitalWrite(TFT_BL, HIGH); // Turn on backlight

    tft.init(135, 240); // Initialize ST7789 screen for the ESP32-S3 Reverse TFT (1.14" display)
                        // Common resolutions: 240x240, 240x135, 135x240.
                        // The Reverse TFT Feather is 135x240.
    tft.setRotation(3); // Adjust rotation as needed. 0&2=portrait, 1&3=landscape.
                        // For 135x240, rotation 3 might be landscape with USB port up/down.
                        // Rotation 0: USB right, Rotation 1: USB Up, Rotation 2: USB Left, Rotation 3: USB Down.
                        // Let's try rotation 3 for landscape.

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(5, 10); // Adjusted for smaller screen
    tft.println("Initializing...");
    Serial.println("Display Initialized.");
    return true; // Assuming success for now
}

void updateDisplay() {
    tft.fillScreen(ST77XX_BLACK); // Clear screen
    tft.setCursor(0, 0);        // Reset cursor to top-left
    tft.setTextSize(2);         // Default text size

    // 1. Display BLE Status
    String bleStatus = getBLEStatus();
    tft.setTextColor(ST77XX_CYAN); // Cyan for status
    tft.print("BLE: ");
    tft.println(bleStatus.substring(0, 12)); // Truncate if too long for one line

    // 2. Display Power
    uint16_t power = getPower();
    tft.setTextColor(ST77XX_YELLOW); // Yellow for power
    tft.print("Pwr: ");
    tft.print(power);
    tft.println(" W");

    // 3. Display Cadence
    uint8_t cadence = getCadence();
    tft.setTextColor(ST77XX_GREEN); // Green for cadence
    tft.print("Cad: ");
    tft.print(cadence);
    tft.println(" RPM");

    // 4. Display System State (example, can be expanded)
    tft.setCursor(0, tft.height() - 20); // Position at bottom
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("State: ");
    switch (currentSystemState) {
        case STATE_INITIALIZING:    tft.print("Init"); break;
        case STATE_WAITING_GPS_FIX: tft.print("GPS Wait"); break;
        case STATE_LOGGING:         tft.print("Logging"); break;
        case STATE_SD_CARD_ERROR:   tft.print("SD Err"); break;
        case STATE_WIFI_MODE:       tft.print("WiFi"); break;
        default:                    tft.print("Unknown"); break;
    }
    
    // Add more data as needed: GPS info, logging duration, SD card space etc.
    // Example: GPS Sats (dummy for now)
    // tft.setCursor(0, 80);
    // tft.setTextSize(2);
    // tft.setTextColor(ST77XX_MAGENTA);
    // tft.print("Sats: ");
    // tft.println("5 (dummy)");
}
