#ifndef PSRAM_BUFFER_H
#define PSRAM_BUFFER_H

#include "DataStructures.h"
#include <stddef.h> // For size_t

// Define buffer size (e.g., for N seconds of data at 200Hz)
// Example: 10 seconds * 200 records/sec = 2000 records
// Size of LogRecordV1 is approx 81 bytes.
// 2000 records * 81 bytes/record = 162000 bytes (approx 158KB)
// Ensure this fits in available PSRAM (2MB for the specified board)
const size_t PSRAM_BUFFER_NUM_RECORDS = 2000; // Adjustable

bool initializePSRAMBuffer();
bool writeToPSRAMBuffer(const LogRecordV1* record);
size_t readFromPSRAMBuffer(LogRecordV1* destination_buffer, size_t num_records_to_read);
size_t getPSRAMBufferCount(); // Get number of records currently in buffer
bool isPSRAMBufferFull();
bool isPSRAMBufferEmpty();

#endif // PSRAM_BUFFER_H
