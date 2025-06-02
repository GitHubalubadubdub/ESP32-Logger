#include "BleManagerTask.h"
#include "config.h"

#include <NimBLEDevice.h>
#include <vector> // For std::vector

// BLE Service and Characteristic UUIDs
static NimBLEUUID CYCLING_POWER_SERVICE_UUID("0x1818");
static NimBLEUUID CYCLING_POWER_MEASUREMENT_CHAR_UUID("0x2A63");

// --- Shared variables for BLE data and status ---
static uint16_t currentPower = 0;
static uint8_t currentCadence = 0;
static String bleStatus = "Initializing";
static NimBLEClient* pClient = nullptr;
static boolean connected = false;
static NimBLERemoteCharacteristic* pPowerMeasurementChar = nullptr;

// --- Variables for device discovery and selection ---
static std::vector<NimBLEAdvertisedDevice*> discoveredDevicesList;
static int selectedDeviceIndex = 0;
static bool scanRunning = false;
static bool connectFlag = false;

// Mutex for thread-safe access
static portMUX_TYPE bleDataMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Accessor functions ---
uint16_t getPower() {
    uint16_t power_val;
    portENTER_CRITICAL(&bleDataMutex);
    power_val = currentPower;
    portEXIT_CRITICAL(&bleDataMutex);
    return power_val;
}

uint8_t getCadence() {
    uint8_t cadence_val;
    portENTER_CRITICAL(&bleDataMutex);
    cadence_val = currentCadence;
    portEXIT_CRITICAL(&bleDataMutex);
    return cadence_val;
}

String getBLEStatus() {
    String status_val;
    portENTER_CRITICAL(&bleDataMutex);
    status_val = bleStatus;
    portEXIT_CRITICAL(&bleDataMutex);
    return status_val;
}

// --- Helper functions ---
static void updateStatus(String newStatus) {
    portENTER_CRITICAL(&bleDataMutex);
    if (bleStatus != newStatus) {
        bleStatus = newStatus;
        Serial.println("BLE Status: " + newStatus);
    }
    portEXIT_CRITICAL(&bleDataMutex);
}

static void updatePowerAndCadence(uint16_t power_val, uint8_t cadence_val) {
    portENTER_CRITICAL(&bleDataMutex);
    currentPower = power_val;
    currentCadence = cadence_val;
    portEXIT_CRITICAL(&bleDataMutex);
}

static void clearDiscoveredDevices() {
    portENTER_CRITICAL(&bleDataMutex);
    for (auto dev : discoveredDevicesList) {
        delete dev;
    }
    discoveredDevicesList.clear();
    selectedDeviceIndex = 0;
    portEXIT_CRITICAL(&bleDataMutex);
}

// --- Forward declaration for notifyCallback ---
static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

// --- ClientCallbacks class definition ---
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* cl) {
        connected = true;
        pClient = cl;
        updateStatus("Connected");
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
        connected = false;
        pPowerMeasurementChar = nullptr;
        updateStatus("Disconnected");
        Serial.println("Disconnected.");
        startBleScan(); 
    }
};

// --- AdvertisedDeviceCallbacks class definition ---
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
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
                if (discoveredDevicesList.size() < 10) {
                    Serial.print("Found Cycling Power Device: ");
                    Serial.println(advertisedDevice->toString().c_str());
                    discoveredDevicesList.push_back(new NimBLEAdvertisedDevice(*advertisedDevice)); 
                }
            }
            portEXIT_CRITICAL(&bleDataMutex);
        }
    }
};

