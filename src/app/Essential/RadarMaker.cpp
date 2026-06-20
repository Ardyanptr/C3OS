#include "RadarMaker.h"

#include <esp_task_wdt.h>
#include <math.h>

#include "component/service.h"

#define CENTER_X 64
#define CENTER_Y 32
#define MAX_RADIUS 28
#define TOTAL_STEPS 72
#define SCAN_INTERVAL 80

#define ZONE_CLOSE 500
#define ZONE_MID 1200
#define ZONE_FAR 2000

enum RadarState {
    R_TUTORIAL,
    R_SCANNING,
    R_COMPLETE
};
static RadarState currentRState = R_TUTORIAL;

static DFRobot_VL53L0X radarSensor;
static uint16_t radarData[TOTAL_STEPS];
static int currentAngleIndex = 0;
static bool radarIsRunning = true;

static uint8_t getZonePattern(uint16_t dist) {
    if (dist == 0 || dist >= 16383 || dist == 1638) return 0;
    if (dist < ZONE_CLOSE) return 2;
    if (dist < ZONE_MID) return 1;
    return 1;
}

static const char* getZoneLabel(uint16_t dist) {
    if (dist == 0 || dist >= 16383) return "NO SIGNAL";
    if (dist < ZONE_CLOSE) return "CLOSE!";
    if (dist < ZONE_MID) return "NEAR";
    if (dist < ZONE_FAR) return "FAR";
    return "EMPTY";
}

static void getRadarCartesian(uint16_t dist, int angleDeg, int16_t& x, int16_t& y) {
    if (dist == 0 || dist >= 16383 || dist == 1638) {
        x = CENTER_X;
        y = CENTER_Y;
        return;
    }

    if (dist > 2500) dist = 2500;

    float normalized = dist / 2500.0f;
    float scaled = (1 - (1 - normalized) * (1 - normalized)) * MAX_RADIUS;

    float rad = angleDeg * DEG_TO_RAD;
    x = CENTER_X + (int16_t)(scaled * cos(rad));
    y = CENTER_Y + (int16_t)(scaled * sin(rad));
}

static void drawRadarTutorial() {
    display.clearBuffer();

    display.setFont(u8g2_font_6x12_tr);
    display.drawStr(28, 10, "ROOM SCANNER");

    static float pulse = 0;
    pulse += 0.1;
    int r = 4 + (int)(sin(pulse) * 2);
    display.drawCircle(64, 22, r);
    display.drawLine(64, 22, 64 + r, 22);

    display.setFont(u8g2_font_4x6_tr);

    display.drawDisc(10, 35, 2);
    display.drawStr(16, 38, "Stand in center");

    display.drawDisc(10, 46, 2);
    display.drawStr(16, 49, "Rotate 360 slowly");

    display.drawFrame(8, 55, 112, 8);
    display.setDrawColor(0);
    display.drawBox(9, 56, 110, 6);
    display.setDrawColor(1);
    display.drawStr(35, 62, "PRESS OK TO START");

    display.sendBuffer();
}

static void drawRadarScanning() {
    display.clearBuffer();

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 6, "SCANNING...");

    uint16_t currentDist = radarData[currentAngleIndex > 0 ? currentAngleIndex - 1 : 0];
    display.setFont(u8g2_font_5x8_tf);
    display.setCursor(85, 8);
    if (currentDist > 0 && currentDist < 16383) {
        display.print(currentDist / 10);
        display.print("cm");
    } else {
        display.print("---");
    }

    const char* zone = getZoneLabel(currentDist);
    display.setFont(u8g2_font_4x6_tr);
    display.setCursor(85, 16);
    display.print(zone);

    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS, U8G2_DRAW_ALL);
    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS * 0.6, U8G2_DRAW_ALL);
    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS * 0.3, U8G2_DRAW_ALL);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(CENTER_X + MAX_RADIUS + 2, CENTER_Y + 3, "2.5m");
    display.drawStr(CENTER_X + MAX_RADIUS * 0.6 + 2, CENTER_Y + 3, "1.5m");

    for (int i = 0; i < currentAngleIndex; i++) {
        int16_t px, py;
        getRadarCartesian(radarData[i], i * 5, px, py);

        if (radarData[i] > 50 && radarData[i] < 16383) {
            if (radarData[i] < ZONE_CLOSE) {
                display.drawBox(px - 1, py - 1, 3, 3);
            } else if (radarData[i] < ZONE_MID) {
                display.drawDisc(px, py, 1);
            } else {
                display.drawPixel(px, py);
            }
        }
    }

    int16_t lx, ly;
    getRadarCartesian(2500, currentAngleIndex * 5, lx, ly);
    display.drawLine(CENTER_X, CENTER_Y, lx, ly);

    int progressSteps = (currentAngleIndex * 24) / TOTAL_STEPS;
    for (int i = 0; i < progressSteps; i++) {
        float a = (i / 24.0) * 6.28 - 1.57;
        int px = CENTER_X + cos(a) * (MAX_RADIUS + 4);
        int py = CENTER_Y + sin(a) * (MAX_RADIUS + 4);
        display.drawPixel(px, py);
    }

    display.setCursor(55, 62);
    display.print((currentAngleIndex * 100) / TOTAL_STEPS);
    display.print("%");

    display.sendBuffer();
}

