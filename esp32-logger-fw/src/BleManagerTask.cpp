#include "BleManagerTask.h"
#include <NimBLEDevice.h>
#include "config.h" // For g_powerCadenceData, g_dataMutex
#include <Arduino.h> // For Serial prints and other Arduino functions

// Static global variables for this file
static NimBLEScan* pBLEScan;
static NimBLEClient* pClient = nullptr;
static boolean doConnect = false;
static NimBLEAdvertisedDevice* myDevice = nullptr; // Store the advertised device object
static BLERemoteCharacteristic* pCyclingPowerMeasurementChar = nullptr;
static boolean connected = false;

// Notification Callback
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("Notify callback for char ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" data length ");
    Serial.println(length);

    if (pBLERemoteCharacteristic->getUUID().equals(NimBLEUUID(CYCLING_POWER_MEASUREMENT_UUID))) {
        if (length >= 4) { // Minimum length for Flags (2 bytes) and Instantaneous Power (2 bytes)
            // Instantaneous Power is a sint16, at offset 2 (0-indexed)
            int16_t power = (int16_t)((pData[3] << 8) | pData[2]);

            uint8_t cadence = 0; // Default cadence to 0

            // Cadence calculation from Crank Revolution Data (if available)
            // Flags are in pData[0] and pData[1]. Bit 0 (0x01) indicates Crank Revolution Data support.
            bool crankRevSupported = (pData[0] & 0x01);
            if (crankRevSupported && length >= 9) { // Check length for Accumulated Crank Revs and Last Crank Event Time
                // This is a simplified approach. True cadence calculation requires storing previous values
                // and handling rollovers for both crank revolutions and event time.
                // For now, we'll just extract the fields if present but not calculate full cadence.
                // uint16_t accumulatedCrankRevolutions = (pData[6] << 8) | pData[5];
                // uint16_t lastCrankEventTime = (pData[8] << 8) | pData[7]; // Time in 1/1024 seconds

                // A very basic placeholder if cadence was directly sent (not standard for 0x2A63 with crank data)
                // Example: if a sensor sends cadence directly in these bytes (unlikely for standard CPS)
                // cadence = pData[5]; // This is NOT how it works with crank data, just a placeholder idea
                Serial.println("Crank Revolution Data present, but full cadence calculation is complex and not yet implemented.");
                // To implement cadence:
                // 1. Store previous accumulatedCrankRevolutions and lastCrankEventTime.
                // 2. On new notification:
                //    deltaRevs = currentRevs - previousRevs (handle rollover)
                //    deltaTime = currentTime - previousTime (handle rollover, time is in 1/1024s)
                // 3. Cadence = (deltaRevs / (deltaTime / 1024.0)) * 60.0
                // This requires careful state management.
            }


            if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (power < 0) {
                    g_powerCadenceData.power = 0;
                } else {
                    g_powerCadenceData.power = (uint16_t)power;
                }
                g_powerCadenceData.cadence = cadence; // Update with actual cadence once calculation is implemented
                g_powerCadenceData.newData = true; // Flag for other tasks (e.g., display)
                xSemaphoreGive(g_dataMutex);
                Serial.printf("Power: %u W, Cadence: %u RPM (Cadence not fully implemented)\n", g_powerCadenceData.power, g_powerCadenceData.cadence);
            } else {
                Serial.println("Failed to take data mutex in notifyCallback");
            }
        } else {
            Serial.println("Received data length too short for power measurement.");
        }
    }
}

// Client Callback Class
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient_in) {
        Serial.print("Connected to BLE server: ");
        Serial.println(pclient_in->getPeerAddress().toString().c_str());
        connected = true;
        // pclient_in->updatePeerMTU(517); // Optional: Request larger MTU. Do this after connection.
                                        // Consider handling the callback for MTU change.
    }

    void onDisconnect(NimBLEClient* pclient_in) {
        Serial.print("Disconnected from BLE server: ");
        Serial.println(pclient_in->getPeerAddress().toString().c_str());
        connected = false;
        pClient = nullptr; // Crucial: Reset pClient to allow connectToServer to create a new one
        doConnect = false; // Allow main loop to rescan and connect
        // No need to delete pClient here, it's handled in connectToServer failure or main loop
        // NimBLEDevice::getScan()->start(5, false); // Optionally restart scan immediately
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
            myDevice = advertisedDevice; // Store a pointer to the found device.
                                         // The advertisedDevice object is owned by the NimBLEScan.
                                         // It might be deleted if not careful.
                                         // For robust solution, copy necessary data or the object itself.
                                         // However, NimBLE examples often pass this pointer directly.
                                         // Let's try with direct pointer first. If issues, create a copy.
            doConnect = true;
            Serial.println("Device stored, doConnect set to true.");
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
    Serial.println("BLE Manager Task started.");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for system to stabilize if needed

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
                // Ensure pClient is cleaned up if connectToServer failed partway
                if (pClient != nullptr && !pClient->isConnected()) {
                     NimBLEDevice::deleteClient(pClient);
                     pClient = nullptr;
                }
                myDevice = nullptr; // Clear device so scan can find a new one or re-find this one
                doConnect = false;  // Reset connect flag
                connected = false;  // Ensure connected is false
            }
        } else if (!connected) { // If not trying to connect and not connected, then scan
            if (pBLEScan->isScanning() == false) {
                Serial.println("Not connected and not scanning. Starting BLE scan...");
                // Start scan for a defined duration (e.g., 5 seconds).
                // The 'false' means scanEndedCallback is not used here, scan stops after duration.
                // If 0, it scans until explicitly stopped.
                if (pBLEScan->start(5, nullptr) == 0) { // 0 means success
                     Serial.println("Scan started successfully.");
                } else {
                     Serial.println("Failed to start scan.");
                }
            } else {
                Serial.println("Scan in progress...");
            }
        } else { // Is connected
             Serial.println("BLE Connected. Waiting for notifications or disconnect.");
             // Task doesn't need to do much here if relying on notifications and callbacks
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Check status periodically
    }
}
