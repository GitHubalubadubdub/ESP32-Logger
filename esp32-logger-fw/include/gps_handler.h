#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <FreeRTOS.h> // For void *pvParameters

void gpsTask(void *pvParameters);
// It's generally better to have initialization within the task or called by main.
// For now, we'll keep it simple and do init inside the task.
// If complex one-time setup outside the task is needed later, we can add:
// void initGpsModule();

#endif // GPS_HANDLER_H
