#pragma once

#include <stdint.h>

// Basic structure for BLE Power Meter Data
// This can be expanded later based on actual BLE service data
typedef struct {
    float power_w;              // Power in watts
    uint16_t cadence_rpm;       // Cadence in RPM
    float left_right_balance;   // Left/Right power balance (e.g., 0.5 for 50/50)
    // Add other relevant fields from your BLE power service if known
    // For example:
    // uint32_t total_crank_revolutions;
    // uint16_t last_crank_event_time_s; // Or whatever units
} BlePowerMetricsData_t;

// Note: The prompt included an #endif for a traditional include guard.
// #pragma once serves the same purpose and is generally preferred when supported.
// If a traditional guard like #ifndef BLE_DATA_TYPES_H ... #define BLE_DATA_TYPES_H ... #endif
// is strictly required, it can be added. For now, #pragma once is used as per common practice.
