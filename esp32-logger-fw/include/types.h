#ifndef TYPES_H
#define TYPES_H

#include <stdint.h> // For fixed-width integer types
#include <math.h>   // For NAN

// BLE Connection State Enum
enum BleConnectionState {
    BLE_IDLE,
    BLE_SCANNING,
    BLE_CONNECTING,
    BLE_CONNECTED,
    BLE_DISCONNECTED
};

struct PowerCadenceData {
    uint16_t power = 0;
    uint8_t cadence = 0;
    bool newData = false;

    // New members for BLE status
    BleConnectionState bleState = BLE_IDLE;
    char connectedDeviceName[50] = {0}; // Max length for device name

    // New fields for L/R Power Balance
    float left_pedal_balance_percent = 50.0f; // Default to 50%
    bool pedal_balance_available = false;

    // New fields for TDS/BDS Angles
    uint16_t top_dead_spot_angle = 0;     // Value in degrees
    bool top_dead_spot_available = false;
    uint16_t bottom_dead_spot_angle = 0;  // Value in degrees
    bool bottom_dead_spot_available = false;
    bool dead_spot_angles_supported = false; // Is the feature supported by connected PM
};

// LogRecordV1 is now defined in src/LogDataStructure.h
// to avoid redefinition errors.

#endif // TYPES_H
