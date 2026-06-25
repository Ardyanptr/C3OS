#include "FileManager.h"

#include <LittleFS.h>
#include <vector>
#include <algorithm>

#include "esp_task_wdt.h"
#include "system16/emulator/Chip8.h"
#include "system16/lua_l/lua_l.h"
#include "system16/esp826.h"

Chip8 myChip8;

static const unsigned char icon_folder[] PROGMEM = {
    0xfe, 0x07, 0x01, 0x08, 0x01, 0x08, 0xff, 0xff, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0xff, 0x00, 0x00};
static const unsigned char icon_file[] PROGMEM = {
    0xfc, 0x3f, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20,
    0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0xfc, 0x3f, 0x00, 0x00};
static const unsigned char icon_drive[] PROGMEM = {
    0xff, 0xff, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0xff,
    0xff, 0xff, 0x01, 0x80, 0x21, 0x84, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0xff, 0x00, 0x00};

struct FileItem {
    char   name[64];
    bool   isDir;
    size_t size;
};

enum FM_Mode { MODE_DRIVES, MODE_FILES, MODE_ESP8266_FILES };
static FM_Mode fmMode = MODE_DRIVES;

static char currentPath[64] = "/";
static int  cursor           = 0;
static bool exitManager      = false;
static int  itemCount        = 0;

static const int LINE_H = 12;
static const int VISIBLE_LINES = 4;

static std::vector<FileItem> fileList;
static bool redrawNeeded = true;
static char pendingLua[128] = {0};

static void scanDirectory() {
    fileList.clear();
    if (fmMode == MODE_DRIVES) {
        FileItem internal;
        strcpy(internal.name, "Internal Flash");
        internal.isDir = true;
        internal.size = 0;
        fileList.push_back(internal);

        FileItem esp8266;
        strcpy(esp8266.name, "ESP8266 Storage");
        esp8266.isDir = true;
        esp8266.size = 0;
        fileList.push_back(esp8266);

        itemCount = fileList.size();
        return;
    }

    if (fmMode == MODE_ESP8266_FILES) {
        sendCommand("esp:list");

        String buffer;
        uint32_t start = millis();
        while (millis() - start < 2000) {
            while (Serial1.available()) {
                char c = Serial1.read();
                if (c == '\n') {
                    buffer.trim();
                    if (buffer == "OK") {
                        itemCount = fileList.size();
                        return;
                    }
                    if (buffer.length() > 0) {
                        FileItem item;
                        int comma = buffer.indexOf(',');
                        if (comma > 0) {
                            String fname = buffer.substring(0, comma);
                            String fsize = buffer.substring(comma + 1);
                            fsize.trim();
                            strncpy(item.name, fname.c_str(), sizeof(item.name) - 1);
                            item.name[sizeof(item.name) - 1] = 0;
                            item.size = (size_t)fsize.toInt();
                        } else {
                            strncpy(item.name, buffer.c_str(), sizeof(item.name) - 1);
                            item.name[sizeof(item.name) - 1] = 0;
                            item.size = 0;
                        }
                        item.isDir = false;
                        fileList.push_back(item);
                    }
                    buffer = "";
                } else {
                    buffer += c;
                }
            }
            delay(5);
        }
        itemCount = fileList.size();
        return;
    }

    if (strcmp(currentPath, "/") != 0) {
        FileItem back;
        strcpy(back.name, "..");
        back.isDir = true;
        back.size = 0;
        fileList.push_back(back);
    }

    File root = LittleFS.open(currentPath);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            FileItem item;
            const char *full = file.name();
            const char *nameOnly = strrchr(full, '/');
            nameOnly = nameOnly ? nameOnly + 1 : full;
            strncpy(item.name, nameOnly, sizeof(item.name) - 1);
            item.name[sizeof(item.name) - 1] = 0;
            item.isDir = file.isDirectory();
            item.size = file.size();
            fileList.push_back(item);
            file = root.openNextFile();
        }
        root.close();
    }
    itemCount = fileList.size();
}

static void onUp() {
    if (cursor > 0) cursor--; else cursor = itemCount - 1;
    redrawNeeded = true;
}

static void onDown() {
    if (cursor < itemCount - 1) cursor++; else cursor = 0;
    redrawNeeded = true;
}

static void onOK() {
    if (fmMode == MODE_DRIVES) {
        if (cursor == 0) {
            fmMode = MODE_FILES;
            strcpy(currentPath, "/");
        } else {
            fmMode = MODE_ESP8266_FILES;
        }
        cursor = 0;
        scanDirectory();
        redrawNeeded = true;
        return;
    }

    if (cursor < 0 || cursor >= itemCount) return;
    FileItem& selected = fileList[cursor];

    if (selected.isDir) {
        if (strcmp(selected.name, "..") == 0) {
            char *last = strrchr(currentPath, '/');
            if (last == currentPath) { fmMode = MODE_DRIVES; cursor = 0; }
            else if (last) { *last = '\0'; }
        } else {
            if (currentPath[strlen(currentPath)-1] != '/') strcat(currentPath, "/");
            strcat(currentPath, selected.name);
        }
        cursor = 0;
        scanDirectory();
        redrawNeeded = true;
    } else {
        char full[128];
        snprintf(full, 128, "%s%s%s", currentPath, currentPath[strlen(currentPath)-1]=='/'?"":"/", selected.name);
        if (String(selected.name).endsWith(".lua")) { strcpy(pendingLua, full); }
        else if (String(selected.name).endsWith(".ch8")) { strcpy(pendingLua, "CHIP8:"); strcat(pendingLua, full); }
    }
}

