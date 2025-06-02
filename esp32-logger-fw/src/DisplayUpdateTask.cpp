#include "DisplayUpdateTask.h"
#include "config.h"
#include "BleManagerTask.h" // To get BLE data AND device list
#include "DataBuffer.h"     // For LogRecordV1, if displaying buffer status
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Button helper functions (defined in main.cpp)
extern bool isButtonAPressed(); 
extern bool isButtonBPressed();

// TFT Pin definitions and tft object (already defined)
#define TFT_CS        39 
#define TFT_DC        40 
#define TFT_RST       41 
// MOSI: 35, SCK: 36
#define TFT_BL        42
// extern Adafruit_ST7789 tft; // Defined globally in this file in previous step
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Re-affirming definition here as per original structure

// Global system state variable (defined in main.cpp)
extern SystemState currentSystemState; 
extern DataBuffer<LogRecordV1> psramDataBuffer;


// --- Variables for button handling (simple debounce/rate limiting) ---
static uint32_t lastButtonATime = 0;
static uint32_t lastButtonBTime = 0;
const uint32_t buttonDebounceDelay = 250; // ms

// --- Forward declarations ---
void handleButtonInputs();
bool initializeDisplay(); // Already in .h but good practice for .cpp internal structure
void updateDisplay();     // Already in .h

// --- Main Display Task ---
void displayUpdateTask(void *pvParameters) {
    Serial.println("Display Update Task started");

    if (!initializeDisplay()) {
        Serial.println("Display Initialization Failed!");
        vTaskDelete(NULL);
        return;
    }
    // currentSystemState is initialized in main.cpp
    // BleManagerTask will set initial status like "Scanning..."

    for (;;) {
        handleButtonInputs(); // Process button presses
        updateDisplay();      // Redraw display contents
        vTaskDelay(pdMS_TO_TICKS(100)); // Update display and check buttons at ~10Hz
    }
}

// --- Display Initialization (largely same) ---
bool initializeDisplay() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(135, 240);
    tft.setRotation(3); 
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(5, 10);
    tft.println("Initializing..."); // This will be quickly overwritten by updateDisplay
    Serial.println("Display Initialized.");
    return true;
}

// --- Button Input Handling ---
void handleButtonInputs() {
    String currentBLEStatus = getBLEStatus(); // Cache status

    if (isButtonAPressed() && (millis() - lastButtonATime > buttonDebounceDelay)) {
        lastButtonATime = millis();
        Serial.println("Button A Pressed");
        if (currentBLEStatus == "Select Device") {
            int count = getDiscoveredDeviceCount();
            if (count > 0) {
                int currentIndex = getSelectedDeviceIndex();
                setSelectedDeviceIndex((currentIndex + 1)); // setSelectedDeviceIndex handles wrap-around
            }
        } else if (currentBLEStatus == "No Devices Found" || currentBLEStatus == "Disconnected" || currentBLEStatus == "Connection Failed" || currentBLEStatus == "Connect Failed") {
            // Button A to rescan
            startBleScan();
        }
        // Add other actions for Button A in different states if needed
    }

    if (isButtonBPressed() && (millis() - lastButtonBTime > buttonDebounceDelay)) {
        lastButtonBTime = millis();
        Serial.println("Button B Pressed");
        if (currentBLEStatus == "Select Device") {
            if (getDiscoveredDeviceCount() > 0) { // Only connect if there are devices
                 connectToSelectedDevice(); 
            }
        }
        // Add other actions for Button B in different states if needed
    }
}

// --- Display Update Function ---
void updateDisplay() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    
    String currentBLEStatus = getBLEStatus(); // Cache status for consistent display

    if (currentBLEStatus == "Select Device") {
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_CYAN);
        tft.println("Select Device:"); // Approx 16px height
        tft.setTextSize(1); // Smaller text for list items (approx 8px height per line)
        tft.setTextColor(ST77XX_WHITE);

        int count = getDiscoveredDeviceCount();
        int selectedIdx = getSelectedDeviceIndex();
        const int itemsPerPage = 7; // Max devices to show (135px height - 16px for title) / ~15px per item ~ 7-8 items.
                                    // Header (16px) + Footer (10px) = 26px. Remaining 109px. 109/15 = ~7 items.

        if (count == 0) {
            tft.setCursor(5, 30);
            tft.println("No devices found.");
            tft.setCursor(5, 45);
            tft.println("Press A to rescan.");
        } else {
            for (int i = 0; i < count; ++i) { 
                 if (i >= itemsPerPage) break; 
                NimBLEAdvertisedDevice* device = getDiscoveredDevice(i);
                if (device) {
                    tft.setCursor(5, 20 + (i * 12) ); // Y spacing for text size 1
                    if (i == selectedIdx) {
                        tft.setTextColor(ST77XX_YELLOW); 
                        tft.print("> ");
                    } else {
                        tft.setTextColor(ST77XX_WHITE);
                        tft.print("  ");
                    }
                    String name = device->getName().c_str();
                    if (name.length() == 0) name = device->getAddress().toString().c_str();
                    tft.println(name.substring(0, 18)); 
                }
            }
             tft.setCursor(5, tft.height() - 12); // Position for footer instructions
             tft.setTextSize(1);
             tft.setTextColor(ST77XX_LIGHTGREY);
             tft.print("A:Next, B:Connect");
        }
    } else if (currentBLEStatus == "Connected") {
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_GREEN);
        tft.print("Connected: "); 
        // Optionally display connected device name here if accessible
        // NimBLEAdvertisedDevice* connectedDev = getConnectedDevice(); // Would need such a function
        // if(connectedDev) tft.println(connectedDev->getName().substring(0,10)); else tft.println();
        tft.println(); // Newline after "Connected: "

        uint16_t power = getPower();
        tft.setTextColor(ST77XX_YELLOW);
        tft.print("Pwr: "); tft.print(power); tft.println(" W");

        uint8_t cadence = getCadence();
        tft.setTextColor(ST77XX_GREEN); // Re-set color if changed
        tft.print("Cad: "); tft.print(cadence); tft.println(" RPM");

    } else { // Other statuses: Initializing, Scanning, Connecting, Disconnected, No Devices Found, etc.
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(currentBLEStatus); // Display the status string directly

        if (currentBLEStatus == "No Devices Found" || currentBLEStatus == "Disconnected" || currentBLEStatus == "Connection Failed" || currentBLEStatus == "Connect Failed") {
            tft.setTextSize(1);
            tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(5, 30);
            tft.println("Press A to rescan.");
        }
    }
}
