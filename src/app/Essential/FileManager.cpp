#include "FileManager.h"

#include <LittleFS.h>
#include <vector>
#include <algorithm>

#include "esp_task_wdt.h"
#include "system16/emulator/Chip8.h"
#include "system16/lua_l/lua_l.h"
#include "../../system16/C3FS/C3FS_Cache.h"

Chip8 myChip8;

// ─────────────────────────────────────────────────────────────
//  Icons (Modern Minimalist)
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  State
// ─────────────────────────────────────────────────────────────
enum FM_Mode { MODE_DRIVES, MODE_FILES };
static FM_Mode fmMode = MODE_DRIVES;

static char currentPath[64] = "/";
static int  cursor           = 0;
static bool exitManager      = false;
static int  itemCount        = 0;

static const int ICON_W = 32;
static const int ICON_H = 32;
static const int COLS   = 3;
static const int ROWS   = 2;
static const int FM_PAGE_SIZE = COLS * ROWS;

static float animCursorX = 10;
static float animCursorY = 10;
static bool showProps = false;
static char pendingLua[128] = {0};
static bool redrawNeeded = true;

// Cached visible items to avoid cache reads every frame
static FileItem cachedItems[FM_PAGE_SIZE];
static int cachedStart = -1;
static String cachedCacheName = "";

// ─────────────────────────────────────────────────────────────
//  Logic
// ─────────────────────────────────────────────────────────────
static String getPathHash(const char* path) {
    uint32_t h = 0x811c9dc5;
    while (*path) { h ^= (uint32_t)*path++; h *= 0x01000193; }
    return String(h, HEX);
}

static void scanDirectory() {
    if (fmMode == MODE_DRIVES) {
        itemCount = 1;
        cachedStart = -1;
        return;
    }

    String cacheName = "fm_" + getPathHash(currentPath);
    if (C3FS_Cache::refineCache(cacheName.c_str(), sizeof(FileItem))) {
        itemCount = C3FS_Cache::getCacheCount(cacheName.c_str(), sizeof(FileItem));
        cachedStart = -1;
        return;
    }

    std::vector<FileItem> items;
    if (strcmp(currentPath, "/") != 0) {
        FileItem back; strcpy(back.name, ".."); back.isDir = true; back.size = 0;
        items.push_back(back);
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
            items.push_back(item);
            file = root.openNextFile();
        }
        root.close();
    }
    itemCount = items.size();
    C3FS_Cache::saveCache(cacheName.c_str(), items.data(), sizeof(FileItem), items.size());
    cachedStart = -1;
}

static void loadVisibleItems() {
    int start = (cursor / FM_PAGE_SIZE) * FM_PAGE_SIZE;
    cachedCacheName = "fm_" + getPathHash(currentPath);
    for (int i = 0; i < FM_PAGE_SIZE; i++) {
        int idx = start + i;
        if (idx >= itemCount) break;
        C3FS_Cache::readCacheItem(cachedCacheName.c_str(), idx, &cachedItems[i], sizeof(FileItem));
    }
    cachedStart = start;
}

static void onUp() {
    if (showProps) return;
    int prev = cursor;
    cursor = (cursor > 0) ? cursor - 1 : itemCount - 1;
    if ((cursor / FM_PAGE_SIZE) != (prev / FM_PAGE_SIZE)) loadVisibleItems();
    redrawNeeded = true;
}

static void onDown() {
    if (showProps) return;
    int prev = cursor;
    cursor = (cursor < itemCount - 1) ? cursor + 1 : 0;
    if ((cursor / FM_PAGE_SIZE) != (prev / FM_PAGE_SIZE)) loadVisibleItems();
    redrawNeeded = true;
}

