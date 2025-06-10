#include "terminal_manager.h"
#include "shared_state.h"
#include <Arduino.h> // For Serial object
#include <string.h>  // For strcmp, strtok_r

// Buffer to store incoming serial commands
static char command_buffer[128];
static uint8_t buffer_pos = 0;

void print_help() {
    Serial.println("Available commands:");
    Serial.println("  help (or h)          - Prints this help message.");
    Serial.println("  gps_debug <on|off>   - Enables/disables GPS debug stream.");
    Serial.println("  ble_debug <on|off>   - Enables/disables BLE debug stream.");
    Serial.println("  other_debug <on|off> - Enables/disables other generic debug streams.");
    Serial.println("  ble_stream <on|off>  - Enables/disables verbose BLE activity stream.");
}

void process_command(char *command_line) {
    char *command;
    char *argument;
    char *saveptr; // For strtok_r

    // Get the command (first token)
    command = strtok_r(command_line, " ", &saveptr);

    if (command == NULL) {
        return; // Empty line
    }

    // Get the argument (second token, if any)
    argument = strtok_r(NULL, " ", &saveptr);

    if (strcmp(command, "help") == 0 || strcmp(command, "h") == 0) {
        print_help();
    } else if (strcmp(command, "gps_debug") == 0) {
        if (argument != NULL) {
            if (xSemaphoreTake(g_debugSettingsMutex, portMAX_DELAY) == pdTRUE) {
                if (strcmp(argument, "on") == 0) {
                    g_debugSettings.gpsDebugStreamOn = true;
                    Serial.println("GPS debug stream enabled.");
                } else if (strcmp(argument, "off") == 0) {
                    g_debugSettings.gpsDebugStreamOn = false;
                    Serial.println("GPS debug stream disabled.");
                } else {
                    Serial.println("Invalid argument for gps_debug. Use 'on' or 'off'.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
        } else {
            Serial.println("Missing argument for gps_debug. Use 'on' or 'off'.");
        }
    } else if (strcmp(command, "ble_debug") == 0) {
        if (argument != NULL) {
            if (xSemaphoreTake(g_debugSettingsMutex, portMAX_DELAY) == pdTRUE) {
                if (strcmp(argument, "on") == 0) {
                    g_debugSettings.bleDebugStreamOn = true;
                    Serial.println("BLE debug stream enabled.");
                } else if (strcmp(argument, "off") == 0) {
                    g_debugSettings.bleDebugStreamOn = false;
                    Serial.println("BLE debug stream disabled.");
                } else {
                    Serial.println("Invalid argument for ble_debug. Use 'on' or 'off'.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
        } else {
            Serial.println("Missing argument for ble_debug. Use 'on' or 'off'.");
        }
    } else if (strcmp(command, "other_debug") == 0) {
        if (argument != NULL) {
            if (xSemaphoreTake(g_debugSettingsMutex, portMAX_DELAY) == pdTRUE) {
                if (strcmp(argument, "on") == 0) {
                    g_debugSettings.otherDebugStreamOn = true;
                    Serial.println("Other generic debug streams enabled.");
                } else if (strcmp(argument, "off") == 0) {
                    g_debugSettings.otherDebugStreamOn = false;
                    Serial.println("Other generic debug streams disabled.");
                } else {
                    Serial.println("Invalid argument for other_debug. Use 'on' or 'off'.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
        } else {
            Serial.println("Missing argument for other_debug. Use 'on' or 'off'.");
        }
    } else if (strcmp(command, "ble_stream") == 0) {
        if (argument != NULL) {
            if (xSemaphoreTake(g_debugSettingsMutex, portMAX_DELAY) == pdTRUE) {
                if (strcmp(argument, "on") == 0) {
                    g_debugSettings.bleActivityStreamOn = true;
                    Serial.println("BLE activity stream enabled.");
                } else if (strcmp(argument, "off") == 0) {
                    g_debugSettings.bleActivityStreamOn = false;
                    Serial.println("BLE activity stream disabled.");
                } else {
                    Serial.println("Invalid argument for ble_stream. Use 'on' or 'off'.");
                }
                xSemaphoreGive(g_debugSettingsMutex);
            }
        } else {
            Serial.println("Missing argument for ble_stream. Use 'on' or 'off'.");
        }
    } else {
        Serial.print("Unknown command: ");
        Serial.println(command);
        print_help();
    }
}

void terminal_task(void *pvParameters) {
    (void)pvParameters; // Unused parameter

    // Initialize buffer
    memset(command_buffer, 0, sizeof(command_buffer));
    buffer_pos = 0;

    while (1) {
        if (Serial.available() > 0) {
            char incoming_char = Serial.read();

            if (incoming_char == '\n' || incoming_char == '\r') {
                // End of command
                if (buffer_pos > 0) { // Check if there's a command to process
                    command_buffer[buffer_pos] = '\0'; // Null-terminate the string
                    Serial.println(); // Add this line for a clean break
                    Serial.print("Received command: "); // Echo command
                    Serial.println(command_buffer);
                    process_command(command_buffer);
                    buffer_pos = 0; // Reset buffer position for next command
                    memset(command_buffer, 0, sizeof(command_buffer)); // Clear buffer
                }
            } else if (buffer_pos < (sizeof(command_buffer) - 1)) {
                // Add character to buffer if it's not a newline/return and buffer is not full
                command_buffer[buffer_pos++] = incoming_char;
            } else {
                // Buffer full, possible overflow or very long command
                Serial.println("Error: Command buffer overflow. Command too long.");
                buffer_pos = 0; // Reset buffer
                memset(command_buffer, 0, sizeof(command_buffer)); // Clear buffer
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield for other tasks
    }
}
