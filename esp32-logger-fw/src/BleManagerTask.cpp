#include "BleManagerTask.h"
#include "config.h" // May not be needed directly yet

#include <NimBLEDevice.h> // Main NimBLE library

// BLE Service and Characteristic UUIDs
static NimBLEUUID CYCLING_POWER_SERVICE_UUID("0x1818");
static NimBLEUUID CYCLING_POWER_MEASUREMENT_CHAR_UUID("0x2A63");
// static NimBLEUUID CYCLING_POWER_FEATURE_CHAR_UUID("0x2A65"); // Optional: To check supported features
// static NimBLEUUID CSC_MEASUREMENT_CHAR_UUID("0x2A5B"); // For cadence if separate. Note: Cadence is often part of 0x2A63

// Shared variables for BLE data
static uint16_t currentPower = 0;
static uint8_t currentCadence = 0;
static String bleStatus = "Initializing";
static NimBLEAdvertisedDevice* foundDevice = nullptr;
static NimBLEClient* pClient = nullptr;
static boolean doConnect = false;
static boolean connected = false;
static NimBLERemoteCharacteristic* pPowerMeasurementChar = nullptr;

// Mutex for thread-safe access to shared variables
static portMUX_TYPE bleDataMutex = portMUX_INITIALIZER_UNLOCKED;

// Accessor functions for other tasks
uint16_t getPower() {
    uint16_t power;
    portENTER_CRITICAL(&bleDataMutex);
    power = currentPower;
    portEXIT_CRITICAL(&bleDataMutex);
    return power;
}

uint8_t getCadence() {
    uint8_t cadence;
    portENTER_CRITICAL(&bleDataMutex);
    cadence = currentCadence;
    portEXIT_CRITICAL(&bleDataMutex);
    return cadence;
}

String getBLEStatus() {
    String status;
    portENTER_CRITICAL(&bleDataMutex);
    status = bleStatus;
    portEXIT_CRITICAL(&bleDataMutex);
    return status;
}

static void updateStatus(String newStatus) {
    portENTER_CRITICAL(&bleDataMutex);
    bleStatus = newStatus;
    portEXIT_CRITICAL(&bleDataMutex);
    Serial.println("BLE Status: " + newStatus);
}

static void updatePowerAndCadence(uint16_t power, uint8_t cadence) {
    portENTER_CRITICAL(&bleDataMutex);
    currentPower = power;
    currentCadence = cadence;
    portEXIT_CRITICAL(&bleDataMutex);
    // Serial.printf("Power: %u W, Cadence: %u RPM
", power, cadence); // Too verbose for regular updates
}


// --- Forward declarations for callbacks ---
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient_param);
    void onDisconnect(NimBLEClient* pClient_param);
};

static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

/**
 * Scan for BLE servers and find the first one that advertises the Cycling Power Service.
 */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        if (advertisedDevice->isAdvertisingService(CYCLING_POWER_SERVICE_UUID)) {
            Serial.println("Found Cycling Power Service!");
            updateStatus("Device Found");
            NimBLEDevice::getScan()->stop(); // Stop scan before connecting
            if(foundDevice != nullptr) { // Manage memory if a device was previously found but not connected
                delete foundDevice;
                foundDevice = nullptr;
            }
            foundDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            doConnect = true;
        }
    }
};

// --- Client Callbacks Implementation ---
void ClientCallbacks::onConnect(NimBLEClient* cl) {
    updateStatus("Connected");
    connected = true;
    pClient = cl; // Assign the connected client to the global pClient
    Serial.println("Connected to server. Discovering services...");

    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChar = nullptr;

    // 1. Get the Cycling Power service
    pSvc = pClient->getService(CYCLING_POWER_SERVICE_UUID);
    if (pSvc) {
        Serial.println("Cycling Power Service found.");
        // 2. Get the Cycling Power Measurement characteristic
        pPowerMeasurementChar = pSvc->getCharacteristic(CYCLING_POWER_MEASUREMENT_CHAR_UUID);
        if (pPowerMeasurementChar) {
            Serial.println("Cycling Power Measurement Characteristic found.");
            // 3. Register for notifications
            if (pPowerMeasurementChar->canNotify()) {
                if (pPowerMeasurementChar->subscribe(true, notifyCallback)) {
                    updateStatus("Subscribed to Notifications");
                    Serial.println("Subscribed to power measurement notifications.");
                } else {
                    updateStatus("Subscription Failed");
                    Serial.println("Failed to subscribe to notifications.");
                    pClient->disconnect(); // Disconnect if subscription fails
                }
            } else {
                updateStatus("Notify Not Supported");
                Serial.println("Power measurement characteristic does not support notifications.");
                pClient->disconnect();
            }
        } else {
            updateStatus("Char Not Found");
            Serial.println("Cycling Power Measurement Characteristic not found.");
            pClient->disconnect();
        }
    } else {
        updateStatus("Service Not Found");
        Serial.println("Cycling Power Service not found.");
        pClient->disconnect();
    }
}

