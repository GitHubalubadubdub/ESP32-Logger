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

// TFT Display (Built-in, specific pins managed by library, but good to note)
// #define TFT_CS   PIN_D5 // Example, check board schematic
// #define TFT_DC   PIN_D6 // Example
// #define TFT_RST  PIN_D7 // Example

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
