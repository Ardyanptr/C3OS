#include "SystemRescue.h"
#include <LittleFS.h>
#include <Wire.h>
#include <esp_task_wdt.h>

U8G2* SystemRescue::_display = nullptr;
OneButton* SystemRescue::_up = nullptr;
OneButton* SystemRescue::_down = nullptr;
OneButton* SystemRescue::_ok = nullptr;
int SystemRescue::_menuIdx = 0;
bool SystemRescue::_isRunning = true;

const char* rescueLabels[] = {
    "1. Fix My Device",
    "2. Clean Junk Files",
    "3. Factory Reset",
    "4. Test Buttons/Screen",
    "5. Restart Normally"
};

void SystemRescue::run(U8G2* display, OneButton* up, OneButton* down, OneButton* ok) {
    _display = display;
    _up = up;
    _down = down;
    _ok = ok;
    _isRunning = true;
    _menuIdx = 0;

    _up->attachClick([](){ _menuIdx = (_menuIdx - 1 + 5) % 5; });
    _down->attachClick([](){ _menuIdx = (_menuIdx + 1) % 5; });
    _ok->attachClick([](){ executeTool((RescueTool)_menuIdx); });

    while(_isRunning) {
        esp_task_wdt_reset();
        _up->tick();
        _down->tick();
        _ok->tick();
        showToolMenu();
        delay(10);
    }
}

void SystemRescue::showToolMenu() {
    _display->clearBuffer();
    
    // Header
    _display->setDrawColor(1);
    _display->drawBox(0, 0, 128, 11);
    _display->setDrawColor(0);
    _display->setFont(u8g2_font_6x10_tr);
    _display->drawStr(15, 9, "RESCUE & REPAIR");
    _display->setDrawColor(1);

    // Menu List
    for(int i=0; i<5; i++) {
        int y = 22 + (i*9);
        if(i == _menuIdx) {
            _display->drawBox(0, y-7, 128, 9);
            _display->setDrawColor(0);
        } else {
            _display->setDrawColor(1);
        }
        _display->setFont(u8g2_font_4x6_tr);
        _display->drawStr(4, y, rescueLabels[i]);
    }
    _display->setDrawColor(1);
    _display->sendBuffer();
}

void SystemRescue::executeTool(RescueTool tool) {
    switch(tool) {
        case TOOL_AUTO_FIX:     doAutoFix(); break;
        case TOOL_WIPE_CACHE:   doWipeCache(); break;
        case TOOL_FACTORY_RESET: doFactoryReset(); break;
        case TOOL_HW_TEST:      doHardwareTest(); break;
        case TOOL_EXIT:         _isRunning = false; break;
    }
}

void SystemRescue::doAutoFix() {
    _display->clearBuffer();
    _display->drawStr(10, 20, "Scanning FS...");
    _display->sendBuffer();
    
    bool fsOk = LittleFS.begin(false);
    delay(1000);
    
    if(!fsOk) {
        _display->drawStr(10, 30, "FS Error! Formatting...");
        _display->sendBuffer();
        LittleFS.format();
    }
    
    _display->drawStr(10, 40, "Checking I2C...");
    _display->sendBuffer();
    Wire.beginTransmission(0x50); // EEPROM
    if(Wire.endTransmission() != 0) {
        _display->drawStr(10, 50, "EEPROM Not Found!");
    } else {
        _display->drawStr(10, 50, "EEPROM OK!");
    }
    
    _display->sendBuffer();
    delay(3000);
}

void SystemRescue::doWipeCache() {
    _display->clearBuffer();
    _display->drawStr(10, 30, "Cleaning Cache...");
    _display->sendBuffer();
    
    // Simulate cache wipe (delete temp files if they exist)
    LittleFS.remove("/tmp");
    LittleFS.remove("/cache");
    
    delay(1500);
    _display->drawStr(10, 45, "Done! System Cleaned.");
    _display->sendBuffer();
    delay(2000);
}

void SystemRescue::doFactoryReset() {
    _display->clearBuffer();
    _display->drawStr(5, 20, "ERASE EVERYTHING?");
    _display->drawStr(5, 30, "OK: Confirm");
    _display->drawStr(5, 40, "Up: Cancel");
    _display->sendBuffer();
    
    while(true) {
        esp_task_wdt_reset();
        if(digitalRead(0) == LOW) return; // UP to cancel
        if(digitalRead(2) == LOW) {       // OK to confirm
            _display->clearBuffer();
            _display->drawStr(10, 30, "WIPING FLASH...");
            _display->sendBuffer();
            LittleFS.format();
            delay(2000);
            esp_restart();
        }
        delay(10);
    }
}

void SystemRescue::doHardwareTest() {
    bool testing = true;
    while(testing) {
        esp_task_wdt_reset();
        _display->clearBuffer();
        _display->drawStr(5, 10, "HW Test Mode");
        _display->drawStr(5, 25, digitalRead(0) == LOW ? "UP: PRESSED" : "UP: RELEASED");
        _display->drawStr(5, 35, digitalRead(1) == LOW ? "DN: PRESSED" : "DN: RELEASED");
        _display->drawStr(5, 45, digitalRead(2) == LOW ? "OK: PRESSED" : "OK: RELEASED");
        _display->drawStr(5, 60, "Hold OK to Exit");
        _display->sendBuffer();
        
        static uint32_t holdStart = 0;
        if(digitalRead(2) == LOW) {
            if(holdStart == 0) holdStart = millis();
            if(millis() - holdStart > 2000) testing = false;
        } else {
            holdStart = 0;
        }
        delay(20);
    }
}
