#include "WifiHandlerTask.h"
#include "config.h"
void wifiHandlerTask(void *pvParameters) {
    Serial.println("WiFi Handler Task started (dummy)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
