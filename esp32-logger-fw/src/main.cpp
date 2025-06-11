#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// SPI.h is included by Adafruit_ST7789.h if needed, or by SdFat.h

// Project-specific headers
#include "config.h"           // For BUTTON_A_PIN, TFT_CS, TFT_DC, TFT_RST etc.
#include "shared_state.h"     // For is_recording, g_sdCardStatus, SdCardStatus_t
#include "LogDataStructure.h" // For LogRecordV1 struct (used for queue size)

// Task headers
#include "DataAcquisitionTask.h"
#include "SdLoggingTask.h"
#include "DisplayUpdateTask.h"
// #include "BleManagerTask.h" // If BLE task is also created here
// #include "WifiHandlerTask.h" // If WiFi task is also created here

// Graphics and Display
#include "Adafruit_GFX.h"
#include "Adafruit_ST7789.h"


// --- Global Variable Definitions (declared extern in shared_state.h) ---
volatile bool is_recording = false;
volatile SdCardStatus_t g_sdCardStatus = SD_NOT_INITIALIZED; // Initial state
volatile DebugSettings g_debugSettings; // Definition for the global debug settings struct
SemaphoreHandle_t g_debugSettingsMutex; // Definition for its mutex

// --- TFT Display Object ---
// Using pins from config.h (TFT_CS, TFT_DC, TFT_RST)
// SPI pins for TFT (SCK, MOSI) are assumed to be the default SPI pins for the ESP32-S3
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// --- Queue Definition ---
QueueHandle_t xLoggingQueue;
// Sizing the queue: e.g., 200Hz for 10 seconds of data before SD card must catch up
// DATA_ACQUISITION_INTERVAL_MS is 5ms (200Hz)
// For 10 seconds: 200 records/sec * 10 sec = 2000 records
const int LOGGING_QUEUE_LENGTH = PSRAM_BUFFER_SIZE_RECORDS; // Defined in config.h

// --- Button Debouncing ---
// BUTTON_A_PIN is used as the recording toggle button (defined in config.h as GPIO_NUM_0)
volatile bool button_a_flag = false; // Flag set by ISR
volatile unsigned long last_button_a_interrupt_time = 0;
const unsigned long button_debounce_duration = 250; // ms, increased to avoid quick multi-presses in ISR

// ISR for Button A (D0)
void IRAM_ATTR button_a_isr_handler() {
    unsigned long current_millis = millis();
    if ((current_millis - last_button_a_interrupt_time) > button_debounce_duration) {
        button_a_flag = true;
        last_button_a_interrupt_time = current_millis;
    }
}