void ClientCallbacks::onDisconnect(NimBLEClient* cl) {
    updateStatus("Disconnected");
    connected = false;
    pPowerMeasurementChar = nullptr; // Clear characteristic pointer
    // pClient = nullptr; // Client pointer is managed by NimBLE, or deleted if created with 'new' explicitly.
                      // Re-creating client on next connection attempt if it's null.
    Serial.println("Disconnected from server.");
    // Consider adding a delay or specific logic before allowing automatic rescan/reconnect
}

// --- Notify Callback Implementation ---
static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (pRemoteCharacteristic->getUUID().equals(CYCLING_POWER_MEASUREMENT_CHAR_UUID)) {
        // Parse Cycling Power Measurement data (ตาม Bluetooth SIG XML for 0x2A63)
        // Flags (first 2 bytes, little endian)
        // uint16_t flags = (pData[1] << 8) | pData[0];
        
        uint8_t offset = 2; // Start after flags

        // Instantaneous Power (SINT16, Watts)
        int16_t power = (pData[offset+1] << 8) | pData[offset];
        offset += 2;

        uint8_t cadence_value = 0; // Default to 0

        // Check Flags for Cadence data (Bit 5: Crank Revolution Data Present)
        // The exact structure depends on the Cycling Power Measurement characteristic's definition.
        // A common format is: Flags (2 bytes), Inst Power (2 bytes), Pedal Power Balance (1 byte, if present),
        // Accumulated Torque (2 bytes, if present), Crank Revolution Data (Cumulative Crank Revs (2 bytes), Last Crank Event Time (2 bytes))
        // For simplicity, we'll assume if cadence is present, it's directly after power or indicated by flags.
        // The Favero Assioma typically includes cadence. Let's assume a common payload structure.
        // For example, if flags bit 5 (0x20) is set, crank revolution data might follow.
        // However, some power meters send cadence differently or it's part of a combined field.
        // A simple approach for Favero might be to look at a known byte offset if the payload is fixed,
        // or more robustly, fully parse according to the flags.

        // Example: A simplified parsing assuming cadence might be at a fixed position or not present.
        // This part NEEDS to be verified with actual Favero Assioma data format.
        // The Cycling Power Measurement characteristic has flags that indicate which fields are present.
        // Bit 0: Pedal Power Balance Present
        // Bit 1: Pedal Power Balance Reference (0=Unknown, 1=Left)
        // Bit 2: Accumulated Torque Present
        // Bit 3: Accumulated Torque Source (0=Wheel, 1=Crank)
        // Bit 4: Wheel Revolution Data Present
        // Bit 5: Crank Revolution Data Present (THIS IS CADENCE RELATED)
        // Bit 6: Extreme Force Magnitudes Present
        // Bit 7: Extreme Torque Magnitudes Present
        // Bit 8: Extreme Angles Present
        // Bit 9: Top Dead Spot Angle Present
        // Bit 10: Bottom Dead Spot Angle Present
        // Bit 11: Accumulated Energy Present
        // Bit 12: Offset Compensation Indicator (1=True)

        uint16_t flags = *((uint16_t*)pData); // Assuming little endian for flags

        // If Crank Revolution Data is present (Bit 5 of flags)
        if (flags & (1 << 5)) {
            // Search for Crank Revolution Data fields
            // This requires skipping optional fields based on other flags if they come before crank data.
            // For simplicity, let's assume if Pedal Power Balance (bit 0) is present, it's 1 byte.
            if (flags & (1 << 0)) { // Pedal Power Balance
                offset += 1; 
            }
            // If Accumulated Torque (bit 2) is present, it's 2 bytes.
            if (flags & (1 << 2)) { // Accumulated Torque
                offset += 2;
            }
            // Now, Crank Revolution Data: Cumulative Crank Revolutions (UINT16) and Last Crank Event Time (UINT16)
            if (length >= offset + 4) { // Check if there's enough data for crank revs and time
                // uint16_t cumulativeCrankRevs = (pData[offset+1] << 8) | pData[offset];
                // uint16_t lastCrankEventTime = (pData[offset+3] << 8) | pData[offset+2];
                // Cadence is calculated from changes in these two over time.
                // For many power meters, the "Instantaneous Cadence" field might be sent if flags indicate.
                // Let's check if "Instantaneous Cadence" (value rpm) is sent directly as part of an extended data field.
                // The standard characteristic doesn't define a direct "instantaneous cadence" field this way,
                // it's usually calculated or sent via CSC service.
                // However, Favero might send it in a non-standard way or rely on the client to calculate from crank events.

                // HACK/PLACEHOLDER: Favero Assioma might put cadence directly in the payload if a certain flag is set,
                // or it might be part of a combined field.
                // A common way is to send instantaneous cadence in the "Crank Revolution Data" part.
                // If flags bit 5 is set, and if the data length allows, let's assume the next byte after power
                // (and after optional fields like pedal balance) is cadence. This is a guess.
                // A more robust parser would look at all flags.
                // Often, if cadence is present, it's the two bytes after power:
                // Cumulative Crank Revolutions (uint16_t) and Last Crank Event Time (uint16_t).
                // The actual RPM is usually calculated by the client.
                // Some devices might put instantaneous cadence in the last byte if the payload is short.
                
                // For Favero, it's common to have:
                // Flags (2 bytes), Inst Power (2 bytes), Inst Cadence (1 byte) when pedal power balance is NOT present.
                // Flags (2 bytes), Inst Power (2 bytes), Pedal Power Balance (1 byte), Inst Cadence (1 byte) when present.
                // So, if bit 0 of flags (Pedal Power Balance Present) is 0:
                if (! (flags & (1 << 0)) ) { // If Pedal Power Balance is NOT present
                    if (length >= offset + 1) { // Check if cadence byte is there
                         cadence_value = pData[offset];
                         // offset += 1; // If there were more fields after
                    }
                } else { // If Pedal Power Balance IS present
                    offset +=1; // Skip Pedal Power Balance byte
                    if (length >= offset + 1) { // Check if cadence byte is there
                        cadence_value = pData[offset];
                        // offset += 1;
                    }
                }
            }
        }
        
        updatePowerAndCadence(power, cadence_value);
        // Serial.printf("Raw P: %d, C: %d | Flags: %04X, Length: %d
", power, cadence_value, flags, length);
    }
}


