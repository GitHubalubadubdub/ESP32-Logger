#ifndef DATA_ACQUISITION_TASK_H
#define DATA_ACQUISITION_TASK_H

#include <Arduino.h>
#include "DataStructures.h"

void dataAcquisitionTask(void *pvParameters);
void initializeDataAcquisition(); // For setting up timers or initial sensor states

#endif // DATA_ACQUISITION_TASK_H
