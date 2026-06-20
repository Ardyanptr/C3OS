#include "Ruler.h"

#include "component/service.h"
#include "esp_task_wdt.h"

static DFRobot_VL53L0X tof;
static int metric = 0;
static const char* unitStr[] = {"mm", "cm", "m"};
static bool freezeValue = false;
static bool isRunning = true;

uint16_t convert(uint16_t raw, int m) {
    if (m == 0) return raw;       // mm
    if (m == 1) return raw / 10;  // cm
    return raw / 1000;            // m
}

// --- UI ---
void drawRulerScreen(uint16_t dist) {
    display.clearBuffer();

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(2, 10, "Vl53 Ruler");

    display.drawLine(0, 14, 128, 14);

    display.setFont(u8g2_font_logisoso24_tr);

    char buf[20];
    sprintf(buf, "%d %s", dist, unitStr[metric]);
    display.drawStr(5, 50, buf);

    if (dist == 16383 || dist == 0 || dist == 1638) {
        tof.stop();
        tof.setMode(tof.eContinuous, tof.eHigh);
        tof.start();
    }

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 63, "UP/DOWN: Metric   OK: Freeze");

    display.sendBuffer();
}

void startRulerApp() {
    stopService("Pocket Detection");
    delay(100);

    tof.begin(0x29);
    tof.setMode(tof.eContinuous, tof.eHigh);
    tof.start();

    delay(200);

    unsigned long lastRead = 0;

    // Reset flags (button library keep state if not detached)
    freezeValue = false;
    isRunning = true;

    // --- button actions ---
    btnUp.attachClick([]() {
        metric = (metric + 1) % 3;
    });

    btnDown.attachClick([]() {
        metric = (metric - 1 + 3) % 3;
    });

    btnOK.attachClick([]() {
        freezeValue = !freezeValue;
    });

    btnOK.attachLongPressStart([]() {
        tof.stop();

        delay(500);
        isRunning = false;
        drawMenu();
    });

    // --- Main Loop (LOCK APP) ---
    while (isRunning) {
        appHeartBeat();

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        if (!freezeValue && millis() - lastRead > 70) {
            lastRead = millis();
            uint16_t raw = tof.getDistance();
            uint16_t c = convert(raw, metric);
            drawRulerScreen(c);
        }

        esp_task_wdt_reset();
        delay(5);
    }

    startServiceByName("Pocket Detection");
    drawMenu();
}
