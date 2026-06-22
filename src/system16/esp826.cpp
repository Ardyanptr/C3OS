#include "esp826.h"

#include <Arduino.h>

#include "../emergency/eme_restart.h"
#include "config/config.h"
#include "esp_task_wdt.h"

#define ESP8266_RST 10

TaskHandle_t esp8266TaskHandle = NULL;

void wakeESP8266() {
    pinMode(ESP8266_RST, OUTPUT);
    digitalWrite(ESP8266_RST, LOW);
    delay(50);
    digitalWrite(ESP8266_RST, HIGH);
}

void esp8266CommunicationTask(void* param) {
    Serial.println("[TASK] ESP8266 Communication Task Started");

    static char buffer[128];
    int index = 0;

    while (true) {
        esp_task_wdt_reset();
        while (Serial1.available()) {
            char c = Serial1.read();

            if (c == '\n') {
                buffer[index] = '\0';

                if (strcmp(buffer, "AVR8266:HEARTBEAT!") != 0) {
                    Serial.print("<< FROM ESP8266: ");
                    Serial.println(buffer);
                }

                index = 0;
            } else if (index < sizeof(buffer) - 1) {
                buffer[index++] = c;
            }
        }

        if (digitalRead(BUTTON_OK) == LOW && digitalRead(BUTTON_ACTION) == LOW &&
            digitalRead(BUTTON_UP) == LOW) {
            eme_restart_run();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void blinkESP8266(void* pvParam) {
    while (true) {
        sendCommand("avr:blink");
        vTaskDelay(10 / portTICK_PERIOD_MS);
        sendCommand("avr:blink");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void setupESP8266Communication() {
    Serial.begin(115200);
    wakeESP8266();
    delay(2000);

    Serial.println("\n=== ESP32-C3 UART MASTER ===");
    Serial.println("GPIO6(RX) <- ESP8266 TX(D7)");
    Serial.println("GPIO7(TX) -> ESP8266 RX(D6)");

    Serial1.setRxBufferSize(2048);
    Serial1.begin(115200, SERIAL_8N1, ESP8266_RX, ESP8266_TX);
    Serial1.setTimeout(10);

    xTaskCreatePinnedToCore(esp8266CommunicationTask, "ESP8266Comm", 4096, NULL, 1,
                            &esp8266TaskHandle, 0);
    xTaskCreatePinnedToCore(blinkESP8266, "ESP8266Blink", 4096, NULL, 1, NULL, 0);

    Serial.println("[SETUP] ESP8266 Task Created");
}

void sendCommand(String cmd) {
    Serial1.println(cmd);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

void processSerialInput() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "ping") {
            sendCommand("PING");
            Serial.println(">> Sent: PING");
        } else if (cmd == "led on") {
            sendCommand("LED_ON");
            Serial.println(">> Sent: LED_ON");
        } else if (cmd == "led off") {
            sendCommand("LED_OFF");
            Serial.println(">> Sent: LED_OFF");
        } else if (cmd == "status") {
            sendCommand("GET_STATUS");
            Serial.println(">> Sent: GET_STATUS");
        } else if (cmd.startsWith("echo ")) {
            String text = cmd.substring(5);
            sendCommand("ECHO:" + text);
            Serial.print(">> Sent: ECHO:");
            Serial.println(text);
        } else if (cmd == "debug") {
            debugESP8266Connection();
        } else if (cmd == "help") {
            Serial.println("\n=== COMMANDS ===");
            Serial.println("ping     - Send PING");
            Serial.println("led on   - Turn LED ON");
            Serial.println("led off  - Turn LED OFF");
            Serial.println("status   - Get ESP8266 status");
            Serial.println("echo X   - Send echo command");
            Serial.println("auto     - Toggle auto send");
            Serial.println("debug    - Show connection info");
            Serial.println("help     - Show this help");
        } else if (cmd.length() > 0) {
            sendCommand(cmd);
            Serial.print(">> Sent custom: ");
            Serial.println(cmd);
        }
    }
}

void debugESP8266Connection() {
    Serial.println("\n=== ESP8266 Connection Debug ===");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");

    Serial.println("Testing connection...");
    sendCommand("avr32:test");
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void force_stop_task() {
    sendCommand("avr32:stopforce");
    delay(100);
    sendCommand("avr32:normal");
}

bool awaitResponse(const char* response, uint32_t timeoutMs) {
    uint32_t start = millis();
    static char buffer[128];
    int idx = 0;

    while (millis() - start < timeoutMs) {
        while (Serial1.available()) {
            char c = Serial1.read();

            if (c == '\n') {
                buffer[idx] = '\0';
                if (strcmp(buffer, response) == 0) return true;
                idx = 0;
            } else if (idx < sizeof(buffer) - 1) {
                buffer[idx++] = c;
            }
        }

        vTaskDelay(5);
    }

    return false;
}

uint64_t awaitBenchResult(uint32_t timeoutMs) {
    uint32_t start = millis();
    static char buffer[128];
    int idx = 0;

    while (millis() - start < timeoutMs) {
        while (Serial1.available()) {
            char c = Serial1.read();

            if (c == '\n') {
                buffer[idx] = '\0';
                if (strncmp(buffer, "BENCH:RESULT:", 13) == 0) {
                    return strtoull(buffer + 13, NULL, 10);
                }
                idx = 0;
            } else if (idx < sizeof(buffer) - 1) {
                buffer[idx++] = c;
            }
        }

        vTaskDelay(5);
    }

    return 0;
}