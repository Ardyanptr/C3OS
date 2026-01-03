#include "FileManager.h"

#include <LittleFS.h>

#include <vector>

#include "system16/lua_l/lua_l.h"

struct FileItem {
    char name[64];
    bool isDir;
    size_t size;
};

static char currentPath[64] = "/";
static int cursor = 0;
static int scrollOffset = 0;
static float animCursorY = 0;
static bool exitManager = false;
static std::vector<FileItem> items;

const int ITEM_H = 12;
const int MAX_VISIBLE_ITEMS = 4;

static void updateScroll() {
    if (cursor >= scrollOffset + MAX_VISIBLE_ITEMS) {
        scrollOffset = cursor - MAX_VISIBLE_ITEMS + 1;
    }
    if (cursor < scrollOffset) {
        scrollOffset = cursor;
    }
}

static void scanDirectory() {
    items.clear();
    if (strcmp(currentPath, "/") != 0) {
        FileItem back;
        strcpy(back.name, "..");
        back.isDir = true;
        back.size = 0;
        items.push_back(back);
    }

    File root = LittleFS.open(currentPath);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        FileItem item;
        const char* fullPath = file.name();
        const char* fileNameOnly = strrchr(fullPath, '/');
        fileNameOnly = fileNameOnly ? fileNameOnly + 1 : fullPath;

        strncpy(item.name, fileNameOnly, sizeof(item.name) - 1);
        item.name[sizeof(item.name) - 1] = 0;
        item.isDir = file.isDirectory();
        item.size = file.size();
        items.push_back(item);
        file = root.openNextFile();
    }
    root.close();
}

static void onUp() {
    if (cursor > 0)
        cursor--;
    else
        cursor = (int)items.size() - 1;
    updateScroll();
}

static void onDown() {
    if (cursor < (int)items.size() - 1)
        cursor++;
    else
        cursor = 0;
    updateScroll();
}

static void onExit() {
    exitManager = true;
}

static void onOK() {
    if (items.empty()) return;
    FileItem selected = items[cursor];

    if (strcmp(selected.name, "..") == 0) {
        char* lastSlash = strrchr(currentPath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            if (strlen(currentPath) == 0) strcpy(currentPath, "/");
        }
        cursor = 0;
        scrollOffset = 0;
        scanDirectory();
        return;
    }

    char fullPath[128];
    if (currentPath[strlen(currentPath) - 1] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", currentPath, selected.name);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, selected.name);
    }

    if (selected.isDir) {
        strncpy(currentPath, fullPath, sizeof(currentPath) - 1);
        cursor = 0;
        scrollOffset = 0;
        scanDirectory();
    } else {
        size_t len = strlen(selected.name);
        if (len > 4 && strcmp(selected.name + len - 4, ".lua") == 0) {
            exitManager = true;
            runLuaScript(fullPath);
        }
    }
}

static void drawUI() {
    display.clearBuffer();

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    int freePercent = (total > 0) ? ((total - used) * 100) / total : 0;

    display.setFont(u8g2_font_5x7_tr);
    display.setDrawColor(1);
    display.setCursor(0, 6);
    display.printf("DIR: %s", currentPath);
    display.setCursor(105, 6);
    display.printf("%d%%", freePercent);
    display.drawHLine(0, 8, 128);

    float targetY = 10 + ((cursor - scrollOffset) * ITEM_H);
    animCursorY += (targetY - animCursorY) * 0.4f;

    display.setDrawColor(1);
    display.drawRBox(0, (int)animCursorY, 124, ITEM_H, 2);

    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int idx = i + scrollOffset;
        if (idx >= (int)items.size()) break;

        int textY = 19 + (i * ITEM_H);
        display.setDrawColor(idx == cursor ? 0 : 1);

        display.drawStr(2, textY, items[idx].isDir ? ">" : ".");
        display.drawStr(12, textY, items[idx].name);

        if (!items[idx].isDir) {
            char sizeBuf[12];
            if (items[idx].size < 1024)
                snprintf(sizeBuf, sizeof(sizeBuf), "%dB", (int)items[idx].size);
            else
                snprintf(sizeBuf, sizeof(sizeBuf), "%dK", (int)items[idx].size / 1024);
            display.drawStr(122 - display.getStrWidth(sizeBuf), textY, sizeBuf);
        }
    }

    if (items.size() > MAX_VISIBLE_ITEMS) {
        display.setDrawColor(1);
        int trackH = 48;
        int barH = (MAX_VISIBLE_ITEMS * trackH) / items.size();
        if (barH < 4) barH = 4;
        int barY = 10 + (scrollOffset * trackH) / items.size();
        display.drawVLine(127, 10, trackH);
        display.drawBox(126, barY, 2, barH);
    }

    display.sendBuffer();
}

void runFileManager() {
    exitManager = false;
    cursor = 0;
    scrollOffset = 0;
    scanDirectory();

    btnUp.attachClick(onUp);
    btnDown.attachClick(onDown);
    btnOK.attachClick(onOK);
    btnOK.attachLongPressStart(onExit);

    while (!exitManager) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        drawUI();
        delay(5);
    }

    drawMenu();
}