#include "DisplayUpdateTask.h"
#include "config.h" // For SystemState and other shared info

// TFT Library includes
// #include <Adafruit_GFX.h>
// #include <Adafruit_ST7789.h> // Or your specific driver
// #include <SPI.h>

// Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Hardware SPI
// Or specific constructor for ESP32-S3 Reverse TFT Feather if it has fixed pins

SystemState currentSystemState = STATE_INITIALIZING; // Global system state variable

void displayUpdateTask(void *pvParameters) {
    Serial.println("Display Update Task started");

    // if (!initializeDisplay()) {
    //     Serial.println("Display Initialization Failed!");
    //     // No display, but other tasks can continue
    // }

    for (;;) {
        // updateDisplay();
        vTaskDelay(pdMS_TO_TICKS(250)); // Update display at ~4Hz, adjust as needed
    }
}

bool initializeDisplay() {
    // tft.init(240, 240); // Initialize ST7789 screen (check dimensions for feather)
    // tft.setRotation(2); // Adjust rotation as needed
    // tft.fillScreen(ST77XX_BLACK);
    // tft.setTextColor(ST77XX_WHITE);
    // tft.setTextSize(2);
    // tft.setCursor(10, 10);
    // tft.println("Initializing...");
    // Serial.println("Display Initialized.");
    // return true;
    return false; // Placeholder
}

void updateDisplay() {
    // tft.fillScreen(ST77XX_BLACK);
    // tft.setCursor(0, 0);
    // tft.setTextSize(2);

    // switch (currentSystemState) {
    //     case STATE_INITIALIZING:
    //         tft.println("Initializing...");
    //         break;
    //     case STATE_WAITING_GPS_FIX:
    //         tft.println("Waiting GPS...");
    //         // Display GPS sats, fix type
    //         break;
    //     case STATE_LOGGING:
    //         tft.println("Logging...");
    //         // Display speed, power, duration, SD free
    //         break;
    //     case STATE_SD_CARD_ERROR:
    //         tft.setTextColor(ST77XX_RED);
    //         tft.println("SD Card Error!");
    //         tft.setTextColor(ST77XX_WHITE);
    //         break;
    //     case STATE_WIFI_MODE:
    //         tft.println("WiFi Mode");
    //         // Display IP address, file server status
    //         break;
    //     default:
    //         tft.println("Unknown State");
    // }

    // Example: Display a piece of data
    // tft.setTextSize(1);
    // tft.setCursor(0, 50);
    // tft.printf("Speed: %.2f m/s
", getLatestGpsData().speed_mps); // Assuming getLatestGpsData() exists
    // tft.printf("Power: %u W
", getLatestPowerData().power_watts);
}