void setup() {
    Serial.begin(115200);
    unsigned long setup_start_time = millis();
    while (!Serial && (millis() - setup_start_time < 5000)); // Wait for serial, but timeout after 5s

    Serial.println("--- ESP32 Logger Starting Up ---");

    // Initialize global debug settings mutex
    g_debugSettingsMutex = xSemaphoreCreateMutex();
    if (g_debugSettingsMutex == NULL) {
        Serial.println("CRITICAL ERROR: Failed to create debug settings mutex!");
        // Handle error: perhaps loop indefinitely or restart
    }

    // Initialize Display
    Serial.println("Initializing Display (ST7789)...");
    // The ST7789 library typically uses hardware SPI.
    // SPI.begin() might be called by SdFat library if SD card is initialized first,
    // or it can be called here if needed. For many Adafruit boards with integrated
    // displays or FeatherWings, the library handles SPI transactions without explicit global SPI.begin() sometimes.
    // Let's assume the library's init is sufficient for now.
    tft.init(135, 240); // Initialize ST7789 screen for ESP32-S3 Reverse TFT (135x240)
    Serial.println("Display initialized.");
    tft.setRotation(1); // Adjust rotation as needed (0-3). 1 or 3 for landscape on 135x240.
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1); // Smaller text for boot message
    tft.setCursor(5, 5);
    tft.println("Logger Booting...");
    tft.println("Initializing systems...");
    // tft.display(); // For some libraries, an explicit display() call is needed after drawing.
                     // Adafruit_ST7789 usually updates on print/draw.

    // Initialize Button A (D0) for recording toggle
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_A_PIN), button_a_isr_handler, FALLING);
    Serial.printf("Button A (GPIO%d) initialized for recording toggle.\n", BUTTON_A_PIN);

    // Initialize global recording state
    is_recording = false;
    g_sdCardStatus = SD_NOT_INITIALIZED; // SdLoggingTask will update this

    // Create Logging Queue
    xLoggingQueue = xQueueCreate(LOGGING_QUEUE_LENGTH, sizeof(LogRecordV1));
    if (xLoggingQueue == NULL) {
        Serial.println("CRITICAL ERROR: Failed to create logging queue!");
        // This is critical, system cannot function. Halt or reboot.
        while(1);
    } else {
        Serial.printf("Logging queue created. Size: %d records of %d bytes each.\n", LOGGING_QUEUE_LENGTH, sizeof(LogRecordV1));
    }

    // --- Create FreeRTOS tasks ---
    // Parameters: TaskFunction, TaskName, StackSize, Parameters, Priority, TaskHandle, CoreID
    // Core ID: 0 for PRO_CPU, 1 for APP_CPU. APP_CPU often handles WiFi/BLE. PRO_CPU for system tasks.
    // ESP32-S3 is dual-core.

    Serial.println("Creating tasks...");

    // Data Acquisition Task: High priority, potentially on APP_CPU if it's data intensive but not blocking on peripherals shared with PRO_CPU
    if (xTaskCreatePinnedToCore(dataAcquisitionTask, "DataAcqTask", 4096, NULL, 5, NULL, 1) != pdPASS) { // Higher priority
        Serial.println("ERROR: Failed to create Data Acquisition Task!");
    } else {
        Serial.println("Data Acquisition Task created (Core 1, Prio 5).");
    }

    // SD Logging Task: Medium priority, can be on PRO_CPU as it involves file I/O. Larger stack for SdFat.
    if (xTaskCreatePinnedToCore(sdLoggingTask, "SdLogTask", 8192, NULL, 3, NULL, 0) != pdPASS) {
        Serial.println("ERROR: Failed to create SD Logging Task!");
    } else {
        Serial.println("SD Logging Task created (Core 0, Prio 3).");
    }

    // Display Update Task: Lower priority, can be on PRO_CPU
    if (xTaskCreatePinnedToCore(displayUpdateTask, "DisplayTask", 4096, NULL, 2, NULL, 0) != pdPASS) { // Lower priority
        Serial.println("ERROR: Failed to create Display Update Task!");
    } else {
        Serial.println("Display Update Task created (Core 0, Prio 2).");
    }

    // Add other task creations here if needed (e.g., BleManagerTask, WifiHandlerTask)

    Serial.println("--- System setup complete. Tasks running. ---");
    tft.setCursor(5, tft.getCursorY() + 5); // Move cursor down
    tft.println("Tasks started!");
}

void loop() {
    // Handle button A press with debouncing in the main loop
    if (button_a_flag) {
        // Debounce logic is mostly in ISR via last_button_a_interrupt_time
        // This flag is just the trigger from ISR to loop.

        is_recording = !is_recording; // Toggle recording state

        if (is_recording) {
            Serial.println("MAIN: Recording STARTED");
            // Optional: Check SD card status before "confirming" start
            // This is tricky because SdLoggingTask initializes SD card.
            // DisplayUpdateTask should show the actual status from g_sdCardStatus.
            // If g_sdCardStatus shows an error *after* trying to start, is_recording might be set
            // back to false by SdLoggingTask or a managing task.
            if (g_sdCardStatus == SD_ERROR_INIT || g_sdCardStatus == SD_ERROR_OPEN || g_sdCardStatus == SD_NOT_PRESENT) {
                Serial.println("WARN: Attempting to start recording, but SD card has an error or not present.");
                // is_recording = false; // Force stop if SD is known bad? Could race with SdLoggingTask.
                                      // Better to let SdLoggingTask handle its state.
            }
        } else {
            Serial.println("MAIN: Recording STOPPED");
        }

        button_a_flag = false; // Reset flag after handling
    }

    // The main loop in a FreeRTOS application is often minimal.
    // It can be used for non-real-time tasks, checks, or be empty if all work is in tasks.
    // Example: Monitor stack usage, print heap info periodically for debugging.
    vTaskDelay(pdMS_TO_TICKS(50)); // Yield for a short period
}
