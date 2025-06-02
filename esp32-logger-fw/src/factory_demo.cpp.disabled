// SPDX-FileCopyrightText: 2022 Limor Fried for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include <Arduino.h>
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
bool valid_i2c[128]; // Global array for I2C scan results, auto-initialized to false

void setup() {
  Serial.begin(115200);
  //while (! Serial) delay(10);
  delay(100);
  
  TB.neopixelPin = PIN_NEOPIXEL;
  TB.neopixelNum = 1; 
  TB.begin();
  TB.setColor(WHITE);

  display.init(135, 240);           // Init ST7789 240x135
  display.setRotation(3);
  pinMode(TFT_BACKLITE, OUTPUT);    // Configure backlight pin mode once
  digitalWrite(TFT_BACKLITE, HIGH); // Turn backlight on

  canvas.setFont(&FreeSans12pt7b);
  canvas.setTextColor(ST77XX_WHITE); 

  if (!lipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }
    
  Serial.print(F("Found MAX17048"));
  Serial.print(F(" with Chip ID: 0x")); 
  Serial.println(lipo.getChipID(), HEX);

  if (TB.scanI2CBus(0x77)) {
    Serial.println("BME280 address");

    unsigned status = bme.begin();  
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
      Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
      Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
      Serial.print("        ID of 0x60 represents a BME 280.\n");
      Serial.print("        ID of 0x61 represents a BME 680.\n");
      return;
    }
    Serial.println("BME280 found OK");
    bmefound = true;
  }

  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLDOWN);
  pinMode(2, INPUT_PULLDOWN);
}

int j = 0;

void loop() {
  // bool valid_i2c[128]; // Moved to global scope
  Serial.println("**********************");

  if (j == 0) {
    Serial.print("I2C scan: ");
    for (int i=0; i< 128; i++) {
      if (TB.scanI2CBus(i, 0)) {
        Serial.print("0x");
        Serial.print(i, HEX);
        Serial.print(", ");
        valid_i2c[i] = true;
      } 
      // No else needed, global valid_i2c is already initialized to false
    }
    Serial.println(); // Add a newline after scan results on Serial
  }

  if (j % 2 == 0) {
    canvas.fillScreen(ST77XX_BLACK);
    canvas.setCursor(0, 17); // Note: For FreeFonts, cursor Y is top. Font height for 12pt is >17. Consider increasing Y.
    canvas.setTextColor(ST77XX_RED);
    canvas.println("Adafruit Feather");
    canvas.setTextColor(ST77XX_YELLOW);
    canvas.println("ESP32-S3 TFT Demo");

    canvas.setTextColor(ST77XX_GREEN); 
    canvas.print("Battery: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(lipo.cellVoltage(), 1);
    canvas.print(" V  /  ");
    canvas.print(lipo.cellPercent(), 0);
    canvas.println("%");

    canvas.setTextColor(ST77XX_BLUE); 
    canvas.print("I2C: ");
    canvas.setTextColor(ST77XX_WHITE);
    bool found_any_i2c = false;
    for (uint8_t a=0x01; a<=0x7F; a++) {
      if (valid_i2c[a])  {
        canvas.print("0x");
        canvas.print(a, HEX);
        canvas.print(" "); // Use space instead of comma for brevity if many devices
        found_any_i2c = true;
      }
    }
    if (!found_any_i2c) {
        canvas.print("None");
    }
    canvas.println("");

    if (bmefound) {
        canvas.setTextColor(ST77XX_CYAN);
        canvas.print("Env: ");
        canvas.setTextColor(ST77XX_WHITE);
        canvas.print(bme.readTemperature(), 1);
        canvas.print("C, ");
        canvas.print(bme.readHumidity(), 0);
        canvas.println("%");
    }

    canvas.setTextColor(ST77XX_MAGENTA); // Color for "Buttons: " label
    canvas.print("Buttons: ");
    canvas.setTextColor(ST77XX_WHITE); // Color for button states
    bool any_button_pressed = false;
    if (!digitalRead(0)) {
      canvas.print("D0 ");
      any_button_pressed = true;
    }
    if (digitalRead(1)) {
      canvas.print("D1 ");
      any_button_pressed = true;
    }
    if (digitalRead(2)) {
      canvas.print("D2 ");
      any_button_pressed = true;
    }
    if (!any_button_pressed) {
        canvas.print("None");
    }
    canvas.println(""); // Newline after buttons

    display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);
    // pinMode(TFT_BACKLITE, OUTPUT); // Moved to setup
    digitalWrite(TFT_BACKLITE, HIGH); // Ensure backlight is on
  }
  
  TB.setColor(TB.Wheel(j++));
  delay(10);
  // return; // Redundant
}
