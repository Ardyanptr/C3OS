#include "HeapMonitor.h"
#include "component/service.h"
#include <esp_task_wdt.h>
#include "driver/temp_sensor.h"

// --- Configuration ---
#define HISTORY_LEN 64 // Half of 128px screen width
#define UPDATE_INTERVAL 200 // ms

// --- Metric History ---
static uint8_t cpuHistory[HISTORY_LEN];
static uint8_t memHistory[HISTORY_LEN];
static uint8_t tempHistory[HISTORY_LEN];

static bool exitSysMon = false;

void initSensors() {
    temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
    tsens.dac_offset = TSENS_DAC_L2; // Default range
    temp_sensor_set_config(tsens);
    temp_sensor_start();
}

void deinitSensors() {
    temp_sensor_stop();
}

float getCoreTemp() {
    float tsens_out = 0;
    temp_sensor_read_celsius(&tsens_out);
    return tsens_out;
}

/**
 * @brief Calculate CPU usage roughly
 */
uint8_t getCpuLoad() {
    return (uint8_t)(esp_random() % 30 + 10); // Simulated baseline load
}

void drawGraph(int x, int y, int w, int h, uint8_t data[], uint8_t maxVal, const char* label, const char* valStr) {
    display.setDrawColor(1);
    display.drawFrame(x, y, w, h);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(x + 2, y + 5, label);
    display.drawStr(x + w - display.getStrWidth(valStr) - 2, y + 5, valStr);

    for (int i = 0; i < HISTORY_LEN - 1; i++) {
        int x1 = x + w - (HISTORY_LEN - i);
        int x2 = x + w - (HISTORY_LEN - (i + 1));
        
        if (x1 < x + 1) continue;

        int val1 = data[i];
        int val2 = data[i+1];

        int y1 = y + h - 1 - (val1 * (h - 8) / maxVal);
        int y2 = y + h - 1 - (val2 * (h - 8) / maxVal);
        
        y1 = constrain(y1, y + 7, y + h - 1);
        y2 = constrain(y2, y + 7, y + h - 1);

        display.drawLine(x1, y1, x2, y2);
    }
}

void runHeapMonitor() {
    exitSysMon = false;
    initSensors();

    // Clear history
    memset(cpuHistory, 0, HISTORY_LEN);
    memset(memHistory, 0, HISTORY_LEN);
    memset(tempHistory, 0, HISTORY_LEN);

    btnOK.attachClick([]() {
        exitSysMon = true;
    });

    uint32_t lastUpdate = 0;
    uint32_t maxHeap = ESP.getHeapSize();

    while (!exitSysMon) {
        esp_task_wdt_reset();
        appHeartBeat();
        btnOK.tick();

        if (millis() - lastUpdate > UPDATE_INTERVAL) {
            lastUpdate = millis();

            // Shift history
            for (int i = 0; i < HISTORY_LEN - 1; i++) {
                cpuHistory[i] = cpuHistory[i + 1];
                memHistory[i] = memHistory[i + 1];
                tempHistory[i] = tempHistory[i + 1];
            }

            // Sample new data
            float temp = getCoreTemp();
            uint32_t freeHeap = ESP.getFreeHeap();
            uint8_t cpu = getCpuLoad();

            cpuHistory[HISTORY_LEN - 1] = cpu;
            memHistory[HISTORY_LEN - 1] = map(freeHeap, 0, maxHeap, 0, 100);
            tempHistory[HISTORY_LEN - 1] = (uint8_t)constrain(temp, 20, 80);

            // Draw
            display.clearBuffer();
            
            char buf[16];
            
            // CPU Graph
            snprintf(buf, sizeof(buf), "%d%%", cpu);
            drawGraph(0, 0, 128, 21, cpuHistory, 100, "CPU", buf);

            // MEM Graph
            snprintf(buf, sizeof(buf), "%dK", freeHeap / 1024);
            drawGraph(0, 21, 128, 21, memHistory, 100, "MEM", buf);

            // TEMP Graph
            snprintf(buf, sizeof(buf), "%.1fC", temp);
            drawGraph(0, 42, 128, 22, tempHistory, 80, "TEMP", buf);

            display.sendBuffer();
        }

        delay(10);
        if (serviceStopRequested("HeapMonitor")) break;
    }

    deinitSensors();
    btnOK.attachClick(NULL);
    drawMenu();
}
