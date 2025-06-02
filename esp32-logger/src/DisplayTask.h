#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <Arduino.h>

void displayTask(void *pvParameters);
void initializeDisplay();
void updateDisplayInfo(const char* status, float gps_speed, int sats, float power, uint32_t log_duration_ms, bool sd_ok);


#endif // DISPLAY_TASK_H
