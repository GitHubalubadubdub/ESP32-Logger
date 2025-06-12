#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "SPI.h"
#include "SdFat.h"
#include "RTClib.h"

#include "LogDataStructure.h" // From src directory
#include "config.h"           // For SD_CS_PIN etc.
#include "shared_state.h"     // For is_recording, g_sdCardStatus

// RTC object
RTC_PCF8523 rtc;
// SdFat object
SdFat sd;
// File object
FsFile logFile;

// Queue for receiving log data
extern QueueHandle_t xLoggingQueue; // Defined elsewhere, e.g. main.cpp

// Buffer for batch writing
const int RECORDS_TO_BUFFER_BEFORE_WRITE = 50; // Buffer 50 records before writing
LogRecordV1 recordBuffer[RECORDS_TO_BUFFER_BEFORE_WRITE];
int recordsInCurrentBatch = 0;

// Make sure these are defined in a .cpp file (e.g. main.cpp)
// volatile bool is_recording = false;
// volatile SdCardStatus_t g_sdCardStatus = SD_NOT_INITIALIZED;


// Function to initialize SD card
bool initializeSdCard() {
    Serial.println("Attempting SD Card initialization...");

    // Option 1: Let SdFat manage CS entirely with SdSpiConfig.
    // Initialize SPI bus without specifying CS pin here.
    Serial.println("Initializing SPI bus (SCK, MISO, MOSI)...");
    // SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN); // Pass -1 for CS if your SPI lib version needs it, or omit.
                                                       // For ESP32, just calling SPI.begin() without pins often uses default VSPI.
                                                       // Or, be explicit with pins for a specific SPI host.
                                                       // Let's assume we're using a specific SPI host (HSPI/SPI3 often used for SD on ESP32)
                                                       // and want to explicitly set the pins for that bus.
                                                       // The SdFat library will then use this initialized bus.

    // If your board has a dedicated SPIClass instance for the SD slot (e.g., hspi), use that.
    // Otherwise, use the default SPI.
    // For now, let's assume the global SPI object is to be used and pins are set for it.
    // If SPI.begin() is called without arguments it initializes VSPI.
    // If specific pins are used for HSPI (SPI3), it should be SPIClass spi = SPIClass(HSPI); spi.begin(...);
    // Given the pin numbers (35,36,37), these are non-default for VSPI on many ESP32s.
    // This implies a specific SPI host or need to reconfigure default SPI.
    // For now, let's try initializing the default SPI object with these specific pins.
    // The Adalogger FeatherWing typically wires the SD card to specific pins that might not be the default VSPI.
    // The ESP32 has multiple SPI controllers (SPI0/1 used for flash/psram, SPI2 (FSPI/VSPI), SPI3 (HSPI)).
    // Pins 35,36,37 are typically associated with SPI3 (HSPI).
    // Let's try to use HSPI.

    SPIClass spiSD(HSPI); // Create an SPI instance for HSPI
    Serial.println("Using HSPI/SPI3.");
    Serial.print("Calling spiSD.begin(SCK="); Serial.print(SD_SCK_PIN);
    Serial.print(", MISO="); Serial.print(SD_MISO_PIN);
    Serial.print(", MOSI="); Serial.print(SD_MOSI_PIN);
    Serial.println(")...");
    spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN); // Initialize HSPI with specific pins, CS managed by SdFat

    Serial.print("Attempting sd.begin() with SdSpiConfig: CS="); Serial.print(SD_CS_PIN);
    Serial.println(", Speed=10MHz, SPI_OBJECT=&spiSD");

    // Try a lower SPI speed, e.g., 10 MHz. Make sure to pass the correct SPI object.
    // SdSpiConfig(uint8_t csPin, uint8_t spiSettings, uint32_t maxSck, SPIClass* spi);
    // For ESP32, spiSettings is often DEDICATED_SPI or SHARED_SPI
    if (!sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(10), &spiSD))) {
        Serial.println("sd.begin() failed.");
        // Try to get more detailed error information if available
        if (sd.card()) {
            Serial.print("Card error code: 0x"); Serial.println(sd.card()->errorCode(), HEX);
            Serial.print("Card error data: 0x"); Serial.println(sd.card()->errorData(), HEX);
        } else {
            Serial.println("No card object available for error details.");
        }
        g_sdCardStatus = SD_ERROR_INIT; // Or a more specific error if known
        return false;
    }

    Serial.println("SD Card initialized successfully.");
    g_sdCardStatus = SD_OK;
    return true;
}

