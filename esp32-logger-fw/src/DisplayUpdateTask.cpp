#include "DisplayUpdateTask.h"
#include "config.h" // Includes types.h (for BleConnectionState)
#include "gps_data.h" // For GpsData struct and g_gpsDataMutex

#include "Adafruit_MAX1704X.h"
#include <Adafruit_NeoPixel.h>
#include "Adafruit_TestBed.h"
#include <Adafruit_BME280.h>
#include <Adafruit_ST7789.h> 
#include <Fonts/FreeSans12pt7b.h>
#include <cstdio>  // For snprintf
#include <cstring> // For strcpy, strncpy


Adafruit_BME280 bme; // I2C
bool bmefound = false;
extern Adafruit_TestBed TB;

Adafruit_MAX17048 lipo;
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 canvas(240, 135);

static bool valid_i2c[128]; // File-scope static for access by both functions

// Display mode definitions
enum DisplayMode { DISPLAY_POWER, DISPLAY_GPS };
static DisplayMode currentDisplayMode = DISPLAY_POWER;
const int MODE_SWITCH_BUTTON_PIN = BUTTON_B_PIN; // Use BUTTON_B_PIN from config.h
static unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 50; // milliseconds for button debounce


bool initializeDisplay(); // Already in .h but good practice for .cpp internal structure

// static int j = 0; // Keep j static if it's used to alternate states or for initialization checks
// No longer needed for the new display logic

