#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <SPI.h> // Ensure SPI.h is included for SPIClass

// Project-specific headers
#include "config.h"           // For BUTTON_A_PIN etc. (TFT pins from pins_arduino.h)
#include "shared_state.h"     // For is_recording, g_sdCardStatus, SdCardStatus_t
#include "LogDataStructure.h" // For LogRecordV1 struct (used for queue size)

// Task headers
#include "DataAcquisitionTask.h"
#include "SdLoggingTask.h"
#include "DisplayUpdateTask.h"
// #include "BleManagerTask.h" // If BLE task is also created here
// #include "WifiHandlerTask.h" // If WiFi task is also created here

// Graphics and Display
#include "Adafruit_GFX.h"      // Core graphics library
#include "Adafruit_ST7789.h"   // Hardware-specific library for ST7789
// config.h is included for other settings, TFT pins should come from pins_arduino.h

// --- Global Variable Definitions (declared extern in shared_state.h) ---
volatile bool is_recording = false;
volatile SdCardStatus_t g_sdCardStatus = SD_NOT_INITIALIZED; // Initial state
volatile DebugSettings g_debugSettings; // Definition for the global debug settings struct
SemaphoreHandle_t g_debugSettingsMutex; // Definition for its mutex

// --- Dedicated SPI for TFT ---
SPIClass spiTFT(HSPI); // Use HSPI for the TFT

