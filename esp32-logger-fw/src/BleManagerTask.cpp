#include "BleManagerTask.h"
#include "config.h"
void bleManagerTask(void *pvParameters) {
    Serial.println("BLE Manager Task started (dummy)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
