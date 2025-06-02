#include "BleManagerTask.h"
#include "config.h"

#include <NimBLEDevice.h>
#include <vector> // For std::vector

// BLE Service and Characteristic UUIDs (remain the same)
static NimBLEUUID CYCLING_POWER_SERVICE_UUID("0x1818");
static NimBLEUUID CYCLING_POWER_MEASUREMENT_CHAR_UUID("0x2A63");

// --- Shared variables for BLE data and status ---
static uint16_t currentPower = 0;
static uint8_t currentCadence = 0;
static String bleStatus = "Initializing"; // Will be updated: "Scanning", "Select Device", "Connecting", "Connected"
static NimBLEClient* pClient = nullptr;
static boolean connected = false; // True if actively connected to a device
static NimBLERemoteCharacteristic* pPowerMeasurementChar = nullptr;

// --- Variables for device discovery and selection ---
static std::vector<NimBLEAdvertisedDevice*> discoveredDevicesList;
static int selectedDeviceIndex = 0; // Default to 0, but check count before use
static bool scanRunning = false; // Flag to indicate if a scan is in progress
static bool connectFlag = false; // Flag to trigger connection attempt to selected device

// Mutex for thread-safe access to shared variables (power, cadence, status, device list, selectedIndex)
static portMUX_TYPE bleDataMutex = portMUX_INITIALIZER_UNLOCKED;


// --- Accessor functions for power, cadence, status (existing, ensure mutex usage) ---
uint16_t getPower() {
    uint16_t power_val; // Renamed to avoid conflict with static global
    portENTER_CRITICAL(&bleDataMutex);
    power_val = currentPower;
    portEXIT_CRITICAL(&bleDataMutex);
    return power_val;
}

uint8_t getCadence() {
    uint8_t cadence_val; // Renamed
    portENTER_CRITICAL(&bleDataMutex);
    cadence_val = currentCadence;
    portEXIT_CRITICAL(&bleDataMutex);
    return cadence_val;
}

String getBLEStatus() {
    String status_val; // Renamed
    portENTER_CRITICAL(&bleDataMutex);
    status_val = bleStatus;
    portEXIT_CRITICAL(&bleDataMutex);
    return status_val;
}

// --- Helper to update status (existing, ensure mutex usage) ---
static void updateStatus(String newStatus) {
    portENTER_CRITICAL(&bleDataMutex);
    if (bleStatus != newStatus) { // Only print if status changes
        bleStatus = newStatus;
        Serial.println("BLE Status: " + newStatus);
    }
    portEXIT_CRITICAL(&bleDataMutex);
}

// --- Helper to update power/cadence (existing, ensure mutex usage) ---
static void updatePowerAndCadence(uint16_t power_val, uint8_t cadence_val) {
    portENTER_CRITICAL(&bleDataMutex);
    currentPower = power_val;
    currentCadence = cadence_val;
    portEXIT_CRITICAL(&bleDataMutex);
}

// --- Clear discovered devices list (helper) ---
static void clearDiscoveredDevices() {
    portENTER_CRITICAL(&bleDataMutex);
    for (auto dev : discoveredDevicesList) {
        delete dev; // Delete the advertised device object copy
    }
    discoveredDevicesList.clear();
    selectedDeviceIndex = 0; // Reset index
    portEXIT_CRITICAL(&bleDataMutex);
}

// Forward declaration for AdvertisedDeviceCallbacks
class AdvertisedDeviceCallbacks;

// --- Public functions for discovery and selection ---
void startBleScan() {
    if (scanRunning) {
        Serial.println("Scan already in progress.");
        return;
    }
    if (connected) { // Don't scan if already connected
        Serial.println("Already connected, not starting scan.");
        return;
    }
    clearDiscoveredDevices();
    updateStatus("Scanning...");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan == nullptr) {
        Serial.println("Failed to get BLE Scan object");
        updateStatus("Scan Error");
        return;
    }
    pScan->setAdvertisedDeviceCallbacks(nullptr, false); // Clear and do not delete old callbacks object
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), true); // Auto-delete callback after scan
    pScan->setActiveScan(true);
    pScan->setInterval(100); // NimBLE units are 0.625ms, so 100 * 0.625ms = 62.5ms
    pScan->setWindow(99);    // Scan window, should be <= interval
    // Start scan for a fixed duration (e.g., 5 seconds), non-blocking.
    // The onScanComplete callback in AdvertisedDeviceCallbacks will handle the "Select Device" state.
    pScan->start(5, [](NimBLEScanResults results){ 
        scanRunning = false; // Reset flag when scan is complete
        if (getDiscoveredDeviceCount() == 0) {
            updateStatus("No Devices Found");
        } else {
            updateStatus("Select Device");
        }
        Serial.println("Scan ended.");
    }, false); // The 'false' here means the scan complete callback is a one-shot for this scan
    scanRunning = true;
}

