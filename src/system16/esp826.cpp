#include "esp826.h"

#include <Arduino.h>

#include "config/config.h"

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

    while (true) {
        if (Serial1.available()) {
            String response = Serial1.readStringUntil('\n');
            response.trim();

            if (response.length() > 0) {
                Serial.print("<< FROM ESP8266: ");
                Serial.println(response);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
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

    xTaskCreatePinnedToCore(
        esp8266CommunicationTask,
        "ESP8266Comm",
        4096,
        NULL,
        1,
        &esp8266TaskHandle,
        0);

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

void awaitResponse(String response) {
    unsigned long startTime = millis();

    while (millis() - startTime < 10000) {
        if (Serial1.available()) {
            String esp8266responses = Serial1.readStringUntil('\n');
            esp8266responses.trim();

            if (esp8266responses.length() > 0) {
                Serial.println(esp8266responses);

                if (esp8266responses == response) {
                    break;
                }
            }
        }
    }
}