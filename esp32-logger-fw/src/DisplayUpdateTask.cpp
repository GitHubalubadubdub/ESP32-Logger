#include "DisplayUpdateTask.h"
#include "config.h"
// #include "BleManagerTask.h" // Removed
// #include "DataBuffer.h"     // Removed
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
//#include <SPI.h>
#include <Fonts/FreeSans12pt7b.h>
#include "Adafruit_MAX1704X.h"

Adafruit_MAX17048 lipo;

// Button helper functions (defined in main.cpp) - REMOVED
// extern bool isButtonAPressed(); 
// extern bool isButtonBPressed();

// TFT Pin definitions are typically provided by the board variant's pins_arduino.h
// #define TFT_CS        39  // Example, ensure these match your board if not using variant pins
// #define TFT_DC        40 
// #define TFT_RST       41 
// MOSI: 35, SCK: 36 // These are often fixed by SPIClass
// #define TFT_BL        42 // Removed, using TFT_BACKLITE from board variant

//SPIClass spi_display(HSPI); // Define SPIClass object for HSPI (Attempting HSPI instead of VSPI)

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
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



    canvas.fillScreen(ST77XX_BLACK);
    canvas.setCursor(0, 17); // Note: For FreeFonts, cursor Y is top. Font height for 12pt is >17. Consider increasing Y.
    canvas.setTextColor(ST77XX_RED);
    canvas.println("Adafruit Feather");
    canvas.setTextColor(ST77XX_YELLOW);
    canvas.println("ESP32-S3 TFT Demo");

    canvas.setTextColor(ST77XX_GREEN); 
    canvas.print("Battery: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(lipo.cellVoltage(), 1);
    canvas.print(" V  /  ");
    canvas.print(lipo.cellPercent(), 0);
    canvas.println("%");

    canvas.setTextColor(ST77XX_BLUE); 
    canvas.print("I2C: ");
    canvas.setTextColor(ST77XX_WHITE);

    canvas.setTextColor(ST77XX_MAGENTA); // Color for "Buttons: " label
    canvas.print("Buttons: ");
    canvas.setTextColor(ST77XX_WHITE); // Color for button states
    bool any_button_pressed = false;
    if (!digitalRead(0)) {
      canvas.print("D0 ");
      any_button_pressed = true;
      Serial.println("Button D0 pressed");
    }
    if (digitalRead(1)) {
      canvas.print("D1 ");
      any_button_pressed = true;
      Serial.println("Button D1 pressed");
    }
    if (digitalRead(2)) {
      canvas.print("D2 ");
      any_button_pressed = true;
      Serial.println("Button D2 pressed");
    }
    if (!any_button_pressed) {
        canvas.print("None");
    }
    canvas.println(""); // Newline after buttons

    display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);
    // pinMode(TFT_BACKLITE, OUTPUT); // Moved to setup
    digitalWrite(TFT_BACKLITE, HIGH); // Ensure backlight is on
    vTaskDelay(pdMS_TO_TICKS(10));
}

// --- Display Initialization ---
bool initializeDisplay() {
    display.init(135, 240);           // Init ST7789 240x135
    display.setRotation(3);
    pinMode(TFT_BACKLITE, OUTPUT);    // Configure backlight pin mode once
    digitalWrite(TFT_BACKLITE, HIGH); // Turn backlight on

    canvas.setFont(&FreeSans12pt7b);
    canvas.setTextColor(ST77XX_WHITE);

    pinMode(0, INPUT_PULLUP);
    pinMode(1, INPUT_PULLDOWN);
    pinMode(2, INPUT_PULLDOWN);

    Serial.println("Display Initialized with canvas setup.");
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay after init commands
    return true;
}

// --- Button Input Handling --- REMOVED
// void handleButtonInputs() { ... }

// --- Display Update Function --- REMOVED
// void updateDisplay() { ... }

 // <-- Added closing brace to fix 'expected '}' at end of input' error
