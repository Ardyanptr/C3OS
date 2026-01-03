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
static bool luaRunning = false;

const int ITEM_H = 12;
const int MAX_VISIBLE_ITEMS = 5;

static void scanDirectory() {
    items.clear();
    File root = LittleFS.open(currentPath);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        FileItem item;
        const char* fullPath = file.name();

        // Cari karakter '/' terakhir untuk memisahkan nama file dari foldernya
        const char* fileNameOnly = strrchr(fullPath, '/');
        if (fileNameOnly) {
            fileNameOnly++;  // Lewati karakter '/'
        } else {
            fileNameOnly = fullPath;  // Jika tidak ada '/', berarti sudah nama file
        }

        strncpy(item.name, fileNameOnly, sizeof(item.name) - 1);
        item.name[sizeof(item.name) - 1] = 0;

        item.isDir = file.isDirectory();
        item.size = file.size();

        // Filter: Jangan masukkan folder itu sendiri ke dalam list jika namanya sama dengan currentPath
        items.push_back(item);
        file = root.openNextFile();
    }
    root.close();
}

static void onUp() {
    if (cursor > 0)
        cursor--;
    else
        cursor = items.size() - 1;
}

static void onDown() {
    if (cursor < items.size() - 1)
        cursor++;
    else
        cursor = 0;
}

static void onExit() {
    exitManager = true;
    drawMenu();
}

static void onOK() {
    FileItem selected = items[cursor];

    // Buat buffer untuk path lengkap
    char fullPath[128];
    // Pastikan currentPath diakhiri '/' jika belum ada
    if (currentPath[strlen(currentPath) - 1] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", currentPath, selected.name);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, selected.name);
    }

    if (selected.isDir) {
        strncpy(currentPath, fullPath, sizeof(currentPath) - 1);
        currentPath[sizeof(currentPath) - 1] = 0;
        cursor = 0;
        scrollOffset = 0;  // Reset scroll juga
        scanDirectory();
    } else {
        size_t len = strlen(selected.name);
        if (len > 4 && strcmp(selected.name + len - 4, ".lua") == 0) {
            exitManager = true;
            runLuaScript(fullPath);  // Kirim path LENGKAP ke Lua
        }
    }
}

static void drawUI() {
    display.clearBuffer();

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    int freePercent = ((total - used) * 100) / total;

    display.setFont(u8g2_font_5x7_tr);
    display.setDrawColor(1);
    display.setCursor(0, 6);
    display.printf("PATH: %s", currentPath);
    display.setCursor(90, 6);
    display.printf("FREE: %d%%", freePercent);
    display.drawHLine(0, 8, 128);

    float targetY = (cursor - scrollOffset) * ITEM_H + 11;
    animCursorY += (targetY - animCursorY) * 0.3f;
    display.drawRBox(0, (int)animCursorY, 128, ITEM_H, 2);

    if (cursor >= scrollOffset + MAX_VISIBLE_ITEMS) scrollOffset = cursor - MAX_VISIBLE_ITEMS + 1;
    if (cursor < scrollOffset) scrollOffset = cursor;

    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int idx = i + scrollOffset;
        if (idx >= items.size()) break;

        int y = 20 + (i * ITEM_H);
        display.setDrawColor(idx == cursor ? 0 : 1);

        if (items[idx].isDir)
            display.drawStr(2, y, " > ");
        else
            display.drawStr(2, y, " # ");

        display.drawStr(15, y, items[idx].name);

        if (!items[idx].isDir) {
            char sizeBuf[10];
            snprintf(sizeBuf, sizeof(sizeBuf), "%d B", items[idx].size);
            display.drawStr(125 - display.getStrWidth(sizeBuf), y, sizeBuf);
        }
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
        delay(10);
    }

    exitManager = true;
    drawMenu();
}