#include "backapp.h"

#include <Arduino.h>
#include <WiFiManager.h>

#include "../emergency/eme_restart.h"
#include "app/Essential/Ruler.h"
#include "app/Essential/Settings.h"
#include "app/Essential/Timer.h"
#include "config/config.h"

SemaphoreHandle_t displayMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t i2cMutex;

DFRobot_VL53L0X pocket_det;

void BA_CPUTemp(void *param) {
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2;
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();

    while (true) {
        updateServiceHeartbeat((const char *)param);

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

void BA_WiFi(void *param) {
    int animFrame = 0;
    WiFi.setAutoReconnect(true);

    if (Settings::instance->get().wifi) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin();
    }

    while (true) {
        updateServiceHeartbeat((const char *)param);

        if (Settings::instance->get().wifi) {
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50))) {
                display.setDrawColor(0);
                display.drawBox(116, 0, 12, 2);
                display.setDrawColor(1);

                if (WiFi.status() == WL_CONNECTED) {
                    display.drawBox(126, 0, 2, 2);
                } else {
                    int xPos = 120 + (animFrame * 2);
                    display.drawBox(xPos, 0, 2, 2);
                    animFrame = (animFrame + 1) % 4;
                }

                if (WiFi.softAPgetStationNum() > 0) {
                    display.drawBox(116, 0, 2, 2);
                }

                xSemaphoreGive(i2cMutex);
            }

        } else {
            if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void BA_LED(void *param) {
    int duty = 0;
    int fadeAmount = 1;

    while (true) {
        updateServiceHeartbeat((const char *)param);

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

void BA_HEAP_ALLOCATOR(void *param) {
    const TickType_t sampleInterval = 2000 / portTICK_PERIOD_MS;
    const TickType_t microSleep = 10 / portTICK_PERIOD_MS;

    const size_t blockSizes[] = {64, 128, 256, 96, 32};
    const size_t numBlocks = sizeof(blockSizes) / sizeof(blockSizes[0]);

    void *blocks[numBlocks];
    memset(blocks, 0, sizeof(blocks));

    for (;;) {
        updateServiceHeartbeat((const char *)param);

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
            void *probe = malloc(1024);
            if (probe) {
                memset(probe, 0, 1024);
                free(probe);
            }
        }

        vTaskDelay(sampleInterval);
    }
}

void BA_EME_RESTART_COMBINATION(void *param) {
    while (true) {
        updateServiceHeartbeat((const char *)param);

        if (digitalRead(BUTTON_OK) == LOW && digitalRead(BUTTON_ACTION) == LOW &&
            digitalRead(BUTTON_UP) == LOW) {
            eme_restart_run();
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void BA_BATTERY(void *param) {
    unsigned long lastRead = 0;
    const unsigned long interval = 3000;

    while (true) {
        updateServiceHeartbeat((const char *)param);

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

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void BA_TIMERTICK(void *param) {
    TickType_t last = xTaskGetTickCount();

    while (true) {
        updateServiceHeartbeat((const char *)param);

        if (timerIsRunning) timerTick();

        vTaskDelayUntil(&last, 1000 / portTICK_PERIOD_MS);
    }
}

void BA_POCKET_DETECTION(void *param) {
    bool inPocket = false;
    bool isUserTooClose = false;

    unsigned long pocketTimer = 0;
    unsigned long tooCloseTimer = 0;

    pocket_det.begin(0x29);
    pocket_det.setMode(pocket_det.eContinuous, pocket_det.eHigh);
    pocket_det.start();

    uint8_t proximity_value = Settings::instance->get().proximityValue;
    if (proximity_value == 0) proximity_value = 100;

    uint8_t contrast = Settings::instance->get().oledContrast;

    while (!serviceStopRequested((const char *)param)) {
        updateServiceHeartbeat((const char *)param);

        uint16_t raw = 0;

        raw = pocket_det.getDistance();

        if (raw > 0 && raw < proximity_value) {
            if (millis() - pocketTimer > 2000) {
                inPocket = true;
            }
        } else {
            pocketTimer = millis();
            inPocket = false;
        }

        if (inPocket) {
            display.setPowerSave(1);
            display.setContrast(contrast - 50);

            yield();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            return;
        } else {
            display.setPowerSave(0);
            display.setContrast(contrast);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void BA_ESP8266_CHECK(void *param) {
    while (true) {
        updateServiceHeartbeat((const char *)param);

        sendCommand("avr32:get-heap");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        sendCommand("avr32:get-uptime");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}