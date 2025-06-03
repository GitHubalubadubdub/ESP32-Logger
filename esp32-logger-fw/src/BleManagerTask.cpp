#include "BleManagerTask.h"
#include <NimBLEDevice.h>
#include "config.h" // For g_powerCadenceData, g_dataMutex, BleConnectionState, types.h
#include <Arduino.h> // For Serial prints and other Arduino functions
#include <string>    // For std::string
#include <cstring>   // For memset, strncpy

// Static global variables for this file
static NimBLEScan* pBLEScan;
static NimBLEClient* pClient = nullptr;
static boolean doConnect = false;
static NimBLEAdvertisedDevice* myDevice = nullptr; // Store the advertised device object
static BLERemoteCharacteristic* pCyclingPowerMeasurementChar = nullptr;
static boolean connected = false;

// Notification Callback
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    static uint16_t prevCrankRevolutions = 0;
    static uint16_t prevCrankEventTime = 0; // In 1/1024s units
    static bool firstCrankDataPacketProcessed = false; // Renamed for clarity

    // Serial.print("Notify callback for char ");
    // Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    // Serial.print(" data length ");
    // Serial.println(length);

    if (length < 2) { // Minimum length for Flags
        Serial.println("Data length too short for flags.");
        return;
    }

    uint16_t flags = (pData[1] << 8) | pData[0];
    uint16_t finalPower = 0;
    uint8_t finalCadence = 0;

    // --- Parse Power ---
    // Instantaneous Power is at offset 2 (bytes 2, 3), requires length >= 4
    if (length >= 4) {
        int16_t rawPower = (int16_t)((pData[3] << 8) | pData[2]);
        if (rawPower < 0) {
            finalPower = 0;
        } else {
            finalPower = (uint16_t)rawPower;
        }
    } else {
        Serial.println("Data length too short for power measurement.");
        // finalPower remains 0
    }

    // --- Parse Cadence ---
    bool crankDataFlagPresent = (flags & 0x02); // Check bit 1 for Crank Revolution Data

    if (crankDataFlagPresent) {
        uint8_t baseOffset = 4; // Starting offset after Flags (2 bytes) and Power (2 bytes)
        if (flags & 0x01) { // Check bit 0 for Pedal Power Balance Present
            baseOffset += 1; // Increment offset if Pedal Power Balance field is present
        }

        // Check if there's enough data for Accumulated Crank Revolutions (2 bytes) and Last Crank Event Time (2 bytes)
        if (length >= baseOffset + 4) {
            uint16_t currentCrankRevolutions = (pData[baseOffset+1] << 8) | pData[baseOffset+0];
            uint16_t currentCrankEventTime = (pData[baseOffset+3] << 8) | pData[baseOffset+2]; // In 1/1024s

            if (firstCrankDataPacketProcessed) {
                uint16_t deltaRevolutions;
                if (currentCrankRevolutions < prevCrankRevolutions) { // Rollover
                    deltaRevolutions = (65535 - prevCrankRevolutions) + currentCrankRevolutions + 1;
                } else {
                    deltaRevolutions = currentCrankRevolutions - prevCrankRevolutions;
                }

                uint16_t deltaEventTime; // In 1/1024s
                if (currentCrankEventTime < prevCrankEventTime) { // Rollover
                    deltaEventTime = (65535 - prevCrankEventTime) + currentCrankEventTime + 1;
                } else {
                    deltaEventTime = currentCrankEventTime - prevCrankEventTime;
                }

                if (deltaRevolutions > 0 && deltaEventTime > 0) {
                    double deltaTimeSeconds = (double)deltaEventTime / 1024.0;
                    finalCadence = (uint8_t)(((double)deltaRevolutions / deltaTimeSeconds) * 60.0);
                } else if (deltaEventTime > (1024 * 2)) { // More than 2 seconds with no new crank revolution
                    finalCadence = 0; // Cadence is 0 if no pedaling for a while
                } else {
                    // No new revolutions in a short interval, or deltaEventTime is 0.
                    finalCadence = 0;
                }
            } else {
                // This is the first packet with crank data. Cadence is 0 until the next packet.
                firstCrankDataPacketProcessed = true;
                finalCadence = 0;
            }

            prevCrankRevolutions = currentCrankRevolutions;
            prevCrankEventTime = currentCrankEventTime;

        } else {
            Serial.println("Crank data flag set, but data length insufficient for crank fields.");
            firstCrankDataPacketProcessed = false; // Reset if data becomes too short
            finalCadence = 0;
        }
    } else {
        Serial.println("Crank Revolution Data not present in flags.");
        firstCrankDataPacketProcessed = false; // Reset if flag is not set
        finalCadence = 0;
    }

    // --- Update Shared Data ---
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_powerCadenceData.power = finalPower;
        g_powerCadenceData.cadence = finalCadence;
        g_powerCadenceData.newData = true;
        xSemaphoreGive(g_dataMutex);
        Serial.printf("Processed Data -> Power: %u W, Cadence: %u RPM\n", finalPower, finalCadence);
    } else {
        Serial.println("NotifyCallback: Failed to get mutex for data update.");
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
        Serial.print("BLE Advertised Device found: ");
        Serial.print(advertisedDevice->getName().c_str());
        Serial.print(" Addr: ");
        Serial.print(advertisedDevice->getAddress().toString().c_str());
        Serial.print(" RSSI: ");
        Serial.println(advertisedDevice->getRSSI());

        if (advertisedDevice->isAdvertisingService(NimBLEUUID(CYCLING_POWER_SERVICE_UUID))) {
            Serial.println("Found Cycling Power Service in advertisement!");
            // Check if we are already trying to connect or are connected to this device
            if (myDevice != nullptr && myDevice->getAddress().equals(advertisedDevice->getAddress()) && (doConnect || connected)) {
                Serial.println("Already processing this device.");
                return;
            }

            NimBLEDevice::getScan()->stop();
            Serial.println("Scan stopped.");
            // myDevice = new NimBLEAdvertisedDevice(*advertisedDevice); // Make a copy for long term storage
            myDevice = advertisedDevice;
            doConnect = true;
            Serial.println("Device stored, doConnect set to true.");

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
            Serial.println("Failed to create NimBLE client.");
            return false;
        }
        pClient->setClientCallbacks(new ClientCallbacks(), false); // false to not delete callbacks on disconnect
        Serial.println("BLE Client created.");
    } else {
        if (pClient->isConnected() && pClient->getPeerAddress().equals(myDevice->getAddress())) {
            Serial.println("Already connected to this device.");
            return true; // Already connected
        }
        // If client exists but is for a different device or disconnected, it should be cleaned up.
        // The onDisconnect callback sets pClient to nullptr.
        // Or if connection fails, it's deleted.
    }

    Serial.print("Attempting to connect to device: ");
    Serial.println(myDevice->getAddress().toString().c_str());

    // Set connection parameters
    // pClient->setConnectionParams(12,12,0,51); // Example: interval 15ms, latency 0, timeout 510ms
                                             // May need adjustment for specific power meters

    if (!pClient->connect(myDevice)) {
        Serial.println("Failed to connect to device.");
        NimBLEDevice::deleteClient(pClient); // Delete client if connection failed
        pClient = nullptr;
        return false;
    }
    Serial.println("Successfully connected to device.");

    BLERemoteService* pSvc = nullptr;
    try {
        pSvc = pClient->getService(CYCLING_POWER_SERVICE_UUID);
    } catch (const std::exception& e) { // NimBLE uses exceptions for some errors
        Serial.print("Exception while getting service: ");
        Serial.println(e.what());
    }

    if (!pSvc) {
        Serial.println("Failed to find Cycling Power Service on connected device.");
        pClient->disconnect(); // Disconnect if service not found
        // pClient will be set to nullptr by onDisconnect callback
        return false;
    }
    Serial.println("Found Cycling Power Service.");

    pCyclingPowerMeasurementChar = pSvc->getCharacteristic(CYCLING_POWER_MEASUREMENT_UUID);
    if (!pCyclingPowerMeasurementChar) {
        Serial.println("Failed to find Cycling Power Measurement Characteristic.");
        pClient->disconnect();
        return false;
    }
    Serial.println("Found Cycling Power Measurement Characteristic.");

    if (pCyclingPowerMeasurementChar->canNotify()) {
        if (!pCyclingPowerMeasurementChar->subscribe(true, notifyCallback, false)) { // false for notifications, true for indications
            Serial.println("Failed to subscribe to characteristic notifications.");
            pClient->disconnect();
            return false;
        }
        Serial.println("Successfully subscribed to characteristic notifications.");
    } else {
        Serial.println("Characteristic does not support notifications.");
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

    Serial.println("BLE Scanner configured. Starting main loop.");

    for (;;) {
        if (doConnect && myDevice != nullptr && !connected) {
            Serial.println("doConnect is true, myDevice is set, and not connected. Attempting connection...");
            if (connectToServer()) {
                Serial.println("Connection successful. Monitoring connection.");
                // Loop while connected, actual data comes via notifyCallback
                // The 'connected' flag is managed by ClientCallbacks
            } else {
                Serial.println("Connection attempt failed. Resetting flags.");
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
                Serial.println("Not connected and not scanning. Starting BLE scan...");
                // pBLEScan->clearResults(); // Clear old scan results before starting
                if (pBLEScan->start(5, nullptr, false) == 0) { // Scan for 5s, no scan_eof_cb, blocking call
                     Serial.println("Scan started successfully.");
                } else {
                     Serial.println("Failed to start scan (already running or other error).");
                     // If scan fails to start, update state back to IDLE or DISCONNECTED
                     if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        g_powerCadenceData.bleState = BLE_IDLE; // Or DISCONNECTED
                        g_powerCadenceData.newData = true;
                        xSemaphoreGive(g_dataMutex);
                     }
                }
            } else {
                // Serial.println("Scan in progress..."); // This can be very noisy
            }
        } else { // Is connected
             // Serial.println("BLE Connected. Waiting for notifications or disconnect."); // Also noisy
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
