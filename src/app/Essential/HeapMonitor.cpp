#include "HeapMonitor.h"

#include "component/service.h"
#include "system16/app.h"
#include "system16/memmgmt.h"

HeapInfo heapSnapshot[20];
int heapCount = 0;

bool exitHeapMonitor = false;

void updateHeapInfo() {
    heapCount = 0;

    int svcCount = getServiceCount();
    for (int i = 0; i < svcCount; i++) {
        ServiceInfo info = getServiceInfo(i);
        heapSnapshot[heapCount++] = {
            info.name,
            0,
            info.stakcUsed,
            info.running};
    }

    for (int i = 0; i < APP_COUNT; i++) {
        heapSnapshot[heapCount++] = {
            appTable[i].name,
            ESP.getFreeHeap(),
            0,
            true};
    }
}

void drawHeapMonitor() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);

    int y = 0;
    for (int i = 0; i < heapCount && y < 60; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%-10s H:%5u S:%3u %s", heapSnapshot[i].name, (uint32_t)heapSnapshot[i].heapUsed, heapSnapshot[i].stackUsed, heapSnapshot[i].running ? "*" : "");
        display.drawStr(0, y, buf);
        y += 12;
    }

    display.sendBuffer();
}

void runHeapMonitor() {
    btnOK.attachClick([]() {
        exitHeapMonitor = true;
    });

    while (!serviceStopRequested("HeapMonitor") && !exitHeapMonitor) {
        appHeartBeat();

        MEM::monitorLoop("HeapMonitor");
        updateHeapInfo();
        drawHeapMonitor();
        vTaskDelay(pdMS_TO_TICKS(2000));

        btnOK.tick();
    }

    exitHeapMonitor = false;
    drawMenu();
}