static void drawRadarResult() {
    display.clearBuffer();

    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS, U8G2_DRAW_ALL);
    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS * 0.6, U8G2_DRAW_ALL);
    display.drawCircle(CENTER_X, CENTER_Y, MAX_RADIUS * 0.3, U8G2_DRAW_ALL);

    display.drawLine(CENTER_X - 4, CENTER_Y, CENTER_X + 4, CENTER_Y);
    display.drawLine(CENTER_X, CENTER_Y - 4, CENTER_X, CENTER_Y + 4);
    display.drawDisc(CENTER_X, CENTER_Y, 1);

    int validPoints = 0;
    int16_t firstX = 0, firstY = 0;
    int16_t lastX = 0, lastY = 0;
    bool hasFirst = false;

    for (int i = 0; i < TOTAL_STEPS; i++) {
        int16_t x, y;
        getRadarCartesian(radarData[i], i * 5, x, y);

        if (radarData[i] < 50 || radarData[i] >= 16383) continue;

        if (radarData[i] < ZONE_CLOSE) {
            display.drawBox(x - 1, y - 1, 3, 3);
        } else {
            display.drawDisc(x, y, 1);
        }

        if (hasFirst) {
            int distBetween = sqrt((x - lastX) * (x - lastX) + (y - lastY) * (y - lastY));
            if (distBetween < 15) {
                display.drawLine(lastX, lastY, x, y);
            }
        } else {
            firstX = x;
            firstY = y;
            hasFirst = true;
        }

        lastX = x;
        lastY = y;
        validPoints++;
    }

    if (hasFirst && validPoints > 10) {
        int closeDist = sqrt((firstX - lastX) * (firstX - lastX) + (firstY - lastY) * (firstY - lastY));
        if (closeDist < 20) {
            display.drawLine(lastX, lastY, firstX, firstY);
        }
    }

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 8, "MAP COMPLETE");

    display.drawBox(2, 56, 4, 4);
    display.drawStr(8, 62, "<50cm");
    display.drawDisc(50, 58, 1);
    display.drawStr(54, 62, "1.2m");
    display.drawPixel(90, 58);
    display.drawStr(94, 62, "2m+");

    display.setCursor(85, 8);
    display.print(validPoints);
    display.print("pts");

    display.sendBuffer();
}

void startRadarApp() {
    stopService("Pocket Detection");
    delay(100);

    radarSensor.begin(0x29);
    radarSensor.setMode(radarSensor.eContinuous, radarSensor.eHigh);
    radarSensor.start();

    currentRState = R_TUTORIAL;
    radarIsRunning = true;
    currentAngleIndex = 0;
    memset(radarData, 0, sizeof(radarData));

    btnOK.attachClick([]() {
        if (currentRState == R_TUTORIAL || currentRState == R_COMPLETE) {
            currentAngleIndex = 0;
            memset(radarData, 0, sizeof(radarData));
            currentRState = R_SCANNING;
        }
    });

    btnDown.attachClick([]() { radarIsRunning = false; });
    btnUp.attachClick([]() { radarIsRunning = false; });

    unsigned long lastScanTime = 0;
    while (radarIsRunning) {
        btnOK.tick();
        btnDown.tick();
        btnUp.tick();

        switch (currentRState) {
            case R_TUTORIAL:
                drawRadarTutorial();
                break;
            case R_SCANNING:
                if (millis() - lastScanTime >= SCAN_INTERVAL) {
                    lastScanTime = millis();
                    uint16_t dist = radarSensor.getDistance();

                    if (dist == 16383 || dist == 0 || dist == 1638) {
                        radarSensor.stop();
                        radarSensor.setMode(radarSensor.eContinuous, radarSensor.eHigh);
                        radarSensor.start();
                        dist = 0;
                    }

                    radarData[currentAngleIndex++] = dist;
                    drawRadarScanning();

                    if (currentAngleIndex >= TOTAL_STEPS) {
                        currentRState = R_COMPLETE;
                    }
                }
                break;
            case R_COMPLETE:
                drawRadarResult();
                break;
        }

        if (currentRState == R_SCANNING) {
            static unsigned long lastWdt = 0;
            if (millis() - lastWdt > 1000) {
                esp_task_wdt_reset();
                lastWdt = millis();
            }
        }

        delay(5);
    }

    radarSensor.stop();
    delay(100);
    startServiceByName("Pocket Detection");
    drawMenu();
}