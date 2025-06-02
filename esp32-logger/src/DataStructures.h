#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>

// Define NAN if not already defined (e.g., by including <cmath> or <math.h>)
#ifndef NAN
#define NAN (0.0f/0.0f)
#endif

typedef struct __attribute__((packed)) {
    uint32_t system_timestamp_ms;   // System uptime or RTC timestamp

    // GPS Data (18 bytes)
    float    gps_latitude;          // Degrees
    float    gps_longitude;         // Degrees
    float    gps_altitude;          // Meters
    float    gps_speed_mps;         // Speed in meters/second
    uint8_t  gps_sats;              // Number of satellites
    uint8_t  gps_fix_type;          // 0=none, 1=No Fix, 2=2D, 3=3D (use 0,2,3 as per issue)

    // Power Meter Data (3 bytes)
    uint16_t power_watts;           // Watts
    uint8_t  cadence_rpm;           // RPM

    // IMU Data (24 bytes for 6-axis)
    float    imu_accel_x_mps2;      // m/s^2
    float    imu_accel_y_mps2;      // m/s^2
    float    imu_accel_z_mps2;      // m/s^2
    float    imu_gyro_x_radps;      // radians/second
    float    imu_gyro_y_radps;      // radians/second
    float    imu_gyro_z_radps;      // radians/second

    // Future External Analog Channels Data (32 bytes) - Placeholder
    float    analog_ch[8];          // Initialize to NAN or zero
} LogRecordV1;

#endif // DATA_STRUCTURES_H
