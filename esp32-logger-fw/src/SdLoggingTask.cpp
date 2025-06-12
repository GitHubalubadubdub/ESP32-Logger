#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "SPI.h"
#include "SdFat.h"
#include "RTClib.h"

#include "LogDataStructure.h" // From src directory
#include "config.h"           // For SD_CS_PIN etc.
#include "shared_state.h"     // For is_recording, g_sdCardStatus, SdCardStatus_t new error enums
#include "freertos/semphr.h"  // For SemaphoreHandle_t and mutex functions

// HSPI Mutex (defined in main.cpp)
extern SemaphoreHandle_t g_hspiMutex;

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
    bool hspi_mutex_acquired = false;
    Serial.println("SdLoggingTask: initializeSdCard() attempting to take HSPI mutex...");
    if (g_hspiMutex != NULL && xSemaphoreTake(g_hspiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) { // Longer timeout for init
        hspi_mutex_acquired = true;
        Serial.println("SdLoggingTask: initializeSdCard() HSPI mutex taken.");
    } else {
        Serial.println("SdLoggingTask: initializeSdCard() timeout or invalid HSPI mutex! Cannot init SD card.");
        g_sdCardStatus = SD_ERROR_INIT_MUTEX; // Define this new status
        return false;
    }

    Serial.println("Attempting SD Card initialization (inside mutex)...");

    // Using HSPI for SD card, as established.
    SPIClass spiSD(HSPI);
    Serial.println("Using HSPI/SPI3 for SD card.");
    Serial.print("Calling spiSD.begin(SCK="); Serial.print(SD_SCK_PIN);
    Serial.print(", MISO="); Serial.print(SD_MISO_PIN);
    Serial.print(", MOSI="); Serial.print(SD_MOSI_PIN);
    Serial.println(")...");
    spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);

    Serial.print("Attempting sd.begin() with SdSpiConfig: CS="); Serial.print(SD_CS_PIN);
    Serial.println(", Speed=10MHz, SPI_OBJECT=&spiSD");

    if (!sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(10), &spiSD))) {
        Serial.println("sd.begin() failed.");
        if (sd.card()) {
            Serial.print("Card error code: 0x"); Serial.println(sd.card()->errorCode(), HEX);
            Serial.print("Card error data: 0x"); Serial.println(sd.card()->errorData(), HEX);
        } else {
            Serial.println("No card object available for error details.");
        }
        g_sdCardStatus = SD_ERROR_INIT;
        if (hspi_mutex_acquired) {
            xSemaphoreGive(g_hspiMutex);
            Serial.println("SdLoggingTask: initializeSdCard() HSPI mutex given on error path.");
        }
        return false;
    }

    Serial.println("SD Card initialized successfully.");
    g_sdCardStatus = SD_OK;
    if (hspi_mutex_acquired) {
        xSemaphoreGive(g_hspiMutex);
        Serial.println("SdLoggingTask: initializeSdCard() HSPI mutex given on success.");
    }
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
    bool hspi_mutex_acquired = false;
    Serial.println("SdLoggingTask: flushBufferToSd() attempting to take HSPI mutex...");
    if (g_hspiMutex != NULL && xSemaphoreTake(g_hspiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        hspi_mutex_acquired = true;
        Serial.println("SdLoggingTask: flushBufferToSd() HSPI mutex taken.");
    } else {
        Serial.println("SdLoggingTask: flushBufferToSd() timeout or invalid HSPI mutex! Data not flushed.");
        g_sdCardStatus = SD_ERROR_WRITE_MUTEX; // New status
        // Not returning, as the function is void. Error status is set.
    }

    if (hspi_mutex_acquired) {
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
            // logFile.sync(); // Consider if sync is needed here and its performance impact
        }
        xSemaphoreGive(g_hspiMutex);
        Serial.println("SdLoggingTask: flushBufferToSd() HSPI mutex given.");
    }
}

void sdLoggingTask(void *pvParameters) {
    bool sdInitialized = false;
    bool rtcInitialized = false;
    bool fileIsOpen = false;
    char filename[40]; // Buffer for filename e.g., /log_YYYYMMDD_HHMMSS.bin (adjust size if needed)
    bool hspi_mutex_acquired_for_file_op = false; // Flag for file open/close operations

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

                Serial.println("SdLoggingTask: Attempting to take HSPI mutex for file open...");
                if (g_hspiMutex != NULL && xSemaphoreTake(g_hspiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                    hspi_mutex_acquired_for_file_op = true;
                    Serial.println("SdLoggingTask: HSPI mutex taken for file open.");

                    if (!logFile.open(filename, FILE_WRITE)) {
                        Serial.print("SdLoggingTask: Failed to open new log file (inside mutex): "); Serial.println(filename);
                        g_sdCardStatus = SD_ERROR_OPEN;
                        fileIsOpen = false; // Ensure file is not considered open
                        // is_recording = false; // Stop recording might be too drastic here, task will retry
                    } else {
                        Serial.print("SdLoggingTask: Successfully opened log file (inside mutex): "); Serial.println(filename);
                        fileIsOpen = true;
                        g_sdCardStatus = SD_OK;
                        recordsInCurrentBatch = 0;
                    }
                    xSemaphoreGive(g_hspiMutex);
                    Serial.println("SdLoggingTask: HSPI mutex given after file open attempt.");
                    hspi_mutex_acquired_for_file_op = false; // Reset flag

                    if (!fileIsOpen) { // If file opening failed even with mutex
                         vTaskDelay(pdMS_TO_TICKS(1000)); // Wait before trying to create file again
                         continue;
                    }

                } else {
                    Serial.println("SdLoggingTask: Timeout or invalid HSPI mutex for file open! Skipping.");
                    g_sdCardStatus = SD_ERROR_OPEN_MUTEX; // New status
                    // is_recording = false; // Stop recording if cannot access bus for file open
                                          // Task will retry initialization or file opening.
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait before trying again
                    continue;
                }
                // Serial.print("SdLoggingTask: Successfully opened log file: "); Serial.println(filename); // Moved inside mutex block
                // fileIsOpen = true; // Moved inside mutex block
                // g_sdCardStatus = SD_OK; // File opened, SD status is OK for now // Moved inside mutex block
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
                flushBufferToSd(); // This will handle its own mutex for writing

                Serial.println("SdLoggingTask: Attempting to take HSPI mutex for file close...");
                if (g_hspiMutex != NULL && xSemaphoreTake(g_hspiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                    Serial.println("SdLoggingTask: HSPI mutex taken for file close.");
                    logFile.close();
                    xSemaphoreGive(g_hspiMutex);
                    Serial.println("SdLoggingTask: HSPI mutex given after file close. Log file closed.");
                    fileIsOpen = false; // Set only after successful close
                    g_sdCardStatus = SD_OK;
                } else {
                    Serial.println("SdLoggingTask: Timeout or invalid HSPI mutex for file close! File may not be closed properly.");
                    // g_sdCardStatus = SD_ERROR_CLOSE_MUTEX; // Optional new status
                    // fileIsOpen might still be true, attempt close again next cycle if still not recording.
                }
            }
            // If not recording and file is not open, just delay to prevent busy-waiting
            // This task will then wait for is_recording to become true.
            vTaskDelay(pdMS_TO_TICKS(500)); // Check is_recording flag twice a second
        }
    }
}
