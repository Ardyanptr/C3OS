#include "KeyboardEmu.h"

#include "component/vkeyboard.h"
#include "component/rp2040link.h"

#include <Wire.h>

// I dont fucking know what this is

#define RP2040_I2C_ADDR 0x42

static bool useRP2040 = true;
static int sel = 0;
static bool running = true;

static void rp2040TypeText(const char* text) {
    int slen = strlen(text);
    int pos = 0;

    while (pos < slen) {
        int chunk = min(slen - pos, 30);

        uint8_t buf[32];
        buf[0] = 0x20;
        buf[1] = chunk;

        memcpy(buf + 2, text + pos, chunk);

        RP2040Link::rp2040Send(buf, 2 + chunk);

        pos += chunk;
        delay(20);
    }
}

static void rp2040KeyCombo(uint8_t mod, char key) {
    uint8_t buf[] = { 0x21, mod, (uint8_t)key };
    RP2040Link::rp2040Send(buf, 3);
}

static void rp2040Enter() {
    uint8_t cmd[] = { 0x22 };
    RP2040Link::rp2040Send(cmd, 1);
}

enum Preset {
    PRESET_WINR_CMD,
    PRESET_WINR_NOTEPAD,
    PRESET_WINR_CALC,
    PRESET_RICKROLL,
    PRESET_LOCK_PC,
    PRESET_TASK_MANAGER,
    PRESET_TEXT_HELLO,
    PRESET_TYPE_CUSTOM,
    PRESET_COUNT
};

static const char* presetNames[] = {
    "Win+R -> cmd",
    "Win+R -> notepad",
    "Win+R -> calc",
    "Win+R -> Rick Roll",
    "Lock PC (Win+L)",
    "Task Manager",
    "Type 'Hello World!'",
    "Type Custom...",
};

static void executeRP2040Preset(Preset p) {
    switch (p) {
        case PRESET_WINR_CMD:
            rp2040KeyCombo(0x08, 'r'); delay(300);
            rp2040TypeText("cmd");
            rp2040Enter();
            break;
        case PRESET_WINR_NOTEPAD:
            rp2040KeyCombo(0x08, 'r'); delay(300);
            rp2040TypeText("notepad");
            rp2040Enter();
            break;
        case PRESET_WINR_CALC:
            rp2040KeyCombo(0x08, 'r'); delay(300);
            rp2040TypeText("calc");
            rp2040Enter();
            break;
        case PRESET_RICKROLL:
            rp2040KeyCombo(0x08, 'r'); delay(300);
            rp2040TypeText("https://youtu.be/dQw4w9WgXcQ");
            rp2040Enter();
            break;
        case PRESET_LOCK_PC:
            rp2040KeyCombo(0x08, 'l');
            break;
        case PRESET_TASK_MANAGER:
            rp2040KeyCombo(0x03, 0x1B);
            break;
        case PRESET_TEXT_HELLO:
            rp2040TypeText("Hello World!");
            break;
        case PRESET_TYPE_CUSTOM:
            break;
    }
}

static void attachHandlers() {
    btnUp.attachClick([]() {
        sel = (sel - 1 + PRESET_COUNT) % PRESET_COUNT;
    });

    btnDown.attachClick([]() {
        sel = (sel + 1) % PRESET_COUNT;
    });

    btnOK.attachClick([]() {
        if (sel == PRESET_TYPE_CUSTOM) {
            VirtualKeyboard vk(&display, &btnUp, &btnDown, &btnOK);
            String text = vk.run();

            attachHandlers();

            if (text.length() > 0) {
                delay(300);
                rp2040TypeText(text.c_str());
                rp2040Enter();
            }
        } else {
            executeRP2040Preset((Preset)sel);
        }
    });

    btnAction.attachClick([]() {});

    btnAction.attachLongPressStart([]() {
        running = false;
    });
}

void runKeyboardEmu() {
    sel = 0;
    running = true;
    attachHandlers();

    while (running) {
        esp_task_wdt_reset();
        appHeartBeat();

        display.clearBuffer();

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(2, 9, "Keyboard Emulation");

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(2, 17, "RP2040 USB HID");
        display.drawStr(60, 17, "I2C ready");

        display.drawHLine(0, 20, 128);

        int startIdx = max(0, sel - 2);
        int endIdx = min((int)PRESET_COUNT, startIdx + 5);
        int y = 30;
        for (int i = startIdx; i < endIdx; i++) {
            if (i == sel) {
                display.setDrawColor(1);
                display.drawRBox(0, y - 6, 128, 8, 2);
                display.setDrawColor(0);
            } else {
                display.setDrawColor(1);
            }

            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(3, y, presetNames[i]);
            
            y += 9;
        }

        display.sendBuffer();

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    drawMenu();
}
