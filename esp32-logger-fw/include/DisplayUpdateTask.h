#ifndef DISPLAY_UPDATE_TASK_H
#define DISPLAY_UPDATE_TASK_H

#include <Arduino.h>

void displayUpdateTask(void *pvParameters);

bool initializeDisplay();
void updateDisplay(); // Function to redraw screen contents

#endif // DISPLAY_UPDATE_TASK_H
