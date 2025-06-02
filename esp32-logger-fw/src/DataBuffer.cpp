#include "DataBuffer.h"

// Explicit template instantiation for LogRecordV1 if needed by the build system,
// or keep implementation in header for templates. For simplicity with PlatformIO,
// often template implementations are kept in the .h file or an .tpp file included by .h.
// For this step, we'll assume it's okay here, but might need adjustment.

template <typename T>
DataBuffer<T>::DataBuffer(size_t size) : buffer(nullptr), capacity(size), head(0), tail(0), count(0) {
    // xMutex = xSemaphoreCreateMutex();
}

template <typename T>
DataBuffer<T>::~DataBuffer() {
    if (buffer) {
        // In ESP32, PSRAM allocated with ps_malloc should be freed with free()
        // if (psramFound()) {
             free(buffer);
        // } else {
        //    delete[] buffer; // Should not happen if psram allocation was conditional
        // }
    }
    // if (xMutex) {
    //     vSemaphoreDelete(xMutex);
    // }
}

template <typename T>
bool DataBuffer<T>::initialize() {
    #if CONFIG_SPIRAM_SUPPORT
    if (psramFound()) {
        buffer = (T*) ps_malloc(capacity * sizeof(T));
        Serial.printf("PSRAM: Attempted to allocate %d bytes for buffer.
", capacity * sizeof(T));
    } else {
        Serial.println("PSRAM not available, DataBuffer not allocating in PSRAM.");
        // Fallback to heap if desired, or fail
        // buffer = new T[capacity]; // Or handle error: return false;
        return false; // Require PSRAM
    }
    #else
    Serial.println("PSRAM support not enabled in sdkconfig. DataBuffer cannot use PSRAM.");
    // buffer = new T[capacity]; // Fallback to heap if desired
    return false; // Require PSRAM
    #endif

    if (!buffer) {
        Serial.println("Failed to allocate memory for DataBuffer.");
        return false;
    }
    head = 0;
    tail = 0;
    count = 0;
    Serial.printf("DataBuffer initialized with capacity for %d records.
", capacity);
    return true;
}

template <typename T>
bool DataBuffer<T>::write(const T& record) {
    // if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        if (isFull()) {
            // xSemaphoreGive(xMutex);
            // Serial.println("Buffer full, write failed.");
            return false; // Or implement overwrite oldest
        }
        buffer[head] = record;
        head = (head + 1) % capacity;
        count++;
    //     xSemaphoreGive(xMutex);
        return true;
    // }
    // return false;
}

template <typename T>
bool DataBuffer<T>::read(T& record) {
    // if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        if (isEmpty()) {
            // xSemaphoreGive(xMutex);
            return false;
        }
        record = buffer[tail];
        tail = (tail + 1) % capacity;
        count--;
    //     xSemaphoreGive(xMutex);
        return true;
    // }
    // return false;
}

template <typename T>
bool DataBuffer<T>::peek(T& record) const {
    // if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        if (isEmpty()) {
            // xSemaphoreGive(xMutex);
            return false;
        }
        record = buffer[tail];
    //     xSemaphoreGive(xMutex);
        return true;
    // }
    // return false;
}

template <typename T>
bool DataBuffer<T>::isFull() const {
    return count == capacity;
}

template <typename T>
bool DataBuffer<T>::isEmpty() const {
    return count == 0;
}

template <typename T>
size_t DataBuffer<T>::getCount() const {
    return count;
}

template <typename T>
size_t DataBuffer<T>::getCapacity() const {
    return capacity;
}

// Explicit instantiation for LogRecordV1 to ensure linker finds the implementation
// This is needed if the template implementation is in a .cpp file
template class DataBuffer<LogRecordV1>;
