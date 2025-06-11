#pragma once

#include <stdint.h>

// Define a packed binary structure for logging
// This structure should be versioned to allow for future changes
// For example, LogRecordV1, LogRecordV2, etc.
typedef struct __attribute__((packed)) {
    uint64_t timestamp_us;      // System timestamp in microseconds

    // GPS Data
    float latitude;             // Latitude in decimal degrees
    float longitude;            // Longitude in decimal degrees
    float altitude_m;           // Altitude in meters
    float speed_mps;            // Speed in meters per second
    float heading_deg;          // Heading in degrees
    uint8_t satellites;         // Number of satellites used for fix
    uint8_t fix_quality;        // GPS fix quality (e.g., 0: no fix, 1: GPS, 2: DGPS)

    // BLE Power Meter Data
    float power_w;              // Power in watts
    uint16_t cadence_rpm;       // Cadence in RPM
    float left_right_balance;   // Left/Right power balance (e.g., 0.5 for 50/50)
    // Total/Average Power (TDS) or Bicycle Power Measurement (BDS) specific data
    // Add specific fields as needed based on your BLE service
    // For example:
    // float total_power_w;     // If TDS provides total power
    // float average_power_w;   // If TDS provides average power

    // IMU Data (Placeholders)
    float accel_x_g;            // Accelerometer X-axis in g
    float accel_y_g;            // Accelerometer Y-axis in g
    float accel_z_g;            // Accelerometer Z-axis in g
    float gyro_x_dps;           // Gyroscope X-axis in degrees per second
    float gyro_y_dps;           // Gyroscope Y-axis in degrees per second
    float gyro_z_dps;           // Gyroscope Z-axis in degrees per second

    // You might want to add a version field to the struct itself
    // uint8_t struct_version; // e.g., 1 for LogRecordV1

} LogRecordV1;
