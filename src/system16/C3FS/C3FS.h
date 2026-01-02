#pragma once
#include <Arduino.h>
#include <LittleFS.h>

class C3FS {
   public:
    static bool begin();

    static bool readFile(const char* path, char* buffer, size_t bufferSize);
    static bool writeFile(const char* path, const char* content);

    static size_t getFileSize(const char* path);
    static void getStatus(size_t& total, size_t& used);

    static bool exists(const char* path) { return LittleFS.exists(path); }
    static bool remove(const char* path) { return LittleFS.remove(path); }
};