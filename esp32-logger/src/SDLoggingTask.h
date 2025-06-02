#ifndef SD_LOGGING_TASK_H
#define SD_LOGGING_TASK_H

#include <Arduino.h>
#include "DataStructures.h"

void sdLoggingTask(void *pvParameters);
bool initializeSDCard(); // For SD card setup

#endif // SD_LOGGING_TASK_H
