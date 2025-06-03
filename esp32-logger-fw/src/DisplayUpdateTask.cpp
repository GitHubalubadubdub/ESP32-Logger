#include "DisplayUpdateTask.h"
#include "config.h"
// #include "BleManagerTask.h" // Removed
// #include "DataBuffer.h"     // Removed
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Button helper functions (defined in main.cpp) - REMOVED
// extern bool isButtonAPressed(); 
// extern bool isButtonBPressed();

// TFT Pin definitions are typically provided by the board variant's pins_arduino.h
// #define TFT_CS        39  // Example, ensure these match your board if not using variant pins
// #define TFT_DC        40 
// #define TFT_RST       41 
// MOSI: 35, SCK: 36 // These are often fixed by SPIClass
// #define TFT_BL        42 // Removed, using TFT_BACKLITE from board variant
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // TFT_CS, TFT_DC, TFT_RST should be defined in pins_arduino.h
GFXcanvas16 canvas(240, 135); // Added canvas

// Global system state variable (defined in main.cpp) - REMOVED
// extern SystemState currentSystemState; 
// extern DataBuffer<LogRecordV1> psramDataBuffer; // REMOVED


// --- Variables for button handling (simple debounce/rate limiting) --- REMOVED
// static uint32_t lastButtonATime = 0;
// static uint32_t lastButtonBTime = 0;
// const uint32_t buttonDebounceDelay = 250; // ms

// --- Forward declarations --- REMOVED
// void handleButtonInputs();
bool initializeDisplay(); // Already in .h but good practice for .cpp internal structure
// void updateDisplay();     // REMOVED

// --- Main Display Task ---
void displayUpdateTask(void *pvParameters) {
    Serial.println("Display Update Task started");

    if (!initializeDisplay()) {
        Serial.println("Display Initialization Failed!");
        vTaskDelete(NULL);
        vTaskDelete(NULL);
        return;
    }

    // Removed initial diagnostic sequence
    // tft.fillScreen(ST77XX_BLUE); 
    // Serial.println("Attempted to fill screen BLUE.");
    // vTaskDelay(pdMS_TO_TICKS(1000)); 
    // tft.invertDisplay(true);
    // Serial.println("Attempted to invert display.");
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(ST77XX_YELLOW); 
    // Serial.println("Attempted to fill screen YELLOW (after inversion).");
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.invertDisplay(false); 
    // Serial.println("Attempted to revert display inversion.");
    // vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        static int x_coord = 0;
        digitalWrite(TFT_BACKLITE, HIGH); // Continuously assert backlight

        // Draw a red square
        tft.fillRect(x_coord, 0, 10, 10, ST77XX_RED); 
        Serial.println("Drew red rectangle.");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Erase the square by drawing a black one over it
        tft.fillRect(x_coord, 0, 10, 10, ST77XX_BLACK); 
        Serial.println("Drew black rectangle (erase).");
        // No delay here, let the next loop iteration's delay handle overall timing

        x_coord = (x_coord + 10);
        if (x_coord >= tft.width()) { // Use tft.width() to get current screen width
            x_coord = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Overall loop delay
    }
}

// --- Display Initialization ---
bool initializeDisplay() {
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);
    tft.init(135, 240); // Initialize ST7789 with 135x240 for landscape after rotation
    tft.setRotation(3); 
    // Remove direct tft.fillScreen, tft.setTextColor, tft.setTextSize, tft.setCursor, tft.println
    // These will be done on the canvas.
    tft.fillScreen(ST77XX_BLACK); // Optionally, fill screen black once on init
    Serial.println("Display Initialized with canvas setup.");
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay after init commands
    return true;
}

// --- Button Input Handling --- REMOVED
// void handleButtonInputs() { ... }

// --- Display Update Function --- REMOVED
// void updateDisplay() { ... }
