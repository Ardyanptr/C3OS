#include "HeapMonitor.h"

#include "component/service.h"
#include "system16/app.h"
#include "system16/memmgmt.h"

// Inisialisasi snapshot
HeapInfo heapSnapshot[20];
int heapCount = 0;
bool exitHeapMonitor = false;

void updateHeapInfo() {
    heapCount = 0;

    int svcCount = getServiceCount();
    for (int i = 0; i < svcCount && heapCount < 20; i++) {
        ServiceInfo info = getServiceInfo(i);

        // Isi struct satu per satu agar lebih aman dari error "brace-enclosed initializer"
        heapSnapshot[heapCount].name = info.name;
        heapSnapshot[heapCount].heapUsed = 0;
        heapSnapshot[heapCount].stackUsed = (uint32_t)info.stackUsed;  // Perbaikan typo 'stakcUsed'
        heapSnapshot[heapCount].running = info.running;
        heapCount++;
    }

    for (int i = 0; i < APP_COUNT && heapCount < 20; i++) {
        heapSnapshot[heapCount].name = appTable[i].name;
        heapSnapshot[heapCount].heapUsed = ESP.getFreeHeap();
        heapSnapshot[heapCount].stackUsed = 0;
        heapSnapshot[heapCount].running = true;
        heapCount++;
    }
}

void drawHeapMonitor() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);

    int y = 12;  // OLED mulai dari Y=12 karena font 6x12 menggambar dari baseline
    for (int i = 0; i < heapCount && y < 64; i++) {
        char buf[40];
        // Pastikan nama variabel di heapSnapshot benar (heapUsed & stackUsed)
        snprintf(buf, sizeof(buf), "%-8s H:%u S:%u %s",
                 heapSnapshot[i].name,
                 (uint32_t)heapSnapshot[i].heapUsed,
                 (uint32_t)heapSnapshot[i].stackUsed,
                 heapSnapshot[i].running ? "*" : "");
        display.drawStr(0, y, buf);
        y += 12;
    }
    display.sendBuffer();
}

void runHeapMonitor() {
    exitHeapMonitor = false;  // Reset state

    btnOK.attachClick([]() {
        exitHeapMonitor = true;
    });

    // Gunakan nama service yang terdaftar (misal "HeapMonitor" atau sesuai registrasi)
    while (!exitHeapMonitor) {
        appHeartBeat();
        MEM::monitorLoop("HeapMonitor");

        updateHeapInfo();
        drawHeapMonitor();

        vTaskDelay(pdMS_TO_TICKS(1000));  // Update tiap 1 detik lebih enak dipantau

        btnOK.tick();
        // Cek juga jika sistem meminta app berhenti
        if (serviceStopRequested("HeapMonitor")) break;
    }

    // Kembalikan callback tombol OK ke menu utama atau lepas
    btnOK.attachClick(NULL);
    drawMenu();
}