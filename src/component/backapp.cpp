#include "backapp.h"

#include <Arduino.h>

#include "../emergency/eme_restart.h"
#include "config/config.h"

SemaphoreHandle_t displayMutex = xSemaphoreCreateMutex();

void BA_CPUTemp(void* param) {
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2;
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();

    while (true) {
        float result = 0;
        temp_sensor_read_celsius(&result);

        if (result > 60.0) {
            display.setContrast(50);
            WiFi.setTxPower(WIFI_POWER_19_5dBm);
            WiFi.mode(WIFI_STA);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void BA_WiFi(void* param) {
    String ssid, pass;
    loadWiFi(ssid, pass);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.reconnect();
        }

        if (WiFi.status() == WL_CONNECTED) {
            display.drawBox(126, 0, 2, 2);
        }

        if (WiFi.softAPgetStationNum() > 0) {
            display.drawBox(121, 0, 4, 2);
        }

        display.sendBuffer();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void BA_LED(void* param) {
    int duty = 0;
    int fadeAmount = 1;

    while (true) {
        for (int duty = 0; duty <= 255; duty++) {
            ledcWrite(0, duty);
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }

        for (int duty = 255; duty >= 0; duty--) {
            ledcWrite(0, duty);
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }
}

void BA_HEAP_ALLOCATOR(void* param) {
    const TickType_t sampleInterval = 2000 / portTICK_PERIOD_MS;
    const TickType_t microSleep = 10 / portTICK_PERIOD_MS;

    const size_t blockSizes[] = {64, 128, 256, 96, 32};
    const size_t numBlocks = sizeof(blockSizes) / sizeof(blockSizes[0]);

    void* blocks[numBlocks];
    memset(blocks, 0, sizeof(blocks));

    for (;;) {
        size_t freeHeap = ESP.getFreeHeap();
        size_t minFree = ESP.getMinFreeHeap();

        bool doCycle = (minFree < freeHeap * 0.7) || (freeHeap < 20 * 1024);

        if (doCycle) {
            for (size_t i = 0; i < numBlocks; ++i) {
                if (blocks[i]) {
                    free(blocks[i]);
                    blocks[i] = nullptr;
                }

                blocks[i] = malloc(blockSizes[i]);
                if (blocks[i]) {
                    memset(blocks[i], 0xA5, blockSizes[i]);
                }
                vTaskDelay(microSleep);
            }

            for (int i = (int)numBlocks - 1; i >= 0; --i) {
                if (blocks[i]) {
                    memset(blocks[i], 0, blockSizes[i]);
                    free(blocks[i]);
                    blocks[i] = nullptr;
                }
                vTaskDelay(microSleep);
            }
        }

        if (freeHeap > 40 * 1024) {
            void* probe = malloc(1024);
            if (probe) {
                memset(probe, 0, 1024);
                free(probe);
            }
        }

        vTaskDelay(sampleInterval);
    }
}

void BA_EME_RESTART_COMBINATION(void* param) {
    while (true) {
        if (digitalRead(BUTTON_OK) == LOW && digitalRead(BUTTON_ACTION) == LOW && digitalRead(BUTTON_UP) == LOW) {
            eme_restart_run();
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void BA_BATTERY(void* param) {
    unsigned long lastRead = 0;
    const unsigned long interval = 3000;  // update tiap 3 detik

    while (true) {
        unsigned long now = millis();
        if (now - lastRead >= interval) {
            lastRead = now;

            while (Serial1.available()) {
                String line = Serial1.readStringUntil('\n');
                line.trim();
                if (line.startsWith("p_bat:")) {
                    int percent = line.substring(6).toInt();
                    Serial.printf("Battery: %d%%\n", percent);

                    display.setFont(u8g2_font_4x6_tf);
                    display.setDrawColor(1);
                    display.setCursor(128 - 20, 6);
                    display.print(percent);
                    display.print("%");
                    display.sendBuffer();
                }

                if (line.startsWith("p_volt_bat:")) {
                    float voltage = line.substring(11).toFloat();
                    Serial.printf("Voltage: %.2fV\n", voltage);

                    display.setFont(u8g2_font_4x6_tf);
                    display.setDrawColor(1);
                    display.setCursor(90, 6);
                    display.print(voltage);
                    display.print("v");
                    display.sendBuffer();
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);  // kasih sedikit delay biar task lain jalan
    }
}