// --- Main Display Task ---
void displayUpdateTask(void *pvParameters) {
  Serial.println("Display Update Task started");

  if (!initializeDisplay()) { // initializeDisplay also handles backlight
      Serial.println("Display Initialization Failed!");
      vTaskDelete(NULL); // Delete task if display init fails
      return;
  }

  // Ensure backlight is on after initialization (initializeDisplay should handle this)
  // pinMode(TFT_BACKLITE, OUTPUT); // Should be in initializeDisplay
  // digitalWrite(TFT_BACKLITE, HIGH); // Should be in initializeDisplay

  uint16_t power = 0;
  uint8_t cadence = 0;
  BleConnectionState currentBleState = BLE_IDLE;
  char deviceName[50] = {0};
  float local_left_balance = 50.0f;
  bool local_balance_available = false;

  char statusString[100] = {0}; // Buffer for BLE status text
  char lrBalanceString[50] = {0}; // Buffer for L/R balance text
  char battString[30]; // Buffer for battery status

  GpsData localGpsData; // Local copy of GPS data


  for (;;) { // Infinite loop for the task
    // --- Button Logic for Mode Switching ---
    Serial.printf("Button D2 (GPIO%d) State: %d\n", MODE_SWITCH_BUTTON_PIN, digitalRead(MODE_SWITCH_BUTTON_PIN));

    // New more sensitive button detection logic
    bool buttonSignalDetected = false;
    // Try to detect a LOW signal over a short period by polling rapidly
    // Note: digitalRead() itself is fast. The loop is to catch quick transitions
    // that might occur between display task iterations if it were sleeping longer.
    for (int i = 0; i < 5; ++i) {
        if (digitalRead(MODE_SWITCH_BUTTON_PIN) == HIGH) {
            buttonSignalDetected = true;
            break;
        }
        // Consider adding a very short delay if absolutely necessary, like delayMicroseconds(100) or vTaskDelay(0) to yield,
        // but for now, let's try without to maximize sensitivity during this task's slice.
        // The main task delay vTaskDelay(pdMS_TO_TICKS(250)) at the end of the displayUpdateTask loop will prevent this from hogging CPU.
    }

    if (buttonSignalDetected) {
        if (millis() - lastButtonPressTime > debounceDelay) { // debounceDelay is now 50ms
            currentDisplayMode = (currentDisplayMode == DISPLAY_POWER) ? DISPLAY_GPS : DISPLAY_POWER;
            lastButtonPressTime = millis();
            Serial.print("Display Mode Switched to: ");
            Serial.println(currentDisplayMode == DISPLAY_POWER ? "POWER" : "GPS");
            canvas.fillScreen(ST77XX_BLACK); // Clear screen on mode change
        }
    }

    canvas.fillScreen(ST77XX_BLACK); // Clear canvas for current mode's content
    canvas.setFont(&FreeSans12pt7b);
    canvas.setTextWrap(false);
    // canvas.setCursor(10, 20); // Reset cursor for each mode's drawing - DO THIS INSIDE MODE BLOCK

    if (currentDisplayMode == DISPLAY_POWER) {
        canvas.setCursor(10, 20); // Set cursor for this mode
        // --- Read shared power data ---
        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            power = g_powerCadenceData.power;
            cadence = g_powerCadenceData.cadence;
            currentBleState = g_powerCadenceData.bleState;
            strncpy(deviceName, g_powerCadenceData.connectedDeviceName, sizeof(deviceName) - 1);
            deviceName[sizeof(deviceName) - 1] = '\0'; // Ensure null termination

            local_left_balance = g_powerCadenceData.left_pedal_balance_percent;
            local_balance_available = g_powerCadenceData.pedal_balance_available;
            xSemaphoreGive(g_dataMutex);
        } else {
            Serial.println("Display task (POWER): Failed to get g_dataMutex");
            strcpy(statusString, "Status: Mutex Error");
            strcpy(lrBalanceString, "L/R: Mutex Error");
            power = 0; cadence = 0; currentBleState = BLE_IDLE; // Reset data
        }

        // Prepare BLE status string
        switch (currentBleState) {
            case BLE_IDLE: strcpy(statusString, "BLE: Idle"); break;
            case BLE_SCANNING: strcpy(statusString, "BLE: Scanning..."); break;
            case BLE_CONNECTING: snprintf(statusString, sizeof(statusString), "BLE: Connecting %s", deviceName[0] == '\0' ? "" : deviceName); break;
            case BLE_CONNECTED: snprintf(statusString, sizeof(statusString), "BLE: %s", deviceName); break;
            case BLE_DISCONNECTED: strcpy(statusString, "BLE: Disconnected"); break;
            default: strcpy(statusString, "BLE: Unknown State"); break;
        }

        // Prepare L/R Balance display string
        if (local_balance_available) {
            float right_pedal_balance_percent = 100.0f - local_left_balance;
            if (right_pedal_balance_percent < 0.0f) right_pedal_balance_percent = 0.0f;
            snprintf(lrBalanceString, sizeof(lrBalanceString), "L/R: %.0f%% / %.0f%%", local_left_balance, right_pedal_balance_percent);
        } else {
            snprintf(lrBalanceString, sizeof(lrBalanceString), "L/R: --%% / --%%");
        }

        // --- Display Power Data ---
        // 1. Power
        canvas.setTextColor(ST77XX_GREEN);
        canvas.print("Power: ");
        canvas.setTextColor(ST77XX_WHITE);
        canvas.print(power);
        canvas.println(" W");

        // 2. Cadence
        canvas.setCursor(10, 45);
        canvas.setTextColor(ST77XX_GREEN);
        canvas.print("Cadence: ");
        canvas.setTextColor(ST77XX_WHITE);
        canvas.print(cadence);
        canvas.println(" RPM");

        // 3. L/R Balance
        canvas.setCursor(10, 70);
        canvas.setTextColor(ST77XX_WHITE);
        canvas.println(lrBalanceString);

        // 4. BLE Status
        canvas.setCursor(10, 95);
        canvas.setTextColor(ST77XX_CYAN);
        canvas.println(statusString);

        // Display Battery Info for Power Mode
        canvas.setCursor(10, 120);
        canvas.setTextColor(ST77XX_YELLOW);
        snprintf(battString, sizeof(battString), "Batt: %.1fV %.0f%%", lipo.cellVoltage(), lipo.cellPercent());
        canvas.println(battString);

    } else if (currentDisplayMode == DISPLAY_GPS) {
        canvas.setCursor(10, 20); // Set cursor for this mode
        // --- Read shared GPS data ---
        if (xSemaphoreTake(g_gpsDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localGpsData = g_gpsData; // Copy the global struct
            xSemaphoreGive(g_gpsDataMutex);
        } else {
            Serial.println("Display task (GPS): Failed to get g_gpsDataMutex");
            localGpsData.is_valid = false; // Mark as invalid if mutex fails
        }

        // --- Display GPS Data ---
        char gpsLineBuffer[50];

        if (!localGpsData.is_valid || (millis() - localGpsData.last_update_millis > 7000)) { // GPS Acquiring
            canvas.setTextColor(ST77XX_RED);
            canvas.println("GPS: Acquiring fix..."); // Line 1 (Y=20)

            canvas.setCursor(10, 45); // Line 2
            canvas.setTextColor(ST77XX_ORANGE);
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Sats: %u Fix: %u", localGpsData.satellites, localGpsData.fix_quality);
            canvas.println(gpsLineBuffer);

            // Display Battery Info for GPS Acquiring Mode
            canvas.setCursor(10, 70); // Line 3
            canvas.setTextColor(ST77XX_YELLOW);
            snprintf(battString, sizeof(battString), "Batt: %.1fV %.0f%%", lipo.cellVoltage(), lipo.cellPercent());
            // Check if it fits; approx 16px font height
            if (canvas.getCursorY() < (135 - 16)) {
                canvas.println(battString);
            }

        } else { // GPS Valid
            canvas.setTextColor(ST77XX_GREEN);
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Lat: %.5f", localGpsData.latitude);
            canvas.println(gpsLineBuffer); // Line 1 (Y=20)

            canvas.setCursor(10, 45); // Line 2
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Lon: %.5f", localGpsData.longitude);
            canvas.println(gpsLineBuffer);

            canvas.setCursor(10, 70); // Line 3
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Speed: %.1f m/s", localGpsData.speed_mps);
            canvas.println(gpsLineBuffer);

            canvas.setCursor(10, 95); // Line 4
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Alt: %.1f m", localGpsData.altitude_meters);
            canvas.println(gpsLineBuffer);

            canvas.setCursor(10, 120); // Line 5
            canvas.setTextColor(ST77XX_ORANGE);
            snprintf(gpsLineBuffer, sizeof(gpsLineBuffer), "Sats: %u Fix: %u", localGpsData.satellites, localGpsData.fix_quality);
            canvas.println(gpsLineBuffer);

            // No space for battery info in GPS valid mode with 5 lines of GPS data and current font.
            // It would overwrite or exceed screen bounds.
        }
    }

    display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);

    vTaskDelay(pdMS_TO_TICKS(250)); // Update rate (e.g., 4 Hz)
  }
}

