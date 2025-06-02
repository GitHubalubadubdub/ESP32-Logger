# ESP32-S3 Data Logger Firmware

This project contains the firmware for a high-frequency data logger based on the Adafruit ESP32-S3 Reverse TFT Feather.

## Hardware

*   **MCU Board**: Adafruit ESP32-S3 Reverse TFT Feather (adafruit_feather_esp32s3_tft)
*   **GPS Module**: Generic NMEA GPS (UART1: TX GPIO18, RX GPIO17)
*   **Power Meter**: Favero Assioma MX-2 (BLE)
*   **IMU**: Generic 6-axis I2C (SDA GPIO8, SCL GPIO9)
*   **Storage**: MicroSD card (SPI: MOSI GPIO35, MISO GPIO37, SCK GPIO36, CS GPIO34)
*   **Display**: Built-in TFT

## Recommended Libraries (platformio.ini)

The following libraries are recommended for this project and are included in the `platformio.ini` `lib_deps`:

*   **TFT Display**:
    *   `adafruit/Adafruit GFX Library`: Core graphics library.
    *   `adafruit/Adafruit ST7735 and ST7789 Library`: Driver for the ST7789 display controller used on the ESP32-S3 Reverse TFT Feather.
*   **IMU (example for MPU6050)**:
    *   `adafruit/Adafruit MPU6050`: Library for the MPU6050 IMU. If a different IMU is used (e.g., LSM6DS3TR-C), the corresponding Adafruit or vendor library should be used.
    *   `adafruit/Adafruit Unified Sensor`: Required by some Adafruit sensor libraries.
*   **GPS**:
    *   `mikalhart/TinyGPSPlus`: A popular and efficient NMEA parsing library for GPS modules.
*   **BLE Client (Power Meter & Future Sensors)**:
    *   `h2zero/NimBLE-Arduino`: A memory-efficient BLE stack for ESP32, suitable for client operations.
*   **SD Card**:
    *   `SPI`: Arduino built-in SPI library.
    *   `SD`: Arduino built-in SD card library. (Note: For ESP32, this often uses the `FS.h` and `SD.h` from the ESP32 core).
*   **WiFi (for data offload)**:
    *   `WiFi`: ESP32 built-in WiFi library.
    *   `ESPAsyncWebServer`: For creating an HTTP server for file download (can be added later when implementing WiFi features).

## Project Structure

*   `platformio.ini`: PlatformIO project configuration file.
*   `src/`: Source code files.
    *   `main.cpp`: Main application entry point, setup, and loop.
    *   `DataStructures.h`: Defines the `LogRecordV1` and other shared data types.
    *   `PSRAMBuffer.h/.cpp`: Manages the data buffer in PSRAM.
    *   `DataAcquisitionTask.h/.cpp`: Handles the 200Hz data sampling.
    *   `SDLoggingTask.h/.cpp`: Manages writing data from PSRAM to the SD card.
    *   `DisplayTask.h/.cpp`: Controls the TFT display output.
    *   `SensorModules.h/.cpp`: Contains functions for initializing and reading data from various sensors (GPS, IMU, BLE).
    *   `BLEManager.h/.cpp` (Future): Will handle BLE connections and data.
    *   `WiFiServer.h/.cpp` (Future): Will manage WiFi connectivity and the HTTP server for data offload.
*   `lib/`: (Optional) For custom libraries specific to this project.
*   `data/`: (Optional) For files to be uploaded to SPIFFS or LittleFS (e.g., web server content).
*   `docs/`: (Optional) Project documentation.
