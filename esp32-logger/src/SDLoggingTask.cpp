#include "SDLoggingTask.h"
#include "PSRAMBuffer.h" // To read data from buffer
#include "FS.h"      // Part of ESP32 Core for SD card
#include "SD.h"      // SD Card library
#include <SPI.h>     // For SD card communication

// SD Card Configuration
#define SD_CS_PIN    34 // Chip Select for SD Card
// HSPI/SPI3: MOSI: GPIO35, MISO: GPIO37, SCK: GPIO36
#define SPI_MOSI_PIN 35
#define SPI_MISO_PIN 37
#define SPI_SCK_PIN  36

SPIClass spiSD(HSPI); // Use HSPI (SPI3)

File logFile;
bool sd_card_initialized = false;
volatile bool logging_active = false;

bool initializeSDCard() {
    Serial.println("Initializing SD Card...");
    spiSD.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, spiSD)) {
        Serial.println("SD Card Mount Failed!");
        sd_card_initialized = false;
        return false;
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        sd_card_initialized = false;
        return false;
    }
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) Serial.println("MMC");
    else if (cardType == CARD_SD) Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB
", cardSize);
    
    // Create a new file for logging - naming convention TBD
    // For now, use "datalog.bin"
    // File operations should check if file exists, handle naming etc.
    logFile = SD.open("/datalog.bin", FILE_WRITE);
    if(!logFile){
        Serial.println("Failed to open datalog.bin for writing");
        sd_card_initialized = false;
        return false;
    }
    Serial.println("SD Card initialized and log file opened.");
    sd_card_initialized = true;
    logging_active = true; // Start logging once initialized
    return true;
}

void sdLoggingTask(void *pvParameters) {
    Serial.println("SD Logging Task started");
    LogRecordV1 data_buffer[10]; // Temporary local buffer to hold records from PSRAM
    size_t records_read;

    while (true) {
        if (sd_card_initialized && logging_active) {
            records_read = readFromPSRAMBuffer(data_buffer, 10); // Read up to 10 records
            
            if (records_read > 0) {
                if (logFile) {
                    size_t written = logFile.write((const uint8_t*)data_buffer, records_read * sizeof(LogRecordV1));
                    if (written != records_read * sizeof(LogRecordV1)) {
                        Serial.println("SD Card write error!");
                        // Handle error, maybe try to re-open file or stop logging
                    }
                    // logFile.flush(); // Optional: flush periodically or on specific triggers
                } else {
                    Serial.println("Log file not open!");
                    // Try to reopen or handle error
                }
            } else {
                // No data in PSRAM buffer, wait a bit
                vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50ms if buffer is empty
            }
        } else {
            // SD card not ready or logging paused
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
