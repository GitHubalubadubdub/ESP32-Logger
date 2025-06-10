#include "BleManagerTask.h"
#include "shared_state.h" // Added for g_debugSettings
#include <NimBLEDevice.h>
#include "config.h" // For g_powerCadenceData, g_dataMutex, BleConnectionState, types.h
#include <Arduino.h> // For Serial prints and other Arduino functions
#include <string>    // For std::string
#include <cstring>   // For memset, strncpy

// Static global variables for this file
static NimBLEScan* pBLEScan;
static bool s_deadSpotAnglesSupported = false;
static NimBLEClient* pClient = nullptr;
static boolean doConnect = false;
static NimBLEAdvertisedDevice* myDevice = nullptr; // Store the advertised device object
static BLERemoteCharacteristic* pCyclingPowerMeasurementChar = nullptr;
static boolean connected = false;

// Notification Callback
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    static uint16_t prevCrankRevolutions = 0;
    static uint16_t prevCrankEventTime = 0; // In 1/1024s units
    static bool firstCrankDataPacketProcessed = false;

    if (length < 2) { // Minimum length for Flags
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleDebugStreamOn) {
                Serial.println("BLE Notify: Data length too short for flags.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
        return;
    }

    uint16_t flags = (pData[1] << 8) | pData[0];
    uint16_t finalPower = 0;
    uint8_t finalCadence = 0;
    float finalLeftPedalBalance = 50.0f;
    bool finalBalanceAvailable = false;
    uint16_t finalTopDeadSpotAngle = 0;
    bool finalTopDeadSpotAvailable = false;
    uint16_t finalBottomDeadSpotAngle = 0;
    bool finalBottomDeadSpotAvailable = false;

    // --- Parse Power ---
    if (length >= 4) {
        int16_t rawPower = (int16_t)((pData[3] << 8) | pData[2]);
        finalPower = (rawPower < 0) ? 0 : (uint16_t)rawPower;
    } else {
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleDebugStreamOn) {
                Serial.println("BLE Notify: Data length too short for power measurement.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
    }

    // --- Parse Pedal Power Balance ---
    bool pedalBalanceFlagPresent = (flags & 0x01);
    uint8_t currentOffset = 4;

    if (pedalBalanceFlagPresent) {
        if (length >= currentOffset + 1) {
            uint8_t balanceValue = pData[currentOffset];
            finalLeftPedalBalance = (float)balanceValue / 2.0f;
            finalBalanceAvailable = true;
            currentOffset += 1;
        } else {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleDebugStreamOn) {
                    Serial.println("BLE Notify: Pedal Balance flag set, but data length insufficient.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            finalBalanceAvailable = false;
        }
    }

    // --- Parse Cadence ---
    bool crankDataFlagPresent = (flags & 0x02);

    if (crankDataFlagPresent) {
        if (length >= currentOffset + 4) {
            uint16_t currentCrankRevolutions = (pData[currentOffset+1] << 8) | pData[currentOffset+0];
            uint16_t currentCrankEventTime = (pData[currentOffset+3] << 8) | pData[currentOffset+2];

            if (firstCrankDataPacketProcessed) {
                uint16_t deltaRevolutions;
                if (currentCrankRevolutions < prevCrankRevolutions) {
                    deltaRevolutions = (65535 - prevCrankRevolutions) + currentCrankRevolutions + 1;
                } else {
                    deltaRevolutions = currentCrankRevolutions - prevCrankRevolutions;
                }

                uint16_t deltaEventTime;
                if (currentCrankEventTime < prevCrankEventTime) {
                    deltaEventTime = (65535 - prevCrankEventTime) + currentCrankEventTime + 1;
                } else {
                    deltaEventTime = currentCrankEventTime - prevCrankEventTime;
                }

                if (deltaRevolutions > 0 && deltaEventTime > 0) {
                    double deltaTimeSeconds = (double)deltaEventTime / 1024.0;
                    finalCadence = (uint8_t)(((double)deltaRevolutions / deltaTimeSeconds) * 60.0);
                } else if (deltaEventTime > (1024 * 2)) {
                    finalCadence = 0;
                } else {
                    finalCadence = 0;
                }
            } else {
                firstCrankDataPacketProcessed = true;
                finalCadence = 0;
            }

            prevCrankRevolutions = currentCrankRevolutions;
            prevCrankEventTime = currentCrankEventTime;
            currentOffset += 4; // Advance offset after cadence data
        } else {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleDebugStreamOn) {
                    Serial.println("BLE Notify: Crank data flag set, but data length insufficient for crank fields.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            firstCrankDataPacketProcessed = false;
            finalCadence = 0;
        }
    } else {
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleDebugStreamOn) {
                // Serial.println("BLE Notify: Crank Revolution Data not present in flags."); // Can be noisy
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
        firstCrankDataPacketProcessed = false;
        finalCadence = 0;
    }

    // --- Parse Top/Bottom Dead Spot Angles ---
    bool extremeAnglesFlagPresent = (flags & (1 << 8));

    if (s_deadSpotAnglesSupported && extremeAnglesFlagPresent) {
        // Serial.println("Extreme Angles flag present and feature supported."); // Can be noisy
        if (flags & (1 << 9)) { // Top Dead Spot Angle Present
            if (length >= currentOffset + 2) {
                finalTopDeadSpotAngle = (pData[currentOffset+1] << 8) | pData[currentOffset+0];
                finalTopDeadSpotAvailable = true;
                currentOffset += 2;
                // Serial.printf("Parsed TDS Angle: %u\n", finalTopDeadSpotAngle); // Keep commented or wrap if enabled
            } else {
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleDebugStreamOn) {
                        Serial.println("BLE Notify: TDS Angle flag set, but data length insufficient.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
            }
        }
        if (flags & (1 << 10)) { // Bottom Dead Spot Angle Present
            if (length >= currentOffset + 2) {
                finalBottomDeadSpotAngle = (pData[currentOffset+1] << 8) | pData[currentOffset+0];
                finalBottomDeadSpotAvailable = true;
                currentOffset += 2;
                // Serial.printf("Parsed BDS Angle: %u\n", finalBottomDeadSpotAngle); // Keep commented or wrap if enabled
            } else {
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleDebugStreamOn) {
                        Serial.println("BLE Notify: BDS Angle flag set, but data length insufficient.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
            }
        }
    } else {
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleDebugStreamOn) {
                // if (!s_deadSpotAnglesSupported) { /* Serial.println("TDS/BDS angles feature not supported by device."); */ }
                // if (!extremeAnglesFlagPresent && s_deadSpotAnglesSupported) { /* Serial.println("Extreme Angles flag not present in this packet."); */ }
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
    }

    // --- Update Shared Data ---
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_powerCadenceData.power = finalPower;
        g_powerCadenceData.cadence = finalCadence;
        g_powerCadenceData.left_pedal_balance_percent = finalLeftPedalBalance;
        g_powerCadenceData.pedal_balance_available = finalBalanceAvailable;

        g_powerCadenceData.top_dead_spot_angle = finalTopDeadSpotAngle;
        g_powerCadenceData.top_dead_spot_available = finalTopDeadSpotAvailable;
        g_powerCadenceData.bottom_dead_spot_angle = finalBottomDeadSpotAngle;
        g_powerCadenceData.bottom_dead_spot_available = finalBottomDeadSpotAvailable;

        g_powerCadenceData.newData = true;

        xSemaphoreGive(g_dataMutex);
        /*
        Serial.printf("Processed Data -> P: %u, C: %u, LBal: %.1f%%(%s), TDS: %u(%s), BDS: %u(%s)\n",
                      finalPower, finalCadence,
                      finalLeftPedalBalance, finalBalanceAvailable ? "Y" : "N",
                      finalTopDeadSpotAngle, finalTopDeadSpotAvailable ? "Y" : "N",
                      finalBottomDeadSpotAngle, finalBottomDeadSpotAvailable ? "Y" : "N");
        */
        // This ^^^ Serial.printf is a good candidate for wrapping if it were active.
        // Example for the above commented out printf:
        // if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
        //     if (g_debugSettings.bleDebugStreamOn) {
        //         Serial.printf("Processed Data -> P: %u, C: %u, LBal: %.1f%%(%s), TDS: %u(%s), BDS: %u(%s)\n",
        //                       finalPower, finalCadence,
        //                       finalLeftPedalBalance, finalBalanceAvailable ? "Y" : "N",
        //                       finalTopDeadSpotAngle, finalTopDeadSpotAvailable ? "Y" : "N",
        //                       finalBottomDeadSpotAngle, finalBottomDeadSpotAvailable ? "Y" : "N");
        //     }
        //     xSemaphoreGive(g_debugSettingsMutex);
        // }
    } else {
        // This is an operational message, not a continuous stream.
        Serial.println("NotifyCallback: Failed to get g_dataMutex for data update.");
    }
}

// Client Callback Class
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient_in) {
        Serial.print("Connected to BLE server: ");
        Serial.println(pclient_in->getPeerAddress().toString().c_str());
        connected = true;
        // pclient_in->updatePeerMTU(517); // Optional: Request larger MTU.

        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_powerCadenceData.bleState = BLE_CONNECTED;
            std::string name = pclient_in->getPeerAddress().toString();
            if (myDevice && myDevice->haveName()) {
                name = myDevice->getName();
                if (name.empty()) {
                     name = pclient_in->getPeerAddress().toString();
                }
            }
            strncpy(g_powerCadenceData.connectedDeviceName, name.c_str(), sizeof(g_powerCadenceData.connectedDeviceName) - 1);
            g_powerCadenceData.connectedDeviceName[sizeof(g_powerCadenceData.connectedDeviceName) - 1] = '\0';
            g_powerCadenceData.newData = true;
            xSemaphoreGive(g_dataMutex);
            Serial.printf("Device Name/Addr for display: %s\n", name.c_str());
        }
    }

    void onDisconnect(NimBLEClient* pclient_in) {
        Serial.print("Disconnected from BLE server: ");
        Serial.println(pclient_in->getPeerAddress().toString().c_str());
        connected = false;
        pClient = nullptr;
        doConnect = false;

        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_powerCadenceData.bleState = BLE_DISCONNECTED;
            g_powerCadenceData.newData = true; // Trigger display update
            // Keep device name for info, or clear: memset(g_powerCadenceData.connectedDeviceName, 0, sizeof(g_powerCadenceData.connectedDeviceName));
            xSemaphoreGive(g_dataMutex);
        }
    }
};