// --- Main BLE Task ---
void bleManagerTask(void *pvParameters) {
    Serial.println("BLE Manager Task started");
    updateStatus("BLE Init");

    NimBLEDevice::init(""); 
    NimBLEScan* pScan = NimBLEDevice::getScan(); 
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setActiveScan(true); 
    pScan->setInterval(100);    
    pScan->setWindow(99);       

    updateStatus("Scanning...");

    for (;;) {
        if (doConnect) {
            if (foundDevice != nullptr) {
                updateStatus("Connecting to: " + String(foundDevice->getAddress().toString().c_str()));
                
                if (pClient == nullptr) {
                    pClient = NimBLEDevice::createClient();
                    pClient->setClientCallbacks(new ClientCallbacks(), false); 
                    pClient->setConnectionParams(12, 12, 0, 51); // Example: Fast connection params
                }

                // Attempt to connect
                if (pClient->connect(foundDevice)) { 
                    Serial.println("Connection attempt successful (may still be handshaking, see onConnect).");
                    // Service discovery and characteristic subscription now happens in ClientCallbacks::onConnect
                } else {
                    Serial.println("Connection attempt failed immediately.");
                    // If connect() returns false, pClient might be in an invalid state or null.
                    // Safe to delete and nullify if we created it.
                    // However, NimBLE might handle this. For safety, if we are to retry, ensure pClient is valid or recreated.
                    // NimBLEDevice::deleteClient(pClient); // This might be too aggressive.
                    // pClient = nullptr;
                    updateStatus("Connect Failed");
                }
                delete foundDevice; // We are done with this advertised device object
                foundDevice = nullptr;
            }
            doConnect = false; 
        }

        if (!connected && !doConnect) {
            if (!pScan->isScanning()) {
                 updateStatus("Scanning...");
                 pScan->start(5, false); // Scan for 5 seconds, then stop. Loop will restart it if needed.
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}