// --- TFT Display Object ---
// TFT_CS, TFT_DC, TFT_RST are expected to be defined in the board variant's pins_arduino.h
// For Adafruit ESP32-S3 Reverse TFT, these are typically 42, 40, 41.
// Using constructor that accepts SPIClass*
Adafruit_ST7789 tft = Adafruit_ST7789(&spiTFT, TFT_CS, TFT_DC, TFT_RST);

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
    unsigned long setup_entry_time = millis(); // Moved to the very top
    unsigned long last_timestamp = setup_entry_time;

    Serial.begin(115200);
    Serial.println("Serial initialized at top of setup().");
    Serial.print("Time after Serial init: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
    last_timestamp = millis();

    unsigned long setup_start_time = millis(); // This is for the !Serial timeout, can be kept or removed if not strictly needed
    while (!Serial && (millis() - setup_start_time < 5000)); // Wait for serial, but timeout after 5s

    Serial.println("--- ESP32 Logger Starting Up ---");

    // Initialize global debug settings mutex
    // Serial.println("Before xSemaphoreCreateMutex for g_debugSettingsMutex.");
    // g_debugSettingsMutex = xSemaphoreCreateMutex();
    // Serial.println("After xSemaphoreCreateMutex for g_debugSettingsMutex.");
    // if (g_debugSettingsMutex == NULL) {
    //     Serial.println("CRITICAL ERROR: Failed to create debug settings mutex!");
    //     // Handle error: perhaps loop indefinitely or restart
    // }
    Serial.print("Time after (Commented Out) Mutex creation: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
    last_timestamp = millis(); // Ensure last_timestamp is updated for next measurement

    // Initialize Display
    Serial.println("--- TFT Initialization Diagnostics ---");

    Serial.println("Initializing dedicated SPI (spiTFT on HSPI) for TFT...");
    // Using known HSPI pins for ESP32-S3: SCK=36, MISO=37, MOSI=35.
    // CS for spiTFT.begin is -1 because TFT library handles its own CS pin.
    spiTFT.begin(36, 37, 35, -1); // SCK=36, MISO=37, MOSI=35, CS=-1 (software CS)
    Serial.println("Dedicated SPI for TFT initialized with direct GPIO numbers.");

    Serial.print("Expected TFT_CS Pin (from variant): "); Serial.println(TFT_CS);
    Serial.print("Expected TFT_DC Pin (from variant): "); Serial.println(TFT_DC);
    Serial.print("Expected TFT_RST Pin (from variant): "); Serial.println(TFT_RST);

    Serial.println("Before tft.init(135, 240)");
    tft.init(135, 240); // For ESP32-S3 Reverse TFT.
    Serial.println("After tft.init(135, 240)");

    #ifdef TFT_BACKLITE // Check if TFT_BACKLITE is defined by pins_arduino.h
        Serial.println("Attempting to enable TFT backlight using TFT_BACKLITE pin...");
        pinMode(TFT_BACKLITE, OUTPUT);
        digitalWrite(TFT_BACKLITE, HIGH);
        Serial.print("TFT_BACKLITE pin ("); Serial.print(TFT_BACKLITE); Serial.println(") set to HIGH.");
    #else
        Serial.println("Warning: TFT_BACKLITE macro not defined by board variant. Backlight might not enable.");
    #endif

    Serial.println("Before tft.setRotation(3)");
    tft.setRotation(3); // Landscape (or portrait depending on display mounting)
    Serial.println("After tft.setRotation(3)");

    Serial.println("Before tft.fillScreen(ST77XX_BLACK)");
    tft.fillScreen(ST77XX_BLACK);
    Serial.println("After tft.fillScreen(ST77XX_BLACK)");

    Serial.println("Before tft.setCursor/setTextSize/setTextColor for boot message");
    tft.setCursor(10, 10); // Adjusted from 5,5 and size 1 to 10,10 and size 2 for "Logger Booting..."
    tft.setTextSize(2);    // Consistent with previous full version's "TFT Initialized!"
    tft.setTextColor(ST77XX_WHITE);
    Serial.println("After tft.setCursor/setTextSize/setTextColor for boot message");

    Serial.println("Before tft.println(\"Logger Booting...\")");
    tft.println("Logger Booting..."); // Boot message to TFT
    Serial.println("After tft.println(\"Logger Booting...\")");
    Serial.println("--- End TFT Initialization Diagnostics ---");
    Serial.print("Time after TFT init block: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
    last_timestamp = millis();

    // Initialize Button A (D0) for recording toggle
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_A_PIN), button_a_isr_handler, FALLING);
    Serial.printf("Button A (GPIO%d) initialized for recording toggle.\n", BUTTON_A_PIN);
    Serial.print("Time after Button init: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
    last_timestamp = millis();

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
        Serial.print("Time after Queue creation: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
        last_timestamp = millis();
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
        Serial.print("Time after DataAcqTask creation: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
        last_timestamp = millis();
    }

    // SD Logging Task: Medium priority, can be on PRO_CPU as it involves file I/O. Larger stack for SdFat.
    if (xTaskCreatePinnedToCore(sdLoggingTask, "SdLogTask", 8192, NULL, 3, NULL, 0) != pdPASS) {
        Serial.println("ERROR: Failed to create SD Logging Task!");
    } else {
        Serial.println("SD Logging Task created (Core 0, Prio 3).");
        Serial.print("Time after SdLogTask creation: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
        last_timestamp = millis();
    }

    // Display Update Task: Lower priority, can be on PRO_CPU
    if (xTaskCreatePinnedToCore(displayUpdateTask, "DisplayTask", 8192, NULL, 2, NULL, 0) != pdPASS) { // Lower priority, increased stack
        Serial.println("ERROR: Failed to create Display Update Task!");
    } else {
        Serial.println("Display Update Task created (Core 0, Prio 2, Stack 8192).");
        Serial.print("Time after DisplayTask creation: "); Serial.print(millis() - last_timestamp); Serial.println("ms");
        last_timestamp = millis();
    }

    // Add other task creations here if needed (e.g., BleManagerTask, WifiHandlerTask)

    Serial.print("Total setup() time: "); Serial.print(millis() - setup_entry_time); Serial.println("ms");
    Serial.println("--- System setup complete. Tasks running. ---");
    // Message to TFT indicating tasks started, can be part of DisplayUpdateTask's initial draw.
    // tft.setCursor(10, tft.getCursorY() + tft.fontHeight(2) + 5); // Move cursor below "Logger Booting..."
    // tft.println("Tasks started!");
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
