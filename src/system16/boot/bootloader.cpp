#include "bootloader.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

// Static instance for callback access
BootloaderUI *BootloaderUI::instance = nullptr;

const unsigned char BootloaderUI::arrow_right_bits[] = {0x10, 0x20, 0x7f, 0x20, 0x10};

BootloaderUI::BootloaderUI(U8G2 *disp, OneButton *up, OneButton *down, OneButton *ok)
    : display(disp), btnUp(up), btnDown(down), btnOK(ok), selectedIndex(0), 
      cursorCurrentY(12.0f), cursorTargetY(12.0f), menuActive(true), selectionMade(false) {
    instance = this;
}

BootMode BootloaderUI::showMenu(uint16_t autoBootSeconds) {
    startTime = millis();
    autoBootSec = autoBootSeconds;
    menuActive = true;
    selectionMade = false;

    // Attach callbacks
    btnUp->attachClick(onUpClick);
    btnDown->attachClick(onDownClick);
    btnOK->attachClick(onOkClick);

    // Main menu loop
    while (menuActive) {
        esp_task_wdt_reset();
        btnUp->tick();
        btnDown->tick();
        btnOK->tick();

        float elapsed = (float)(millis() - startTime) / 1000.0f;
        if (elapsed >= autoBootSeconds) {
            menuActive = false;
        }

        // Bouncy cursor animation
        cursorCurrentY += (cursorTargetY - cursorCurrentY) * 0.2f;

        updateUI();
        yield();
    }

    return bootModeFromIndex(selectedIndex);
}

void BootloaderUI::drawStaticUI() {
    display->setFontMode(1);
    display->setBitmapMode(1);

    // Main frame
    display->drawRFrame(2, 9, 124, 33, 3);

    // Title
    display->setFont(u8g2_font_4x6_tr);
    display->drawStr(38, 7, "C3 Bootloader");

    // Help text
    display->drawStr(2, 56, "Up/Down: Navigate");
    display->drawStr(2, 63, "OK     : Select and Boot");

    // Menu items
    display->drawStr(13, 17, "C3OS (Stable 1.0)");
    display->drawStr(13, 23, "C3OS Recovery");
    display->drawStr(13, 29, "DFU Mode");
    display->drawStr(13, 35, "RCPM");
}

void BootloaderUI::drawArrow() {
    int yPos = (int)cursorCurrentY;
    display->drawXBM(4, yPos, 7, 5, arrow_right_bits);
}

void BootloaderUI::drawProgress(float progress) {
    int width = (int)(124.0f * progress);
    display->drawBox(2, 49, width, 4);
}

void BootloaderUI::updateUI() {
    display->clearBuffer();
    drawStaticUI();
    drawArrow();
    
    float elapsed = (float)(millis() - startTime) / (autoBootSec * 1000.0f);
    drawProgress(constrain(1.0f - elapsed, 0.0f, 1.0f));
    
    display->sendBuffer();
}

void BootloaderUI::handleUp() {
    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = BL_BOOT_COUNT - 1;
    cursorTargetY = 12.0f + (selectedIndex * 6);
    startTime = millis(); // Reset timeout on activity
}

void BootloaderUI::handleDown() {
    selectedIndex++;
    if (selectedIndex >= BL_BOOT_COUNT) selectedIndex = 0;
    cursorTargetY = 12.0f + (selectedIndex * 6);
    startTime = millis(); // Reset timeout on activity
}

void BootloaderUI::handleOk() {
    selectionMade = true;
    menuActive = false;
}

void BootloaderUI::onUpClick() {
    if (instance) instance->handleUp();
}

void BootloaderUI::onDownClick() {
    if (instance) instance->handleDown();
}

void BootloaderUI::onOkClick() {
    if (instance) instance->handleOk();
}

void BootloaderUI::setSelection(BootMode mode) { selectedIndex = (int)mode; cursorTargetY = 12.0f + (selectedIndex * 6); }

BootMode BootloaderUI::bootModeFromIndex(int idx) {
    switch (idx) {
    case 0:
        return BL_BOOT_NORMAL;
    case 1:
        return BL_BOOT_RECOVERY;
    case 2:
        return BL_BOOT_DFU;
    case 3:
        return BL_BOOT_RCPM;
    default:
        return BL_BOOT_NORMAL;
    }
}
