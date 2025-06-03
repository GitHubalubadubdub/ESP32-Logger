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
};

// Data Record Structure (Binary Format)
// Approximately 81 bytes, check actual size with sizeof(LogRecordV1)
typedef struct __attribute__((__packed__)) {
    uint32_t system_timestamp_ms; // System uptime or RTC timestamp

    // GPS Data (18 bytes)
    float gps_latitude;       // Degrees
    float gps_longitude;      // Degrees
    float gps_altitude;       // Meters
    float gps_speed_mps;      // Speed in meters/second
    uint8_t gps_sats;         // Number of satellites
    uint8_t gps_fix_type;     // 0=none, 1=No GPS, 2=2D, 3=3D (from TinyGPS++)

    // Power Meter Data (3 bytes)
    uint16_t power_watts;     // Watts
    uint8_t cadence_rpm;      // Revolutions per minute

    // IMU Data (24 bytes for 6-axis)
    float imu_accel_x_mps2;   // m/s^2
    float imu_accel_y_mps2;   // m/s^2
    float imu_accel_z_mps2;   // m/s^2
    float imu_gyro_x_radps;   // radians/second
    float imu_gyro_y_radps;   // radians/second
    float imu_gyro_z_radps;   // radians/second

    // Future External Analog Channels Data (32 bytes) - Placeholder
    float analog_ch[8];       // Initialize to NAN or zero
} LogRecordV1;

#endif // TYPES_H
