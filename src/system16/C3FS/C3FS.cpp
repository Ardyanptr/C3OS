#include "C3FS.h"

bool C3FS::begin() { LittleFS.begin(true); }

bool C3FS::readFile(const char* path, char* buffer, size_t bufferSize) {
    File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) return false;

    size_t fileSize = file.size();
    if (fileSize >= bufferSize) {
        file.close();
        return false;
    }

    size_t bytesRead = file.readBytes(buffer, fileSize);
    buffer[bytesRead] = '\0';

    file.close();
    return true;
}

bool C3FS::writeFile(const char* path, const char* content) {
    char tempPath[32];
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", path);

    File file = LittleFS.open(tempPath, "w");
    if (!file) return false;

    bool success = file.print(content);
    file.close();

    if (success) {
        LittleFS.remove(path);
        return LittleFS.rename(tempPath, path);
    }

    return false;
}

size_t C3FS::getFileSize(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) return 0;

    size_t s = file.size();
    file.close();

    return s;
}

void C3FS::getStatus(size_t& total, size_t& used) {
    total = LittleFS.totalBytes();
    used = LittleFS.usedBytes();
}

#include <vector>

static void gatherNonWhitelistedFiles(const String& dirPath, std::vector<String>& filesToDelete) {
    File root = LittleFS.open(dirPath);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        String path = file.path();
        bool isDir = file.isDirectory();

        bool keep = false;
        if (path == "/init.lua") keep = true;
        else if (path == "/bin" || path.startsWith("/bin/")) keep = true;
        else if (path == "/cfg" || path.startsWith("/cfg/")) keep = true;
        else if (path == "/systemd" || path.startsWith("/systemd/")) keep = true;
        else if (path == "/security" || path.startsWith("/security/")) keep = true;
        else if (path == "/GameClient" || path.startsWith("/GameClient/")) keep = true;

        if (isDir) {
            file.close();
            gatherNonWhitelistedFiles(path, filesToDelete);
        } else {
            file.close();
            if (!keep) {
                filesToDelete.push_back(path);
            }
        }
        file = root.openNextFile();
    }
}

void C3FS::safeWipe() {
    std::vector<String> filesToDelete;
    gatherNonWhitelistedFiles("/", filesToDelete);
    for (const auto& path : filesToDelete) {
        LittleFS.remove(path);
        Serial.printf("[SAFE WIPE] Deleted: %s\n", path.c_str());
    }
}