int getDiscoveredDeviceCount() {
    int count;
    portENTER_CRITICAL(&bleDataMutex);
    count = discoveredDevicesList.size();
    portEXIT_CRITICAL(&bleDataMutex);
    return count;
}

NimBLEAdvertisedDevice* getDiscoveredDevice(int index) {
    NimBLEAdvertisedDevice* device = nullptr;
    portENTER_CRITICAL(&bleDataMutex);
    if (index >= 0 && index < discoveredDevicesList.size()) {
        device = discoveredDevicesList[index]; // Return pointer, not a copy
    }
    portEXIT_CRITICAL(&bleDataMutex);
    return device;
}

void setSelectedDeviceIndex(int index) {
    portENTER_CRITICAL(&bleDataMutex);
    int count = discoveredDevicesList.size();
    if (count > 0) { 
      selectedDeviceIndex = index;
      if (selectedDeviceIndex < 0) selectedDeviceIndex = 0; // Bound checks
      if (selectedDeviceIndex >= count) selectedDeviceIndex = count - 1;
    } else {
      selectedDeviceIndex = 0;
    }
    portEXIT_CRITICAL(&bleDataMutex);
}

int getSelectedDeviceIndex() {
    int index;
    portENTER_CRITICAL(&bleDataMutex);
    index = selectedDeviceIndex;
    portEXIT_CRITICAL(&bleDataMutex);
    return index;
}

bool connectToSelectedDevice() {
    if (connected) {
        Serial.println("Already connected.");
        return true; // Indicate already connected
    }
    if (scanRunning) {
        NimBLEDevice::getScan()->stop(); 
        scanRunning = false;
        Serial.println("Scan stopped for connection attempt.");
    }

    int currentSelection;
    portENTER_CRITICAL(&bleDataMutex);
    currentSelection = selectedDeviceIndex;
    portEXIT_CRITICAL(&bleDataMutex);

    if (currentSelection >= 0 && currentSelection < getDiscoveredDeviceCount()) {
        // No need to create a copy here for connectFlag logic, actual copy for connect() is in main task loop
        connectFlag = true; // Signal main loop to connect
        updateStatus("Connecting..."); // Update status early
        return true;
    }
    updateStatus("No Device Selected");
    return false;
}


// --- Callbacks (NimBLEAdvertisedDeviceCallbacks, NimBLEClientCallbacks, notifyCallback) ---
// AdvertisedDeviceCallbacks: modified to add to list
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        // Check only for Cycling Power Service
        if (advertisedDevice->isAdvertisingService(CYCLING_POWER_SERVICE_UUID)) {
            portENTER_CRITICAL(&bleDataMutex);
            bool found = false;
            for (auto dev : discoveredDevicesList) {
                if (dev->getAddress().equals(advertisedDevice->getAddress())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (discoveredDevicesList.size() < 10) { // Limit number of stored devices
                    Serial.print("Found Cycling Power Device: ");
                    Serial.println(advertisedDevice->toString().c_str());
                    discoveredDevicesList.push_back(new NimBLEAdvertisedDevice(*advertisedDevice)); 
                }
            }
            portEXIT_CRITICAL(&bleDataMutex);
        }
    }
};