// Advertised Device Callback
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        // Serial.print("BLE Advertised Device found: ");
        // This entire block is for verbose advertised device logging.
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                // Serial.print("BLE Advertised Device found: ");
                // Serial.print(advertisedDevice->getName().c_str());
                // Serial.print(" Addr: ");
                // Serial.print(advertisedDevice->getAddress().toString().c_str());
                // Serial.print(" RSSI: ");
                // Serial.println(advertisedDevice->getRSSI());
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }

        if (advertisedDevice->isAdvertisingService(NimBLEUUID(CYCLING_POWER_SERVICE_UUID))) {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                     Serial.println("Found Cycling Power Service in advertisement!");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            // Check if we are already trying to connect or are connected to this device
            if (myDevice != nullptr && myDevice->getAddress().equals(advertisedDevice->getAddress()) && (doConnect || connected)) {
                // This message can be frequent if a device is repeatedly scanned while connecting/connected.
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleDebugStreamOn) {
                        Serial.println("BLE Scan: Already processing this device.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                return;
            }

            NimBLEDevice::getScan()->stop();
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Scan stopped by found device."); // Clarified message
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            // myDevice = new NimBLEAdvertisedDevice(*advertisedDevice); // Make a copy for long term storage
            myDevice = advertisedDevice;
            doConnect = true;
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Device stored, doConnect set to true.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }

            if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_powerCadenceData.bleState = BLE_CONNECTING;
                // Optionally, attempt to set name here if it's usually available and useful
                // For example:
                // if (myDevice->haveName()) {
                //    strncpy(g_powerCadenceData.connectedDeviceName, myDevice->getName().c_str(), sizeof(g_powerCadenceData.connectedDeviceName) - 1);
                //    g_powerCadenceData.connectedDeviceName[sizeof(g_powerCadenceData.connectedDeviceName) - 1] = '\0';
                // } else {
                //    memset(g_powerCadenceData.connectedDeviceName, 0, sizeof(g_powerCadenceData.connectedDeviceName)); // Clear if no name
                // }
                g_powerCadenceData.newData = true;
                xSemaphoreGive(g_dataMutex);
            }
        }
    }
};

