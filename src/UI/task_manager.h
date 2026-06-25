#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "component/process_manager.h"
#include "component/ui_.h"

extern C3OSDisplay display;

static int tm_selected = 0;
static bool tm_exit = false;

void showTaskManager() {
    tm_selected = 0;
    tm_exit = false;

    // Detach menu buttons
    btnUp.attachClick([]() {
        tm_selected--;
    });
    btnDown.attachClick([]() {
        tm_selected++;
    });
    btnOK.attachClick([]() {
        auto& pm = ProcessManager::instance();
        auto& procs = pm.getProcesses();
        if (tm_selected >= 0 && tm_selected < procs.size()) {
            pm.setForeground(procs[tm_selected]->id);
        }
    });
    btnAction.attachClick([]() {
        auto& pm = ProcessManager::instance();
        auto& procs = pm.getProcesses();
        if (tm_selected >= 0 && tm_selected < procs.size()) {
            pm.killProcess(procs[tm_selected]->id);
        }
    });
    btnAction.attachLongPressStart([]() {
        tm_exit = true;
    });

    while (!tm_exit) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        auto& pm = ProcessManager::instance();
        auto& procs = pm.getProcesses();
        if (procs.empty()) {
            display.clearBuffer();
            display.setFont(u8g2_font_6x10_tr);
            display.drawStr(10, 32, "No Background Apps");
            display.sendBuffer();
            if (millis() % 2000 > 1800) tm_exit = true; 
            vTaskDelay(10);
            continue;
        }

        if (tm_selected < 0) tm_selected = procs.size() - 1;
        if (tm_selected >= procs.size()) tm_selected = 0;

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(0, 10, "Task Manager");
        display.drawHLine(0, 12, 128);

        for (int i = 0; i < procs.size(); i++) {
            int y = 25 + i * 12;
            if (i == tm_selected) {
                display.drawBox(0, y - 9, 128, 11);
                display.setDrawColor(0);
            } else {
                display.setDrawColor(1);
            }
            
            display.drawStr(2, y, procs[i]->name);
            
            // Show RAM (stack water mark as a proxy)
            char ramBuf[16];
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(procs[i]->handle);
            sprintf(ramBuf, "%u", watermark);
            display.drawStr(100, y, ramBuf);
            
            display.setDrawColor(1);
        }

        display.drawStr(0, 64, "OK: Switch  ACT: Kill");
        display.sendBuffer();
        vTaskDelay(10);
    }

    // Re-init menu buttons will happen in drawMenu or wherever we return
}