// ClientCallbacks: onConnect, onDisconnect (largely same, but manage state for selection mode)
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* cl) {
        connected = true; // Set connected true before updating status
        pClient = cl; 
        updateStatus("Connected"); // Update status after connected flag is set
        Serial.println("Connected. Discovering services...");
        
        NimBLERemoteService* pSvc = pClient->getService(CYCLING_POWER_SERVICE_UUID);
        if (pSvc) {
            pPowerMeasurementChar = pSvc->getCharacteristic(CYCLING_POWER_MEASUREMENT_CHAR_UUID);
            if (pPowerMeasurementChar && pPowerMeasurementChar->canNotify()) {
                if(pPowerMeasurementChar->subscribe(true, notifyCallback)) {
                    Serial.println("Subscribed to notifications.");
                } else {
                     Serial.println("Subscription failed."); pClient->disconnect(); 
                }
            } else { Serial.println("Power char not found or no notify."); pClient->disconnect(); }
        } else { Serial.println("Power service not found."); pClient->disconnect(); }
    }

    void onDisconnect(NimBLEClient* cl) {
        connected = false; // Set connected false before updating status
        pPowerMeasurementChar = nullptr;
        updateStatus("Disconnected"); // Update status after connected flag is set
        Serial.println("Disconnected.");
        // pClient = nullptr; // Let pClient be managed by its creator or main loop
        // After disconnection, automatically start a new scan to allow re-connection or new device selection
        startBleScan(); 
    }
};

// notifyCallback (same as before, simplified parsing for brevity)
static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (pRemoteCharacteristic->getUUID().equals(CYCLING_POWER_MEASUREMENT_CHAR_UUID)) {
        uint16_t flags = 0;
        if (length >=2) flags = *((uint16_t*)pData); 
        
        uint8_t offset = 2; 
        int16_t power = 0;
        if (length >= offset + 2) power = (pData[offset+1] << 8) | pData[offset];
        offset += 2;

        uint8_t cadence_value = 0; 
        // Simplified cadence parsing based on common Favero format (power meter specific)
        // Assumes cadence is next if pedal balance is NOT present, or after pedal balance if present.
        if (! (flags & (1 << 0)) ) { // Pedal Power Balance NOT Present
            if (length >= offset + 1) cadence_value = pData[offset];
        } else { // Pedal Power Balance IS Present
            offset +=1; // Skip Pedal Power Balance byte
            if (length >= offset + 1) cadence_value = pData[offset];
        }
        updatePowerAndCadence(power, cadence_value);
    }
}


// --- Main BLE Task ---
void bleManagerTask(void *pvParameters) {
    Serial.println("BLE Manager Task started");
    NimBLEDevice::init("");
    
    startBleScan(); // Initial scan

    for (;;) {
        if (connectFlag) {
            connectFlag = false; // Reset flag immediately

            NimBLEAdvertisedDevice* deviceToConnectCopy = nullptr;
            int currentSelection; // Local variable to hold index from critical section

            portENTER_CRITICAL(&bleDataMutex);
            currentSelection = selectedDeviceIndex;
            // Ensure index is valid before accessing list
            if (currentSelection >= 0 && currentSelection < discoveredDevicesList.size()) {
                deviceToConnectCopy = new NimBLEAdvertisedDevice(*discoveredDevicesList[currentSelection]);
            }
            portEXIT_CRITICAL(&bleDataMutex);

            if (deviceToConnectCopy) {
                updateStatus("Connecting to: " + String(deviceToConnectCopy->getName().c_str()));

                if (pClient == nullptr) { 
                    pClient = NimBLEDevice::createClient();
                    pClient->setClientCallbacks(new ClientCallbacks(), false); // false: do not delete on disconnect
                    pClient->setConnectionParams(12, 12, 0, 51); // Fast params
                } else if (pClient->isConnected()) {
                     Serial.println("connectFlag set but already connected. Ignoring.");
                     delete deviceToConnectCopy;
                     deviceToConnectCopy = nullptr;
                     continue; // Skip to next iteration
                }
                
                bool connectResult = pClient->connect(deviceToConnectCopy); // This is blocking
                
                if (!connectResult) {
                    updateStatus("Connection Failed");
                    // pClient might be in an invalid state, but NimBLE might handle it.
                    // If we were to delete pClient, it should be done carefully.
                    // NimBLEDevice::deleteClient(pClient); pClient = nullptr;
                    startBleScan(); // If connection fails, rescan
                }
                // If connect() is successful, onConnect callback handles status update ("Connected")
                // If it fails, onDisconnect might not be called if connection was never established.
                
                delete deviceToConnectCopy; // Delete the copy we made for connect()
                deviceToConnectCopy = nullptr;
            } else {
                updateStatus("Selected device invalid.");
                startBleScan(); // Rescan if selected device was invalid
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Main task loop delay
    }
}