// Function to connect to the server
bool connectToServer() {
    if (myDevice == nullptr) {
        Serial.println("connectToServer: myDevice is null, cannot connect.");
        return false;
    }

    // If pClient is null, create a new client. If it exists, ensure it's disconnected before trying to connect.
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
        if (!pClient) {
            Serial.println("Failed to create NimBLE client."); // Critical Error
            return false;
        }
        pClient->setClientCallbacks(new ClientCallbacks(), false); // false to not delete callbacks on disconnect
        Serial.println("BLE Client created."); // One-time status
    } else {
        if (pClient->isConnected() && pClient->getPeerAddress().equals(myDevice->getAddress())) {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Already connected to this device.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            return true; // Already connected
        }
        // If client exists but is for a different device or disconnected, it should be cleaned up.
        // The onDisconnect callback sets pClient to nullptr.
        // Or if connection fails, it's deleted.
    }

    if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
        if (g_debugSettings.bleActivityStreamOn) {
            Serial.print("Attempting to connect to device: ");
            Serial.println(myDevice->getAddress().toString().c_str());
        }
        xSemaphoreGive(g_debugSettingsMutex);
    }

    // Set connection parameters
    // pClient->setConnectionParams(12,12,0,51); // Example: interval 15ms, latency 0, timeout 510ms
                                             // May need adjustment for specific power meters

    if (!pClient->connect(myDevice)) {
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Failed to connect to device.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
        NimBLEDevice::deleteClient(pClient); // Delete client if connection failed
        pClient = nullptr;
        return false;
    }
    if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
        if (g_debugSettings.bleActivityStreamOn) {
            Serial.println("Successfully connected to device.");
        }
        xSemaphoreGive(g_debugSettingsMutex);
    }

    BLERemoteService* pSvc = nullptr;
    s_deadSpotAnglesSupported = false; // Reset before checking features of newly connected device
    try {
        pSvc = pClient->getService(CYCLING_POWER_SERVICE_UUID);
    } catch (const std::exception& e) { // NimBLE uses exceptions for some errors
        Serial.print("Exception while getting service: "); // Error
        Serial.println(e.what());
    }

    if (pSvc) { // Successfully got Cycling Power Service
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Found Cycling Power Service");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }

        // Attempt to read Cycling Power Feature characteristic (0x2A65)
        BLERemoteCharacteristic* pFeatureChar = nullptr;
        try {
            pFeatureChar = pSvc->getCharacteristic(NimBLEUUID((uint16_t)0x2A65));
        } catch (const std::exception& e) {
            Serial.print("Exception getting Feature characteristic: "); // Error
            Serial.println(e.what());
        }

        if (pFeatureChar && pFeatureChar->canRead()) {
            std::string featuresValue = pFeatureChar->readValue();
            if (featuresValue.length() >= 4) { // Feature is uint32_t
                uint32_t featuresBitmask = 0;
                // Assuming little-endian encoding for the characteristic value
                featuresBitmask |= (uint32_t)featuresValue[0];
                featuresBitmask |= (uint32_t)featuresValue[1] << 8;
                featuresBitmask |= (uint32_t)featuresValue[2] << 16;
                featuresBitmask |= (uint32_t)featuresValue[3] << 24;
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleActivityStreamOn) {
                        Serial.printf("Cycling Power Features Bitmask: 0x%08X\n", featuresBitmask);
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                // Check Bit 6 for "Top and Bottom Dead Spot Angles Supported"
                if (featuresBitmask & (1 << 6)) {
                    s_deadSpotAnglesSupported = true;
                    if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                        if (g_debugSettings.bleActivityStreamOn) {
                            Serial.println("Feature: Top/Bottom Dead Spot Angles SUPPORTED.");
                        }
                        xSemaphoreGive(g_debugSettingsMutex);
                    }
                } else {
                    s_deadSpotAnglesSupported = false;
                    if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                        if (g_debugSettings.bleActivityStreamOn) {
                            Serial.println("Feature: Top/Bottom Dead Spot Angles NOT supported.");
                        }
                        xSemaphoreGive(g_debugSettingsMutex);
                    }
                }
                // Update shared struct, useful for display task to know support without waiting for data
                if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_powerCadenceData.dead_spot_angles_supported = s_deadSpotAnglesSupported;
                    xSemaphoreGive(g_dataMutex);
                }

            } else {
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleActivityStreamOn) {
                        Serial.println("Failed to read valid Cycling Power Features or length too short.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                s_deadSpotAnglesSupported = false; // Default if not readable
            }
        } else {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Cycling Power Feature characteristic (0x2A65) not found or not readable.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            s_deadSpotAnglesSupported = false; // Default if not found
        }

        // Now get the measurement characteristic
        pCyclingPowerMeasurementChar = pSvc->getCharacteristic(CYCLING_POWER_MEASUREMENT_UUID);
        if (!pCyclingPowerMeasurementChar) {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Failed to find Cycling Power Measurement Characteristic.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            pClient->disconnect();
            return false;
        }
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Found Cycling Power Measurement Characteristic.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }

    } else { // if (!pSvc)
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Failed to find Cycling Power Service on connected device.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
        pClient->disconnect();
        return false;
    }

    if (pCyclingPowerMeasurementChar->canNotify()) {
        if (!pCyclingPowerMeasurementChar->subscribe(true, notifyCallback, false)) {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("Failed to subscribe to characteristic notifications.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            pClient->disconnect();
            return false;
        }
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Successfully subscribed to characteristic notifications.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
    } else {
        if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
            if (g_debugSettings.bleActivityStreamOn) {
                Serial.println("Characteristic does not support notifications.");
            }
            xSemaphoreGive(g_debugSettingsMutex);
        }
        pClient->disconnect(); // Cannot proceed if we can't get data
        return false;
    }

    connected = true; // Set connected state
    return true;
}

