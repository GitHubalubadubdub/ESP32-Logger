#include "PSRAMBuffer.h"
#include <Arduino.h> // For esp_malloc_caps, Serial

// Check if PSRAM is available and enabled in platformio.ini
#if defined(BOARD_HAS_PSRAM) && defined(CONFIG_SPIRAM_USE_MALLOC)
// Great! PSRAM available.
#else
#error "PSRAM not enabled or not available for this board. Check platformio.ini and board capabilities."
#endif

static LogRecordV1* psram_log_buffer = nullptr;
static size_t buffer_capacity = 0;
static volatile size_t write_index = 0;
static volatile size_t read_index = 0;
static volatile size_t current_fill_count = 0;

// For protecting access to buffer indices/count
static portMUX_TYPE psram_buffer_mux = portMUX_INITIALIZER_UNLOCKED;

bool initializePSRAMBuffer() {
    // Try to allocate buffer in PSRAM
    // psram_log_buffer = (LogRecordV1*)malloc(PSRAM_BUFFER_NUM_RECORDS * sizeof(LogRecordV1)); // Standard malloc might go to PSRAM if heap caps allow
    psram_log_buffer = (LogRecordV1*)ps_malloc(PSRAM_BUFFER_NUM_RECORDS * sizeof(LogRecordV1)); // Explicit PSRAM allocation

    if (psram_log_buffer == nullptr) {
        Serial.println("Failed to allocate PSRAM buffer!");
        buffer_capacity = 0;
        return false;
    }

    buffer_capacity = PSRAM_BUFFER_NUM_RECORDS;
    write_index = 0;
    read_index = 0;
    current_fill_count = 0;
    Serial.printf("PSRAM Buffer initialized. Capacity: %zu records (%zu bytes).
", buffer_capacity, buffer_capacity * sizeof(LogRecordV1));
    return true;
}

bool writeToPSRAMBuffer(const LogRecordV1* record) {
    if (psram_log_buffer == nullptr) return false;

    portENTER_CRITICAL(&psram_buffer_mux);
    if (current_fill_count >= buffer_capacity) {
        portEXIT_CRITICAL(&psram_buffer_mux);
        // Buffer is full - handle overflow (e.g., overwrite oldest or signal error)
        // For now, we'll just signal error / drop new data.
        // A more robust implementation might overwrite the oldest data (read_index).
        Serial.println("PSRAM Buffer Full! Data dropped.");
        return false; 
    }

    memcpy(&psram_log_buffer[write_index], record, sizeof(LogRecordV1));
    write_index = (write_index + 1) % buffer_capacity;
    current_fill_count++;
    portEXIT_CRITICAL(&psram_buffer_mux);
    
    return true;
}

size_t readFromPSRAMBuffer(LogRecordV1* destination_buffer, size_t num_records_to_read) {
    if (psram_log_buffer == nullptr || destination_buffer == nullptr || num_records_to_read == 0) {
        return 0;
    }

    size_t records_actually_copied = 0;

    portENTER_CRITICAL(&psram_buffer_mux);
    if (current_fill_count == 0) { // Buffer is empty
        portEXIT_CRITICAL(&psram_buffer_mux);
        return 0;
    }

    size_t num_to_copy = num_records_to_read;
    if (num_to_copy > current_fill_count) { // Don't try to read more than available
        num_to_copy = current_fill_count;
    }

    for (size_t i = 0; i < num_to_copy; ++i) {
        memcpy(&destination_buffer[i], &psram_log_buffer[read_index], sizeof(LogRecordV1));
        read_index = (read_index + 1) % buffer_capacity;
        records_actually_copied++;
    }
    current_fill_count -= records_actually_copied;
    portEXIT_CRITICAL(&psram_buffer_mux);

    return records_actually_copied;
}

size_t getPSRAMBufferCount() {
    portENTER_CRITICAL(&psram_buffer_mux);
    size_t count = current_fill_count;
    portEXIT_CRITICAL(&psram_buffer_mux);
    return count;
}

bool isPSRAMBufferFull() {
    portENTER_CRITICAL(&psram_buffer_mux);
    bool full = (current_fill_count >= buffer_capacity);
    portEXIT_CRITICAL(&psram_buffer_mux);
    return full;
}

bool isPSRAMBufferEmpty() {
    portENTER_CRITICAL(&psram_buffer_mux);
    bool empty = (current_fill_count == 0);
    portEXIT_CRITICAL(&psram_buffer_mux);
    return empty;
}
