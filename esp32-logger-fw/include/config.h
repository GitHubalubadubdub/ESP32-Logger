#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "types.h" // For PowerCadenceData
#include <FreeRTOS.h>
#include <semphr.h> // For SemaphoreHandle_t

// Pin Definitions
// GPS (using Serial2, typically UART2 on ESP32-S3)
#define GPS_RX_PIN GPIO_NUM_38 // ESP32 RX from GPS TX (User confirmed for Adafruit GPS Featherwing)
#define GPS_TX_PIN GPIO_NUM_39 // ESP32 TX to GPS RX (User confirmed for Adafruit GPS Featherwing)

// IMU (I2C0 - Wire)
#define IMU_SDA_PIN GPIO_NUM_8
#define IMU_SCL_PIN GPIO_NUM_9

// SD Card (SPI3/HSPI)
#define SD_MOSI_PIN GPIO_NUM_35
#define SD_MISO_PIN GPIO_NUM_37
#define SD_SCK_PIN  GPIO_NUM_36
#define SD_CS_PIN   GPIO_NUM_34

// Buttons (ESP32-S3 Reverse TFT Feather)
#define BUTTON_A_PIN GPIO_NUM_0  // BOOT/D0 button
#define BUTTON_B_PIN GPIO_NUM_1 // USER/D1 button
#define BUTTON_C_PIN GPIO_NUM_2 // USER/D2 button

// TFT Display (ESP32-S3 Reverse TFT Feather)
#define TFT_CS   GPIO_NUM_6  // Chip Select
#define TFT_DC   GPIO_NUM_7  // Data/Command
#define TFT_RST  GPIO_NUM_10 // Reset
// SPI for TFT: SCK=GPIO_NUM_12, MOSI=GPIO_NUM_11 (usually default SPI pins for the board)
// Backlight (BLK/LITE) is often GPIO_NUM_38, but it's used by GPS_RX_PIN.
// Assuming backlight is either always on or controlled by a different mechanism/default.

// Data Acquisition
#define DATA_ACQUISITION_INTERVAL_MS 5 // 200Hz

// LogRecordV1 Structure (defined in types.h)

// PSRAM Buffer Configuration
#define PSRAM_BUFFER_SIZE_RECORDS 2000 // Number of LogRecordV1 entries (e.g., 10 seconds at 200Hz)

// Shared data structure for power and cadence
extern PowerCadenceData g_powerCadenceData;
extern SemaphoreHandle_t g_dataMutex;

// System States (Example)
enum SystemState {
    STATE_INITIALIZING,
    STATE_WAITING_GPS_FIX,
    STATE_LOGGING,
    STATE_SD_CARD_ERROR,
    STATE_WIFI_MODE,
    STATE_LOW_BATTERY // Future
};

extern SystemState currentSystemState;

#endif // CONFIG_H