static void onAction() {
    if (fmMode == MODE_DRIVES) {
        exitManager = true;
        return;
    }
    fmMode = MODE_DRIVES;
    cursor = 0;
    scanDirectory();
    redrawNeeded = true;
}

static void drawUI() {
    display.clearBuffer();

    display.setFont(u8g2_font_5x7_tr);
    if (fmMode == MODE_DRIVES) {
        display.drawStr(2, 8, "Select Drive");
    } else if (fmMode == MODE_ESP8266_FILES) {
        display.drawStr(2, 8, "ESP8266 Storage");
    } else {
        display.drawStr(2, 8, currentPath);
    }
    display.drawHLine(0, 10, 128);

    int scrollOffset = 0;
    if (cursor >= VISIBLE_LINES) scrollOffset = cursor - VISIBLE_LINES + 1;

    for (int i = 0; i < VISIBLE_LINES; i++) {
        int idx = scrollOffset + i;
        if (idx >= itemCount) break;

        int y = 21 + i * LINE_H;
        bool selected = (idx == cursor);

        if (selected) {
            display.drawBox(0, y - 9, 128, LINE_H - 1);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        const unsigned char* ico;
        if (fmMode == MODE_DRIVES) {
            ico = icon_drive;
        } else if (fmMode == MODE_ESP8266_FILES) {
            ico = icon_file;
        } else if (idx == 0 && strcmp(currentPath, "/") != 0 && fileList[idx].isDir && strcmp(fileList[idx].name, "..") == 0) {
            ico = icon_folder;
        } else {
            ico = fileList[idx].isDir ? icon_folder : icon_file;
        }
        display.drawXBMP(2, y - 7, 16, 16, ico);

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(20, y, fileList[idx].name);

        if (!selected && !fileList[idx].isDir) {
            char sz[16];
            if (fileList[idx].size > 1024) {
                snprintf(sz, sizeof(sz), "%uKB", (unsigned int)(fileList[idx].size / 1024));
            } else {
                snprintf(sz, sizeof(sz), "%uB", (unsigned int)fileList[idx].size);
            }
            uint8_t sw = display.getStrWidth(sz);
            display.setDrawColor(1);
            display.drawStr(128 - sw - 2, y, sz);
        }

        display.setDrawColor(1);
    }

    if (itemCount > VISIBLE_LINES) {
        int barH = (VISIBLE_LINES * 44) / itemCount;
        int barY = 12 + (scrollOffset * 44) / (itemCount - VISIBLE_LINES);
        display.drawBox(126, barY, 2, barH);
    }

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 63, "OK=Open  ACT=Back");
    display.sendBuffer();
}

void runFileManager() {
    fmMode = MODE_DRIVES; cursor = 0; exitManager = false;
    redrawNeeded = true;
    scanDirectory();

    btnUp.attachClick(onUp);
    btnDown.attachClick(onDown);
    btnOK.attachClick(onOK);
    btnOK.attachLongPressStart([](){ exitManager = true; });
    btnAction.attachClick(onAction);

    while (!exitManager) {
        esp_task_wdt_reset();
        btnUp.tick(); btnDown.tick(); btnOK.tick(); btnAction.tick();

        if (pendingLua[0]) {
            if (strncmp(pendingLua, "CHIP8:", 6) == 0) {
                 const char *romPath = pendingLua + 6;
                 myChip8.init(&display);
                 if (myChip8.loadROM(romPath)) {
                     myChip8.start();
                     bool chip8Done = false;
                     while (!chip8Done) {
                         btnUp.tick(); btnDown.tick(); btnOK.tick(); btnAction.tick();
                         if (digitalRead(BUTTON_ACTION) == LOW) {
                             unsigned long start = millis();
                             while (digitalRead(BUTTON_ACTION) == LOW) {
                                 if (millis() - start > 1000) {
                                     chip8Done = true;
                                     break;
                                 }
                                 delay(10);
                                 esp_task_wdt_reset();
                             }
                         }
                         delay(20);
                         esp_task_wdt_reset();
                     }
                     myChip8.stop();
                 }
            } else {
                 runLuaScript(pendingLua);
            }
            pendingLua[0] = 0;
            delay(200);
            scanDirectory();
            redrawNeeded = true;
            exitManager = true;
            break;
        }

        if (redrawNeeded) {
            drawUI();
            redrawNeeded = false;
        }

        delay(33);
    }
    drawMenu();
}
