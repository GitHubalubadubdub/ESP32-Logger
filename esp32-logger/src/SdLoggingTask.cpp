#include "SdLoggingTask.h"
#include "config.h"
#include "DataBuffer.h" // To read from PSRAM buffer

// SD Card library includes
// #include <SPI.h>
// #include <SD.h>

extern DataBuffer<LogRecordV1> psramDataBuffer; // Assuming global buffer object
// File logFile;
// bool sdCardPresent = false;
// char currentLogFileName[30];

void sdLoggingTask(void *pvParameters) {
    Serial.println("SD Logging Task started");

    // if (initializeSDCard()) {
    //     sdCardPresent = true;
    //     createNewLogFile();
    // } else {
    //     currentSystemState = STATE_SD_CARD_ERROR;
    //     Serial.println("SD Card Initialization Failed!");
    // }

    LogRecordV1 recordToSave;

    for (;;) {
        // if (sdCardPresent && !psramDataBuffer.isEmpty()) {
        //     if (psramDataBuffer.read(recordToSave)) {
        //         if (logFile) {
        //             size_t bytesWritten = logFile.write((const uint8_t *)&recordToSave, sizeof(LogRecordV1));
        //             if (bytesWritten != sizeof(LogRecordV1)) {
        //                 Serial.println("SD Card write error!");
        //                 currentSystemState = STATE_SD_CARD_ERROR;
        //                 // Potentially try to re-initialize SD or stop logging
        //             }
        //         } else {
        //             Serial.println("Log file not open!");
        //             // Try to reopen or create new file
        //         }
        //     }
        // } else if (!sdCardPresent) {
        //     // Wait and retry SD card initialization periodically?
        //     vTaskDelay(pdMS_TO_TICKS(5000));
        //     // if (initializeSDCard()) { sdCardPresent = true; createNewLogFile(); }
        // } else {
        //     // Buffer is empty, wait a bit
        //     vTaskDelay(pdMS_TO_TICKS(100)); // Check buffer every 100ms
        // }
        vTaskDelay(pdMS_TO_TICKS(50)); // Prevent busy loop if nothing to do
    }
}

bool initializeSDCard() {
    // SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN); // Or use default SPI
    // if (!SD.begin(SD_CS_PIN)) {
    //     Serial.println("Card Mount Failed");
    //     return false;
    // }
    // uint8_t cardType = SD.cardType();
    // if (cardType == CARD_NONE) {
    //     Serial.println("No SD card attached");
    //     return false;
    // }
    // Serial.print("SD Card Type: ");
    // if (cardType == CARD_MMC) Serial.println("MMC");
    // else if (cardType == CARD_SD) Serial.println("SDSC");
    // else if (cardType == CARD_SDHC) Serial.println("SDHC");
    // else Serial.println("UNKNOWN");
    // uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    // Serial.printf("SD Card Size: %lluMB
", cardSize);
    // return true;
    return false; // Placeholder
}

void createNewLogFile() {
    // // Create a new file, e.g., LOG_YYYYMMDD_HHMMSS.bin
    // // This needs an RTC or NTP time, or use an incrementing number for now
    // int n = 0;
    // do {
    //     sprintf(currentLogFileName, "/log_%03d.bin", n++);
    // } while (SD.exists(currentLogFileName));

    // logFile = SD.open(currentLogFileName, FILE_WRITE);
    // if (!logFile) {
    //     Serial.print("Failed to open file for writing: ");
    //     Serial.println(currentLogFileName);
    //     currentSystemState = STATE_SD_CARD_ERROR;
    // } else {
    //     Serial.print("Opened log file: ");
    //     Serial.println(currentLogFileName);
    // }
}

void closeLogFile() {
    // if (logFile) {
    //     logFile.close();
    //     Serial.println("Log file closed.");
    // }
}