static void onOK() {
    if (showProps) { showProps = false; redrawNeeded = true; return; }

    if (fmMode == MODE_DRIVES) {
        fmMode = MODE_FILES;
        strcpy(currentPath, "/");
        cursor = 0;
        scanDirectory();
        loadVisibleItems();
        redrawNeeded = true;
        return;
    }

    String cacheName = "fm_" + getPathHash(currentPath);
    FileItem selected;
    if (!C3FS_Cache::readCacheItem(cacheName.c_str(), cursor, &selected, sizeof(FileItem))) return;

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
        loadVisibleItems();
        redrawNeeded = true;
    } else {
        char full[128];
        snprintf(full, 128, "%s%s%s", currentPath, currentPath[strlen(currentPath)-1]=='/'?"":"/", selected.name);
        if (String(selected.name).endsWith(".lua")) { strcpy(pendingLua, full); }
        else if (String(selected.name).endsWith(".ch8")) { strcpy(pendingLua, "CHIP8:"); strcat(pendingLua, full); }
    }
}

static void onAction() {
    if (fmMode == MODE_DRIVES) { showProps = !showProps; redrawNeeded = true; }
}

// ─────────────────────────────────────────────────────────────
//  Drawing
// ─────────────────────────────────────────────────────────────
static void drawPie(int x, int y, int r, float p) {
    display.drawCircle(x, y, r);
    for (int a = 0; a < (int)(p * 360); a += 10) {
        float rad = a * 0.01745f;
        display.drawLine(x, y, (int)(x + cos(rad) * r), (int)(y + sin(rad) * r));
    }
}

static void drawUI() {
    display.clearBuffer();
    const int W = 128, H = 64;

    if (showProps) {
        display.drawRFrame(10, 5, 108, 54, 4);
        display.setFont(u8g2_font_5x7_tr);
        size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
        drawPie(30, 32, 18, (float)used/total);
        display.drawStr(55, 20, "Internal Flash");
        display.drawStr(55, 30, "Type: LittleFS");
        char buf[32];
        snprintf(buf, 32, "Used: %d%%", (int)(used * 100 / total));
        display.drawStr(55, 40, buf);
        snprintf(buf, 32, "Free: %d KB", (int)((total-used)/1024));
        display.drawStr(55, 50, buf);
    } else {
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(2, 8, fmMode == MODE_DRIVES ? "Drives" : currentPath);
        display.drawHLine(0, 10, W);

        int start = (cursor / FM_PAGE_SIZE) * FM_PAGE_SIZE;
        for (int i = 0; i < FM_PAGE_SIZE; i++) {
            int idx = start + i;
            if (idx >= itemCount) break;

            int gx = i % COLS;
            int gy = i / COLS;
            int ix = 10 + gx * 40;
            int iy = 15 + gy * 24;

            if (idx == cursor) {
                float tx = (float)ix - 2, ty = (float)iy - 2;
                animCursorX += (tx - animCursorX) * 0.4f;
                animCursorY += (ty - animCursorY) * 0.4f;
                display.drawRFrame((int)animCursorX, (int)animCursorY, 28, 22, 2);
            }

            if (fmMode == MODE_DRIVES) {
                display.drawXBMP(ix + 4, iy, 16, 16, icon_drive);
                display.drawStr(ix, iy + 22, "Internal");
            } else if (idx >= cachedStart && idx < cachedStart + FM_PAGE_SIZE) {
                int ci = idx - cachedStart;
                display.drawXBMP(ix + 4, iy, 16, 16, cachedItems[ci].isDir ? icon_folder : icon_file);
                char t[10]; strncpy(t, cachedItems[ci].name, 6); t[6] = 0;
                display.drawStr(ix, iy + 22, t);
            }
        }
    }
    display.sendBuffer();
}

void runFileManager() {
    fmMode = MODE_DRIVES; cursor = 0; exitManager = false; showProps = false;
    animCursorX = 10; animCursorY = 15; redrawNeeded = true;
    scanDirectory();
    loadVisibleItems();
    
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
                     // Chip8 runs in a background task — wait for user to exit
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
            loadVisibleItems();
            redrawNeeded = true;
            exitManager = true;
            break;
        }

        int tx = 10 + (cursor % COLS) * 40 - 2;
        int ty = 15 + ((cursor % FM_PAGE_SIZE) / COLS) * 24 - 2;
        bool cursorMoving = fabs(animCursorX - tx) > 0.5f || fabs(animCursorY - ty) > 0.5f;

        if (redrawNeeded || cursorMoving) {
            drawUI();
            redrawNeeded = false;
        }

        delay(cursorMoving ? 16 : 33);
    }
    drawMenu();
}
