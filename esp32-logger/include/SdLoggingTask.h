#ifndef SD_LOGGING_TASK_H
#define SD_LOGGING_TASK_H

#include <Arduino.h>
#include "types.h" // For LogRecordV1

void sdLoggingTask(void *pvParameters);

bool initializeSDCard();
void createNewLogFile();
void closeLogFile();

#endif // SD_LOGGING_TASK_H
