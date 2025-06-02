#include "DisplayTask.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

// TFT display and CS pins for ESP32-S3 Reverse TFT Feather
#define TFT_CS        7
#define TFT_DC        39
#define TFT_RST       48 // Or -1 if not connected
#define TFT_MOSI      35 // Hardware SPI
#define TFT_SCLK      36 // Hardware SPI
// #define TFT_MISO      37 // Optional for read operations, not always needed for display

// Use hardware SPI
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
// If using software SPI:
// Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

bool display_initialized = false;

void initializeDisplay() {
    Serial.println("Initializing Display...");
    // SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS); // Initialize SPI if not done elsewhere
                                                    // Note: SD card SPI might conflict or need coordination
                                                    // For ESP32-S3 Reverse TFT, display SPI is usually shared (SPI3/HSPI)
                                                    // but managed by the library. Let's assume library handles SPI init.
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(10);
    digitalWrite(TFT_RST, HIGH);
    delay(10);

    tft.init(240, 240); // Initialize ST7789 screen
    tft.setRotation(2); // Adjust rotation as needed for ESP32-S3 Reverse TFT. 2 is common.
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 10);
    tft.println("Logger Booting...");
    Serial.println("Display Initialized.");
    display_initialized = true;
}

void updateDisplayInfo(const char* status, float gps_speed, int sats, float power, uint32_t log_duration_s, bool sd_ok) {
    if (!display_initialized) return;

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 10);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.printf("Status: %s
", status);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1); // Smaller text for data
    tft.printf("GPS Speed: %.2f m/s
", gps_speed);
    tft.printf("Sats: %d
", sats);
    tft.printf("Power: %.0f W
", power);
    tft.printf("Log Time: %lu s
", log_duration_s);
    tft.printf("SD Card: %s
", sd_ok ? "OK" : "Error");
    // Add more info as needed: SD free space, etc.
}


void displayTask(void *pvParameters) {
    Serial.println("Display Task started");
    uint32_t last_update = 0;
    const uint32_t update_interval_ms = 500; // Update display every 500ms

    // Example data - will be replaced by actual data from shared structures/queues
    char current_status[50] = "Initializing...";
    float current_gps_speed = 0.0f;
    int current_sats = 0;
    float current_power = 0.0f;
    uint32_t current_log_duration_s = 0;
    bool sd_status_ok = false;


    while (true) {
        if (millis() - last_update > update_interval_ms) {
            // In a real system, fetch this data from shared variables or queues
            // For now, just update with placeholder or static data
            // strcpy(current_status, "Logging"); // Example
            // sd_status_ok = sd_card_initialized; // Reflect SD card status

            updateDisplayInfo(current_status, current_gps_speed, current_sats, current_power, current_log_duration_s, sd_status_ok);
            last_update = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Check for update interval
    }
}
