#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H

#include <Arduino.h>
#include "config.h" // For PSRAM_BUFFER_SIZE_RECORDS
#include "types.h"  // For LogRecordV1

template <typename T>
class DataBuffer {
public:
    DataBuffer(size_t size);
    ~DataBuffer();

    bool initialize(); // Allocate PSRAM
    bool write(const T& record);
    bool read(T& record); // Reads and removes the oldest record
    bool peek(T& record) const; // Reads the oldest record without removing
    bool isFull() const;
    bool isEmpty() const;
    size_t getCount() const;
    size_t getCapacity() const;

private:
    T* buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    // Add mutex for thread safety if accessed by multiple writer/reader tasks directly
    // SemaphoreHandle_t xMutex;
};

#endif // DATA_BUFFER_H