// Function to initialize RTC
bool initializeRtc() {
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC!");
        // Consider a specific RTC_STATUS if needed, or SD_ERROR_INIT implies system can't start logging
        return false;
    }
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, attempting to set the time to compile time.");
        // This line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // TODO: Implement a more robust time-setting mechanism if compile time is not sufficient
        // e.g., from GPS, NTP, or user input via BLE/Serial
    }
    Serial.println("RTC initialized.");
    return true;
}

void flushBufferToSd() {
    if (recordsInCurrentBatch > 0 && logFile.isOpen()) {
        size_t bytesToWrite = sizeof(LogRecordV1) * recordsInCurrentBatch;
        if (logFile.write(recordBuffer, bytesToWrite) != bytesToWrite) {
            Serial.println("SD Write Error!");
            g_sdCardStatus = SD_ERROR_WRITE;
            // Consider actions: stop recording, try to close/reopen file, etc.
        } else {
            // Serial.printf("Wrote %d records (%d bytes) to SD.\n", recordsInCurrentBatch, bytesToWrite);
        }
        recordsInCurrentBatch = 0; // Reset batch count
        // Periodically sync the file to ensure data is written to card from cache
        // This has a performance cost, so use judiciously or at end of file.
        // logFile.sync();
    }
}

void sdLoggingTask(void *pvParameters) {
    bool sdInitialized = false;
    bool rtcInitialized = false;
    bool fileIsOpen = false;
    char filename[40]; // Buffer for filename e.g., /log_YYYYMMDD_HHMMSS.bin (adjust size if needed)

    // Initial status
    g_sdCardStatus = SD_NOT_INITIALIZED;

    // Initialization attempts - these run once at task startup
    // If they fail, the task will loop but not attempt to record until conditions are met
    sdInitialized = initializeSdCard();
    if (sdInitialized) {
        rtcInitialized = initializeRtc(); // Only try RTC if SD is there (or vice-versa depending on logic)

        // Print SD card details if successfully initialized
        Serial.println("--- SD Card Details ---");
        // sectorCount returns uint32_t, ensure calculations promote to float for division
        float cardSizeMB = (float)sd.card()->sectorCount() * 512.0f / (1024.0f * 1024.0f);
        Serial.print("SD Card Size: "); Serial.print(cardSizeMB, 2); Serial.println(" MB");

        Serial.print("Card type: ");
        switch (sd.card()->type()) {
            case SD_CARD_TYPE_SD1: Serial.println("SD1"); break;
            case SD_CARD_TYPE_SD2: Serial.println("SD2"); break;
            case SD_CARD_TYPE_SDHC: Serial.println("SDHC/SDXC"); break;
            default: Serial.println("Unknown");
        }

        Serial.print("Volume format: ");
        uint8_t volType = sd.vol()->fatType(); // For SdFat v2
        if (volType == FAT_TYPE_EXFAT) Serial.println("exFAT");
        else if (volType == FAT_TYPE_FAT32) Serial.println("FAT32");
        else if (volType == FAT_TYPE_FAT16) Serial.println("FAT16");
        else if (volType == 0) Serial.println("Not a FAT volume or error determining type.");
        else { Serial.print("Unknown FAT type code: "); Serial.println(volType); }
        Serial.println("--- End SD Card Details ---");
    }


    while (1) {
        if (is_recording) {
            // --- Try to initialize hardware if not already done ---
            if (!sdInitialized) {
                Serial.println("SD not initialized, attempting init...");
                sdInitialized = initializeSdCard();
                if (!sdInitialized) {
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait before retrying
                    continue; // Skip to next loop iteration
                }
            }
            if (!rtcInitialized) {
                 Serial.println("RTC not initialized, attempting init...");
                 rtcInitialized = initializeRtc();
                 if (!rtcInitialized) {
                    // Decide if RTC is critical for logging. If so, stop/pause.
                    // For now, we'll let it proceed without RTC if SD is up, but filename will be an issue.
                    // Or, better: prevent file opening without RTC.
                    Serial.println("RTC failed to initialize. Cannot create timestamped filename.");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                 }
            }

            // --- File Operations ---
            if (!fileIsOpen && sdInitialized && rtcInitialized) { // Only open file if hardware is ready
                DateTime now = rtc.now();
                // Format: /LOG_YYYYMMDD_HHMMSS.BIN - ensuring directory for root path
                sprintf(filename, "/LOG_%04d%02d%02d_%02d%02d%02d.BIN",
                        now.year(), now.month(), now.day(),
                        now.hour(), now.minute(), now.second());

                Serial.print("SdLoggingTask: Attempting to open log file: '");
                Serial.print(filename);
                Serial.println("'");

                if (!logFile.open(filename, FILE_WRITE)) {
                    Serial.print("SdLoggingTask: Failed to open new log file: "); Serial.println(filename);
                    g_sdCardStatus = SD_ERROR_OPEN;
                    // Critical error: stop recording to prevent data loss or queue overflow
                    // is_recording = false; // This should be managed by a higher-level task or user input
                    // For now, just prevent further writes in this session.
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait before trying to create file again if still recording
                    continue;
                }
                Serial.print("SdLoggingTask: Successfully opened log file: "); Serial.println(filename);
                fileIsOpen = true;
                g_sdCardStatus = SD_OK; // File opened, SD status is OK for now
                recordsInCurrentBatch = 0; // Reset batch counter for the new file
            }

            // --- Data Logging ---
            if (fileIsOpen) { // Only proceed if file is successfully open
                LogRecordV1 receivedRecord;
                // Wait for data in the queue. Timeout allows periodic checks and buffer flushing.
                // A shorter timeout (e.g., 100ms) makes flushing more frequent if data rate is low.
                // A longer timeout reduces CPU usage but delays flushing if queue is not full.
                if (xQueueReceive(xLoggingQueue, &receivedRecord, pdMS_TO_TICKS(200)) == pdPASS) {
                    recordBuffer[recordsInCurrentBatch++] = receivedRecord;
                    if (recordsInCurrentBatch >= RECORDS_TO_BUFFER_BEFORE_WRITE) {
                        flushBufferToSd();
                        // Check g_sdCardStatus after flush, if SD_ERROR_WRITE, may need to stop.
                        if (g_sdCardStatus == SD_ERROR_WRITE) {
                            Serial.println("SD Write Error detected after flush. Stopping recording.");
                            // is_recording = false; // Inform other tasks recording has stopped.
                            // This should be handled by a central state manager.
                            // For now, close the file to prevent further errors.
                            logFile.close();
                            fileIsOpen = false;
                        }
                    }
                } else {
                    // Queue was empty for the timeout period.
                    // If still recording and file is open, flush any pending data.
                    // This ensures data is written if the queue doesn't fill up quickly
                    // or before recording stops.
                    if (is_recording && fileIsOpen) {
                        flushBufferToSd();
                    }
                }
            }
        } else { // Not recording
            if (fileIsOpen) {
                Serial.println("Recording stopped by external flag. Finalizing log file.");
                flushBufferToSd(); // Write any remaining data
                logFile.close();
                fileIsOpen = false;
                Serial.println("Log file closed.");
                g_sdCardStatus = SD_OK; // Reset status to OK after successful close
            }
            // If not recording and file is not open, just delay to prevent busy-waiting
            // This task will then wait for is_recording to become true.
            vTaskDelay(pdMS_TO_TICKS(500)); // Check is_recording flag twice a second
        }
    }
}
