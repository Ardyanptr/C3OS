#include "C3FS_Cache.h"
#include <U8g2lib.h>

#include "config/config.h"

bool C3FS_Cache::init() {
    if (!LittleFS.exists("/cfg/cache")) {
        return LittleFS.mkdir("/cfg/cache");
    }
    return true;
}

String C3FS_Cache::getCachePath(const char* name) {
    String path = "/cfg/cache/";
    path += name;
    path += ".bin";
    return path;
}

bool C3FS_Cache::saveCache(const char* name, const void* items, size_t itemSize, int count) {
    init();
    File f = LittleFS.open(getCachePath(name), "w");
    if (!f) return false;
    
    size_t written = f.write((const uint8_t*)items, itemSize * count);
    f.close();
    return written == (itemSize * count);
}

bool C3FS_Cache::readCacheItem(const char* name, int index, void* buffer, size_t itemSize) {
    File f = LittleFS.open(getCachePath(name), "r");
    if (!f) return false;
    
    if (!f.seek(index * itemSize)) {
        f.close();
        return false;
    }
    
    size_t r = f.read((uint8_t*)buffer, itemSize);
    f.close();
    return r == itemSize;
}

int C3FS_Cache::getCacheCount(const char* name, size_t itemSize) {
    File f = LittleFS.open(getCachePath(name), "r");
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s / itemSize;
}

void C3FS_Cache::drawRefiningBar(int progress) {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(30, 30, "Refining cache...");
    display.drawFrame(20, 35, 88, 6);
    display.drawBox(22, 37, (progress * 84) / 100, 2);
    display.sendBuffer();
}

bool C3FS_Cache::refineCache(const char* name, size_t itemSize) {
    String path = getCachePath(name);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    size_t size = f.size();
    if (size % itemSize != 0) {
        f.close();
        return false; // Broken structure
    }
    
    int count = size / itemSize;
    for (int i = 0; i <= 100; i += 20) {
        drawRefiningBar(i);
        delay(10); // Simulate "refining" process
    }
    
    f.close();
    return true;
}

void C3FS_Cache::clearCache(const char* name) {
    if (name) {
        LittleFS.remove(getCachePath(name));
    } else {
        File root = LittleFS.open("/cfg/cache");
        File f = root.openNextFile();
        while (f) {
            String p = f.path();
            f.close();
            LittleFS.remove(p);
            f = root.openNextFile();
        }
    }
}

void C3FS_Cache::getCacheMetrics(size_t &used, size_t &totalCount) {
    used = 0;
    totalCount = 0;
    File root = LittleFS.open("/cfg/cache");
    if (!root || !root.isDirectory()) return;
    
    File f = root.openNextFile();
    while (f) {
        used += f.size();
        totalCount++;
        f = root.openNextFile();
    }
}