// --- Public functions for discovery and selection ---
void startBleScan() {
    if (scanRunning) {
        //Serial.println("Scan already in progress."); // Can be noisy
        return;
    }
    if (connected) {
        //Serial.println("Already connected, not starting scan."); // Can be noisy
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
    pScan->setAdvertisedDeviceCallbacks(nullptr, false); 
    // The following line is changed as per instruction.
    // Note: This creates a new callback object each time. If NimBLEScan doesn't delete it, this is a leak.
    // The previous version with ", true" was likely safer for auto-deletion.
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks()); 
    pScan->setActiveScan(true);
    pScan->setInterval(100); 
    pScan->setWindow(99);    
    pScan->start(5, [](NimBLEScanResults results){ 
        scanRunning = false; 
        if (getDiscoveredDeviceCount() == 0) {
            updateStatus("No Devices Found");
        } else {
            updateStatus("Select Device");
        }
        Serial.println("Scan ended.");
    }, false); 
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
        device = discoveredDevicesList[index]; 
    }
    portEXIT_CRITICAL(&bleDataMutex);
    return device;
}

void setSelectedDeviceIndex(int index) {
    portENTER_CRITICAL(&bleDataMutex);
    int count = discoveredDevicesList.size();
    if (count > 0) { 
      selectedDeviceIndex = index;
      if (selectedDeviceIndex < 0) selectedDeviceIndex = 0; 
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
        return true; 
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
        connectFlag = true; 
        updateStatus("Connecting..."); 
        return true;
    }
    updateStatus("No Device Selected");
    return false;
}

// --- notifyCallback definition ---
static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (pRemoteCharacteristic->getUUID().equals(CYCLING_POWER_MEASUREMENT_CHAR_UUID)) {
        uint16_t flags = 0;
        if (length >=2) flags = *((uint16_t*)pData); 
        
        uint8_t offset = 2; 
        int16_t power = 0;
        if (length >= offset + 2) power = (pData[offset+1] << 8) | pData[offset];
        offset += 2;

        uint8_t cadence_value = 0; 
        if (! (flags & (1 << 0)) ) { 
            if (length >= offset + 1) cadence_value = pData[offset];
        } else { 
            offset +=1; 
            if (length >= offset + 1) cadence_value = pData[offset];
        }
        updatePowerAndCadence(power, cadence_value);
    }
}

// --- Main BLE Task ---
void bleManagerTask(void *pvParameters) {
    Serial.println("BLE Manager Task started");
    NimBLEDevice::init("");
    
    startBleScan(); 

    for (;;) {
        if (connectFlag) {
            connectFlag = false; 

            NimBLEAdvertisedDevice* deviceToConnectCopy = nullptr;
            int currentSelection; 

            portENTER_CRITICAL(&bleDataMutex);
            currentSelection = selectedDeviceIndex;
            if (currentSelection >= 0 && currentSelection < discoveredDevicesList.size()) {
                deviceToConnectCopy = new NimBLEAdvertisedDevice(*discoveredDevicesList[currentSelection]);
            }
            portEXIT_CRITICAL(&bleDataMutex);

            if (deviceToConnectCopy) {
                // Status "Connecting to: ..." is more specific, but "Connecting..." is already set by connectToSelectedDevice()
                // For consistency, let's use the more specific one here if the name is available.
                String deviceName = deviceToConnectCopy->getName().c_str();
                if (deviceName.length() == 0) deviceName = deviceToConnectCopy->getAddress().toString().c_str();
                updateStatus("Connecting to: " + deviceName);


                if (pClient == nullptr) { 
                    pClient = NimBLEDevice::createClient();
                    pClient->setClientCallbacks(new ClientCallbacks(), false); 
                    pClient->setConnectionParams(12, 12, 0, 51); 
                } else if (pClient->isConnected()) {
                     Serial.println("connectFlag set but already connected. Ignoring.");
                     delete deviceToConnectCopy;
                     deviceToConnectCopy = nullptr;
                     continue; 
                }
                
                bool connectResult = pClient->connect(deviceToConnectCopy); 
                
                if (!connectResult) {
                    updateStatus("Connection Failed");
                    startBleScan(); 
                }
                delete deviceToConnectCopy; 
                deviceToConnectCopy = nullptr;
            } else {
                updateStatus("Selected device invalid.");
                startBleScan(); 
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}