// --- Display Initialization ---
bool initializeDisplay() {
  // delay(5000); // Long delay, consider reducing or removing if not essential for hardware init
  Serial.println("Initializing Display...");

  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH); // Turn on backlight early

  // Initialize Mode Switch Button
  pinMode(MODE_SWITCH_BUTTON_PIN, INPUT_PULLDOWN);
  Serial.println("Mode switch button D2 (GPIO" + String(MODE_SWITCH_BUTTON_PIN) + ") initialized.");

  TB.neopixelPin = PIN_NEOPIXEL;
  TB.neopixelNum = 1;
  TB.begin(); // Corrected: TB.begin() is void
  Serial.println(F("TestBed initialized.")); // Optional log
  TB.setColor(0x000020); // Dim blue during init, can be changed to WHITE or other color as per original intent

  display.init(135, 240);           // Init ST7789 240x135 TFT
  display.setRotation(3);           // Rotate to landscape
  canvas.setFont(&FreeSans12pt7b);  // Set font for the canvas
  canvas.setTextColor(ST77XX_WHITE); // Default text color
  canvas.setTextWrap(false);        // Disable text wrap to control layout

  // Initialize MAX17048 Lipo Fuel Gauge
  if (!lipo.begin()) {
    Serial.println(F("Could not find Adafruit MAX17048. Check wiring and ensure battery is connected."));
    // Depending on requirements, may want to return false or set an error state
    // For now, continue, but battery readings will be invalid.
  } else {
    Serial.print(F("Found MAX17048. Chip ID: 0x"));
    Serial.println(lipo.getChipID(), HEX);
  }
    
  // Initialize BME280 (optional, based on your hardware setup)
  // Keeping the BME280 init code, but ensure it doesn't halt if not found.
  if (TB.scanI2CBus(BME280_ADDRESS_ALTERNATE)) { // Check if BME280 is present at its typical address
    unsigned status = bme.begin(BME280_ADDRESS_ALTERNATE);
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      // Log details if needed, but don't halt
    } else {
      Serial.println("BME280 found and initialized.");
      bmefound = true; // Assuming bmefound is a global/static variable
    }
  } else {
    Serial.println("BME280 not found at default address during scan.");
  }

  // Initialize Buttons (already done in original code, just ensure it's logical)
  pinMode(BUTTON_A_PIN, INPUT_PULLUP); // Assuming BUTTON_A_PIN is 0
  // Add other button pins if used

  // Perform I2C scan (can be useful for debugging, but might not be needed in final version)
  Serial.println("I2C Scan Results:");
  for (uint8_t i = 0x01; i <= 0x7F; i++) {
    if (TB.scanI2CBus(i, 0)) { // Scan without writing
      Serial.print("Found I2C device at 0x");
      if (i < 0x10) Serial.print("0");
      Serial.print(i, HEX);
      Serial.println();
      valid_i2c[i] = true; // Assuming valid_i2c is a global/static array
    } else {
      valid_i2c[i] = false;
    }
  }
  
  digitalWrite(TFT_BACKLITE, HIGH); // Ensure backlight is on
  TB.setColor(0x000000); // Turn off Neopixel after init
  Serial.println("Display Initialization Complete.");
  return true; // Successfully initialized display components
}