// BLE Manager Task
void bleManagerTask(void *pvParameters) {
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_powerCadenceData.bleState = BLE_IDLE;
        memset(g_powerCadenceData.connectedDeviceName, 0, sizeof(g_powerCadenceData.connectedDeviceName));
        g_powerCadenceData.newData = true;
        xSemaphoreGive(g_dataMutex);
    }
    Serial.println("BLE Manager Task started, initial state BLE_IDLE.");
    //vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for system to stabilize if needed (already have one before this)

    NimBLEDevice::init("");
    Serial.println("NimBLE initialized.");

    // Configure the scanner
    pBLEScan = NimBLEDevice::getScan();
    if (!pBLEScan) {
        Serial.println("Failed to get BLE scanner instance.");
        vTaskDelete(NULL); // Cannot proceed
        return;
    }
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), true); // true to delete callbacks when scan ends
    pBLEScan->setActiveScan(true);  // Active scan uses more power but gets more info (like device name)
    pBLEScan->setInterval(100);     // Scan interval in ms
    pBLEScan->setWindow(99);        // Scan window in ms (must be <= interval)
    pBLEScan->setFilterPolicy(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE); // Filter scan results by device address
                                                               // Helps in getting one result per device in a scan period
    pBLEScan->setLimitedOnly(false); // Scan for all devices, not just limited discoverable mode

    Serial.println("BLE Scanner configured. Starting main loop."); // One-time status

    for (;;) {
        if (doConnect && myDevice != nullptr && !connected) {
            if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) {
                    Serial.println("doConnect is true, myDevice is set, and not connected. Attempting connection...");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
            if (connectToServer()) {
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleActivityStreamOn) {
                        Serial.println("Connection successful. Monitoring connection.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                // Loop while connected, actual data comes via notifyCallback
                // The 'connected' flag is managed by ClientCallbacks
            } else {
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleActivityStreamOn) {
                        Serial.println("Connection attempt failed. Resetting flags.");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_powerCadenceData.bleState = BLE_DISCONNECTED;
                    g_powerCadenceData.newData = true;
                    xSemaphoreGive(g_dataMutex);
                }
                // Ensure pClient is cleaned up if connectToServer failed partway
                if (pClient != nullptr && !pClient->isConnected()) { // Check isConnected before deleting
                     NimBLEDevice::deleteClient(pClient); // This should trigger onDisconnect if connected
                }
                pClient = nullptr; // Ensure pClient is null after deletion or if it was not connected
                myDevice = nullptr;
                doConnect = false;
                connected = false;
            }
        } else if (!connected) {
            if (pBLEScan->isScanning() == false) {
                if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_powerCadenceData.bleState = BLE_SCANNING;
                    memset(g_powerCadenceData.connectedDeviceName, 0, sizeof(g_powerCadenceData.connectedDeviceName));
                    g_powerCadenceData.newData = true;
                    xSemaphoreGive(g_dataMutex);
                }
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.otherDebugStreamOn) {
                        Serial.println("Not connected and not scanning. Starting BLE scan...");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
                // pBLEScan->clearResults(); // Clear old scan results before starting
                if (pBLEScan->start(5, nullptr, false) == 0) { // Scan for 5s, no scan_eof_cb, blocking call
                     if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                         if (g_debugSettings.bleActivityStreamOn) {
                            Serial.println("Scan started successfully.");
                         }
                         xSemaphoreGive(g_debugSettingsMutex);
                     }
                } else {
                     // This "Failed to start scan" is already under otherDebugStreamOn. No change here.
                     if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                        if (g_debugSettings.otherDebugStreamOn) {
                            Serial.println("Failed to start scan (already running or other error).");
                        }
                        xSemaphoreGive(g_debugSettingsMutex);
                     }
                     // If scan fails to start, update state back to IDLE or DISCONNECTED
                     if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        g_powerCadenceData.bleState = BLE_IDLE; // Or DISCONNECTED
                        g_powerCadenceData.newData = true;
                        xSemaphoreGive(g_dataMutex);
                     }
                }
            } else {
                // Serial.println("Scan in progress..."); // This can be very noisy. Wrap if uncommented.
                if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                    if (g_debugSettings.bleActivityStreamOn) { // Changed to bleActivityStreamOn
                        // Serial.println("BLE MainLoop: Scan in progress...");
                    }
                    xSemaphoreGive(g_debugSettingsMutex);
                }
            }
        } else { // Is connected
             // Serial.println("BLE Connected. Waiting for notifications or disconnect."); // Also noisy. Wrap if uncommented.
             if (xSemaphoreTake(g_debugSettingsMutex, (TickType_t)10) == pdTRUE) {
                if (g_debugSettings.bleActivityStreamOn) { // Changed to bleActivityStreamOn
                    // Serial.println("BLE MainLoop: Connected. Waiting for notifications or disconnect.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
             }
             if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (g_powerCadenceData.bleState != BLE_CONNECTED) { // Update if state was changed elsewhere
                    g_powerCadenceData.bleState = BLE_CONNECTED;
                    g_powerCadenceData.newData = true;
                }
                // Refresh device name if it can change or was not set at connection
                // This is already handled in onConnect, so might be redundant unless name can change post-connection
                xSemaphoreGive(g_dataMutex);
             }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Reduced delay for faster state updates if needed
    }
}
