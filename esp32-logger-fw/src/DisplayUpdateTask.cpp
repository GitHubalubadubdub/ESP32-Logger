#include "DisplayUpdateTask.h"
#include "config.h"

#include "Adafruit_MAX1704X.h"
#include <Adafruit_NeoPixel.h>
#include "Adafruit_TestBed.h"
#include <Adafruit_BME280.h>
#include <Adafruit_ST7789.h> 
#include <Fonts/FreeSans12pt7b.h>


Adafruit_BME280 bme; // I2C
bool bmefound = false;
extern Adafruit_TestBed TB;

Adafruit_MAX17048 lipo;
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 canvas(240, 135);

static bool valid_i2c[128]; // File-scope static for access by both functions

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
  // bool newDataToDisplay = false; // Optional: Only update screen if newData

  for (;;) { // Infinite loop for the task
    // Read shared data
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        power = g_powerCadenceData.power;
        cadence = g_powerCadenceData.cadence;
        // Optional: Reset newData flag if you are using it
        // if (g_powerCadenceData.newData) {
        //    newDataToDisplay = true;
        //    g_powerCadenceData.newData = false;
        // }
        xSemaphoreGive(g_dataMutex);
    } else {
        Serial.println("Display task failed to get mutex. Displaying potentially stale data.");
        // Optionally, indicate stale data on screen
    }

    // Update display (consider only if newDataToDisplay is true for efficiency)
    canvas.fillScreen(ST77XX_BLACK); // Clear canvas

    // Font is set in initializeDisplay()
    // canvas.setFont(&FreeSans12pt7b);

    // Display Power
    canvas.setCursor(10, 35); // Adjusted for 12pt font height
    canvas.setTextColor(ST77XX_GREEN);
    canvas.print("Power: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(power);
    canvas.println(" W");

    // Display Cadence
    canvas.setCursor(10, 70); // Position below power
    canvas.setTextColor(ST77XX_GREEN);
    canvas.print("Cadence: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(cadence);
    canvas.println(" RPM");

    // Display other sensor data if needed (e.g. battery, GPS status)
    // Example: Battery
    canvas.setCursor(10, 105);
    canvas.setTextColor(ST77XX_YELLOW);
    canvas.print("Batt: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(lipo.cellVoltage(), 1);
    canvas.print("V ");
    canvas.print(lipo.cellPercent(), 0);
    canvas.println("%");


    // Update the physical display
    display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);

    // TB.setColor(TB.Wheel(j++)); // Removed Neopixel cycling for now
    vTaskDelay(pdMS_TO_TICKS(250)); // Update rate (e.g., 4 Hz)
  }
}

// --- Display Initialization ---
bool initializeDisplay() {
  // delay(5000); // Long delay, consider reducing or removing if not essential for hardware init
  Serial.println("Initializing Display...");

  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH); // Turn on backlight early

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

