#include "BenchMyESP.h"
#include <Arduino.h>
#include "esp_task_wdt.h"
#include "system16/esp826.h"
#include "component/rp2040link.h"

// --- C3-TITAN "ULTIMATE" Config ---
#define MAX_ITER 1024          // Professional depth
#define BUCKET_SIZE 16         // 16x16 buckets
#define SSAA 2                 // 2x2 Super-sampling (4x the work per pixel)
#define ZOOM_LEVEL 1200.0      // Deep dive into complex structure
#define CENTER_X -0.743643887037158
#define CENTER_Y 0.131825904206497
#define NUM_RENDER_TASKS 4     // Parallel render tasks (time-sliced on single core)

static bool isRunning = false;
static bool benchMenuActive = false;
static uint64_t totalIterations = 0;

struct RenderResult {
    volatile uint64_t iters;
    volatile bool done;
};

/**
 * @brief C3-Titan Render Engine
 * Features: SSAA 2x2, Double Precision Software Emulation, 1024 Iterations
 */
void renderC3Titan() {
    Serial.println("\n[TITAN] Starting Ultimate Stress Test...");
    Serial.printf("[TITAN] Config: SSAA 2x2, Depth %d, Double-Precision SW Emulation\n", MAX_ITER);
    Serial.printf("[TITAN] Target: %f, %f @ %fx\n", CENTER_X, CENTER_Y, ZOOM_LEVEL);

    uint32_t renderStart = millis();
    totalIterations = 0;

    sendCommand("avr32:bench-start");
    RP2040Link::startBenchmark();
    Serial.println("[TITAN] ESP8266 + RP2040 co-processors benchmark started!");
    
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 8, "TITAN - C3+ESP8266+RP2040");
    display.drawHLine(0, 10, 128);
    display.sendBuffer();

    // Draw all bucket outlines at once
    for (int by = 11; by < 64; by += BUCKET_SIZE) {
        for (int bx = 0; bx < 128; bx += BUCKET_SIZE) {
            display.drawFrame(bx, by, BUCKET_SIZE, BUCKET_SIZE);
        }
    }
    display.sendBuffer();

    // Multi-task parallel rendering — 4 tasks, each renders one horizontal strip
    struct TaskParam { int yStart, yEnd, id; RenderResult* res; };
    RenderResult results[NUM_RENDER_TASKS];
    TaskParam params[NUM_RENDER_TASKS];
    int rowsPerTask = (64 - 11) / NUM_RENDER_TASKS;

    for (int i = 0; i < NUM_RENDER_TASKS; i++) {
        results[i].iters = 0;
        results[i].done = false;
        params[i].id = i;
        params[i].res = results;
        params[i].yStart = 11 + i * rowsPerTask;
        params[i].yEnd = (i == NUM_RENDER_TASKS - 1) ? 64 : params[i].yStart + rowsPerTask;
    }

    auto renderFunc = [](void* p) {
        TaskParam* tp = (TaskParam*)p;
        uint64_t iters = 0;
        for (int y = tp->yStart; y < tp->yEnd; y++) {
            esp_task_wdt_reset();
            appHeartBeat();
            for (int x = 0; x < 128; x++) {
                int subPixelHits = 0;
                for (int sy = 0; sy < SSAA; sy++) {
                    for (int sx = 0; sx < SSAA; sx++) {
                        double py = (y + (double)sy / SSAA - 32.0) / ZOOM_LEVEL + CENTER_Y;
                        double px = (x + (double)sx / SSAA - 64.0) / ZOOM_LEVEL + CENTER_X;
                        double zx = 0, zy = 0;
                        int iter = 0;
                        while (zx * zx + zy * zy < 4.0 && iter < MAX_ITER) {
                            double tmp = zx * zx - zy * zy + px;
                            zy = 2.0 * zx * zy + py;
                            zx = tmp;
                            iter++;
                        }
                        iters += iter;
                        if (iter == MAX_ITER) subPixelHits++;
                    }
                }
                if (subPixelHits > 0) display.drawPixel(x, y);
            }
        }
        tp->res[tp->id].iters = iters;
        tp->res[tp->id].done = true;
        vTaskDelete(NULL);
    };

    for (int i = 0; i < NUM_RENDER_TASKS; i++) {
        xTaskCreate(renderFunc, "Rdr", 4096, &params[i], 1, NULL);
    }

    while (true) {
        bool allDone = true;
        for (int i = 0; i < NUM_RENDER_TASKS; i++) {
            if (!results[i].done) { allDone = false; break; }
        }
        if (allDone) break;
        display.sendBuffer();
        esp_task_wdt_reset();
        delay(5);
    }

    for (int i = 0; i < NUM_RENDER_TASKS; i++) {
        totalIterations += results[i].iters;
    }
    
    uint32_t renderTime = millis() - renderStart;
    Serial.printf("[TITAN] Render Complete in %ums\n", (unsigned int)renderTime);
    Serial.printf("[TITAN] Total C3 Calculations: %llu iterations\n", totalIterations);

    sendCommand("avr32:bench-stop");
    uint64_t esp8266Iters = awaitBenchResult(5000);
    Serial.printf("[TITAN] ESP8266 contributed %llu iterations\n", esp8266Iters);

    uint32_t rp2040Iters = 0;
    uint32_t rp2040PollStart = millis();
    bool rp2040FirstPrint = true;
    while (!RP2040Link::readBenchResult(rp2040Iters)) {
        if (millis() - rp2040PollStart > 10000) {
            if (rp2040FirstPrint) Serial.println("[TITAN] RP2040 timed out (10s)");
            break;
        }
        if (rp2040FirstPrint) { Serial.println("[TITAN] Waiting for RP2040..."); rp2040FirstPrint = false; }
        delay(10);
    }
    Serial.printf("[TITAN] RP2040 contributed %u iterations\n", rp2040Iters);

    uint64_t combinedIterations = totalIterations + esp8266Iters + rp2040Iters;
    float score = ((float)combinedIterations * 1.5f) / renderTime;
    Serial.printf("[TITAN] Combined Score: %.2f TITAN-PTS (%llu total iters)\n\n", score, combinedIterations);

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tr);
    display.drawStr(15, 15, "TITAN CORE SCORE");
    display.drawHLine(0, 18, 128);
    
    display.setFont(u8g2_font_logisoso24_tr);
    char scoreStr[16];
    sprintf(scoreStr, "%d", (int)score);
    display.drawStr(20, 50, scoreStr);
    
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(80, 45, "TITAN-PTS");
    
    char timeStr[48];
    sprintf(timeStr, "Time: %dms | T:%llu", (int)renderTime, combinedIterations);
    display.drawStr(5, 62, timeStr);
    
    display.setFont(u8g2_font_4x6_tr);
    char coopStr[48];
    sprintf(coopStr, "C3:%llu E:%llu R:%u", totalIterations, esp8266Iters, rp2040Iters);
    display.drawStr(5, 55, coopStr);
    
    display.sendBuffer();
    
    isRunning = true;
    btnOK.attachClick([]() { isRunning = false; });
    while(isRunning) {
        btnOK.tick();
        esp_task_wdt_reset();
        delay(10);
    }
}

void runBenchMyESP() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(10, 15, "C3-TITAN RENDER");
    display.drawHLine(0, 18, 128);
    
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(5, 28, "Tri-Core: C3+ESP8266+RP2040");
    display.drawStr(5, 36, "Complexity: TITAN (SSAA 2x2)");
    display.drawStr(5, 44, "Precision: SW Double-Float");
    display.drawStr(5, 52, "Iteration: 1024-Peak");
    
    display.drawStr(10, 62, "[OK] BENCHMARK [LONG] EXIT");
    display.sendBuffer();

    benchMenuActive = true;

    btnOK.attachClick([]() {
        if (!benchMenuActive) return;
        benchMenuActive = false;
        renderC3Titan();
        drawMenu();
    });

    btnOK.attachLongPressStart([]() {
        if (!benchMenuActive) return;
        benchMenuActive = false;
        drawMenu();
    });

    while (benchMenuActive) {
        btnOK.tick();
        esp_task_wdt_reset();
        delay(10);
    }
}
