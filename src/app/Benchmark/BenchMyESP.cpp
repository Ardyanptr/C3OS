#include "BenchMyESP.h"
#include <Arduino.h>
#include "esp_task_wdt.h"
#include "system16/esp826.h"

// --- C3-TITAN "ULTIMATE" Config ---
#define MAX_ITER 1024          // Professional depth
#define BUCKET_SIZE 16         // 16x16 buckets
#define SSAA 2                 // 2x2 Super-sampling (4x the work per pixel)
#define ZOOM_LEVEL 1200.0      // Deep dive into complex structure
#define CENTER_X -0.743643887037158
#define CENTER_Y 0.131825904206497

static bool isRunning = false;
static uint64_t totalIterations = 0;

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
    int bucketCount = 0;

    sendCommand("avr32:bench-start");
    Serial.println("[TITAN] ESP8266 Co-processor benchmark started!");
    
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 8, "TITAN ENGINE - C3+ESP8266");
    display.drawHLine(0, 10, 128);
    display.sendBuffer();

    // Section by Section (Bucket) Rendering
    for (int by = 11; by < 64; by += BUCKET_SIZE) {
        for (int bx = 0; bx < 128; bx += BUCKET_SIZE) {
            uint32_t bucketStart = millis();
            bucketCount++;
            
            Serial.printf("[TITAN] Rendering Bucket %d | Coord: (%d, %d)... ", bucketCount, bx, by);
            
            // Draw "Multi-thread" scanning indicators (4 buckets visible)
            display.setDrawColor(1);
            display.drawFrame(bx, by, BUCKET_SIZE, BUCKET_SIZE);
            if (bx + BUCKET_SIZE < 128) display.drawFrame(bx + BUCKET_SIZE, by, 2, 2);
            if (by + BUCKET_SIZE < 64) display.drawFrame(bx, by + BUCKET_SIZE, 2, 2);
            display.sendBuffer();
            
            uint64_t bucketIters = 0;
            // Process pixels in this bucket with SSAA
            for (int y = by; y < by + BUCKET_SIZE && y < 64; y++) {
                esp_task_wdt_reset();
                appHeartBeat();

                for (int x = bx; x < bx + BUCKET_SIZE && x < 128; x++) {
                    int subPixelHits = 0;
                    
                    // SSAA 2x2 Loop (4 samples per pixel)
                    for (int sy = 0; sy < SSAA; sy++) {
                        for (int sx = 0; sx < SSAA; sx++) {
                            double py = (y + (double)sy/SSAA - 32.0) / ZOOM_LEVEL + CENTER_Y;
                            double px = (x + (double)sx/SSAA - 64.0) / ZOOM_LEVEL + CENTER_X;
                            
                            double zx = 0;
                            double zy = 0;
                            int iter = 0;
                            
                            while (zx * zx + zy * zy < 4.0 && iter < MAX_ITER) {
                                double tmp = zx * zx - zy * zy + px;
                                zy = 2.0 * zx * zy + py;
                                zx = tmp;
                                iter++;
                            }
                            
                            bucketIters += (uint64_t)iter;
                            if (iter == MAX_ITER) subPixelHits++;
                        }
                    }
                    
                    if (subPixelHits > 0) {
                        display.drawPixel(x, y);
                    }
                }
            }
            totalIterations += bucketIters;
            uint32_t bucketTime = millis() - bucketStart;
            Serial.printf("Done! (%ums, %llu iters)\n", (unsigned int)bucketTime, bucketIters);

            // Update screen after each bucket completes
            display.sendBuffer();
        }
    }
    
    uint32_t renderTime = millis() - renderStart;
    Serial.printf("[TITAN] Render Complete in %ums\n", (unsigned int)renderTime);
    Serial.printf("[TITAN] Total C3 Calculations: %llu iterations\n", totalIterations);

    sendCommand("avr32:bench-stop");
    uint64_t esp8266Iters = awaitBenchResult(5000);
    Serial.printf("[TITAN] ESP8266 contributed %llu iterations\n", esp8266Iters);

    uint64_t combinedIterations = totalIterations + esp8266Iters;
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
    sprintf(coopStr, "C3:%llu + ESP:%llu", totalIterations, esp8266Iters);
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
    display.drawStr(5, 28, "Dual-Core: C3 + ESP8266");
    display.drawStr(5, 36, "Complexity: TITAN (SSAA 2x2)");
    display.drawStr(5, 44, "Precision: SW Double-Float");
    display.drawStr(5, 52, "Iteration: 1024-Peak");
    
    display.drawStr(10, 62, "[OK] BENCHMARK [LONG] EXIT");
    display.sendBuffer();

    btnOK.attachClick([]() {
        renderC3Titan();
        drawMenu();
    });

    btnOK.attachLongPressStart([]() {
        drawMenu();
    });
}
