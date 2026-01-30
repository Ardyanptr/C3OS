#include "StoreManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// ================= SAFE INSTALL =================
bool checkLuaSyntax_File(const char* path);
bool hasSensitiveAPI_File(const char* path);

// ================= DATA =================
struct StoreApp_Store {
    String name;
    String url;
    String file;
    String desc;
    String license;
    String author;
};

std::vector<StoreApp_Store> storeItems_Store;

// ================= STATE =================
enum StoreView_Store {
    VIEW_LIST_Store,
    VIEW_OVERVIEW_Store,
    VIEW_INSTALLING_Store,
    VIEW_WIFI_ERROR_Store,
    VIEW_WARNING_Store,
    VIEW_ERROR_Store
};

StoreView_Store storeView_Store;
bool needRedraw_Store = true;
bool exitStore_Store = false;

int cursor_Store = 0;
int offset_Store = 0;
int selected_Store = -1;
int downloadProgress_Store = 0;
int totalBytes_Store = 0;
int currentBytes_Store = 0;
int chunkCount_Store = 0;

static bool pendingInstall_Store = false;
static bool sensitiveDetected_Store = false;
static String pendinFile_Store;

float visualProgress_Store = 0;

// ================= FORWARD =================
void setView_Store(StoreView_Store v);
bool downloadApp_Store(String url, String file);
void fetchStore_Store(const char* url);

void drawWarning_Store();

// ================= BUTTON =================
void onUp_Store() {
    if (cursor_Store > 0) cursor_Store--;
    if (cursor_Store < offset_Store) offset_Store = cursor_Store;
    needRedraw_Store = true;
}

void onDown_Store() {
    if (cursor_Store < (int)storeItems_Store.size() - 1) cursor_Store++;
    if (cursor_Store >= offset_Store + 4) offset_Store = cursor_Store - 3;
    needRedraw_Store = true;
}

void onOK_Store() {
    selected_Store = cursor_Store;
    setView_Store(VIEW_OVERVIEW_Store);
}

void onBack_Store() {
    exitStore_Store = true;
    drawMenu();
}

void onInstall_Store() {
    setView_Store(VIEW_INSTALLING_Store);
}

void onOverviewBack_Store() {
    setView_Store(VIEW_LIST_Store);
}

void onWiFiRetry_Store() {
    setView_Store(VIEW_LIST_Store);
}

void onWiFiExit_Store() {
    exitStore_Store = true;
    drawMenu();
}

void onWarningOK_Store() {
    LittleFS.rename("/tmp/app.lua.part", "/bin/" + pendinFile_Store);
    pendingInstall_Store = false;
    setView_Store(VIEW_LIST_Store);
}

void onWarningCancel() {
    LittleFS.remove("/tmp/app.lua.part");
    pendingInstall_Store = false;
    setView_Store(VIEW_LIST_Store);
}

// ================= VIEW SWITCH =================
void setView_Store(StoreView_Store v) {
    storeView_Store = v;
    needRedraw_Store = true;

    btnUp.reset();
    btnDown.reset();
    btnOK.reset();

    switch (v) {
        case VIEW_LIST_Store:
            btnUp.attachClick(onUp_Store);
            btnDown.attachClick(onDown_Store);
            btnOK.attachClick(onOK_Store);
            btnOK.attachLongPressStart(onBack_Store);
            break;

        case VIEW_OVERVIEW_Store:
            btnOK.attachClick(onInstall_Store);
            btnOK.attachLongPressStart(onOverviewBack_Store);
            break;

        case VIEW_WIFI_ERROR_Store:
            btnOK.attachClick(onWiFiRetry_Store);
            btnOK.attachLongPressStart(onWiFiExit_Store);
            break;

        case VIEW_WARNING_Store:
            btnOK.attachClick(onWarningOK_Store);
            btnOK.attachLongPressStart(onWarningCancel);
            break;

        default:
            break;
    }
}

// ================= UI =================
void drawTetBoxWrap(int x, int y, int w, int h, const char* text) {
    int lineH = display.getMaxCharHeight();
    int cx = x + 2;
    int cy = y + lineH;

    char buf[256];
    strncpy(buf, text, sizeof(buf) - 1);

    char* word = strtok(buf, " ");
    while (word) {
        int ww = display.getStrWidth(word);
        int sw = display.getStrWidth(" ");

        if (cx + ww > x + w) {
            cx = x + 2;
            cy += lineH;
            if (cy > y + h) break;
        }

        display.drawStr(cx, cy, word);
        cx += ww + sw;

        word = strtok(nullptr, " ");
    }
}

void drawWarning_Store() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(3, 10, "Warning!");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(3, 20, "The last file you downloaded");
    display.drawStr(3, 27, "are containing some");
    display.drawStr(3, 34, "sensitive API usage!");

    display.drawRFrame(3, 52, 43, 10, 2);
    display.drawRBox(48, 52, 33, 10, 2);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(5, 60, "Continue");

    display.setDrawColor(2);
    display.drawStr(50, 60, "Cancel");

    display.setDrawColor(1);
    display.drawXBM(52, 3, 9, 8, image_Alert_bits);

    display.sendBuffer();
}

void drawList_Store() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tr);

    display.drawStr(0, 7, "C3 App Store");
    display.drawHLine(0, 9, 128);

    for (int i = 0; i < 4; i++) {
        int idx = offset_Store + i;
        if (idx >= storeItems_Store.size()) break;

        int y = 22 + i * 12;
        if (idx == cursor_Store) {
            display.drawRBox(0, y - 9, 128, 11, 2);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(6, y, storeItems_Store[idx].name.c_str());
    }

    display.setDrawColor(1);
    display.sendBuffer();
}

void drawOverview_Store() {
    auto& app = storeItems_Store[selected_Store];

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(2, 10, "Device Optimizer");

    display.setFont(u8g2_font_4x6_tr);

    drawTetBoxWrap(1, 14, 125, 15, app.desc.c_str());

    display.drawRBox(2, 52, 39, 10, 2);

    display.setDrawColor(2);
    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(4, 60, "Install");

    display.setDrawColor(1);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(4, 38, "License:");
    display.drawStr(4, 45, "Author:");

    display.drawStr(36, 38, app.license.c_str());
    display.drawStr(36, 45, app.author.c_str());

    display.sendBuffer();
}

void drawInstalling_Store() {
    auto& app = storeItems_Store[selected_Store];
    display.clearBuffer();

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(10, 11, app.name.c_str());

    String loadingText = "Installing";
    int dots = (millis() / 500) % 4;
    for (int i = 0; i < dots; i++) loadingText += ".";

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 20, loadingText.c_str());

    char infoBuf[32];

    snprintf(infoBuf, sizeof(infoBuf), "Data: %d / % d B", currentBytes_Store, totalBytes_Store);
    display.drawStr(10, 32, infoBuf);

    snprintf(infoBuf, sizeof(infoBuf), "Chunk: #%d (OK)", chunkCount_Store);
    display.drawStr(10, 39, infoBuf);

    display.drawStr(10, 48, "Target: /bin");

    display.drawRFrame(10, 58, 108, 5, 2);

    visualProgress_Store += (downloadProgress_Store - visualProgress_Store) * 0.2;

    int barWidth = (int)(visualProgress_Store * 104 / 100);
    if (barWidth > 0) display.drawRBox(12, 60, barWidth, 1, 0);

    display.sendBuffer();
}

void drawWiFiError_Store() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(2, 2, 7, 7, image_Pin_star_bits);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(12, 9, "WiFi Failed!");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 18, "Failed connecting to Store!");
    display.drawStr(2, 25, "check your network connection!");

    display.drawRFrame(1, 53, 43, 10, 2);
    display.drawRBox(46, 53, 23, 10, 2);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(9, 61, "Retry");

    display.setDrawColor(2);
    display.drawStr(48, 61, "Exit");

    display.setDrawColor(1);

    display.sendBuffer();
}

// ================= NETWORK =================
void fetchStore_Store(const char* url) {
    storeItems_Store.clear();

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (http.begin(client, url) && http.GET() == HTTP_CODE_OK) {
        JsonDocument doc;
        deserializeJson(doc, http.getString());

        for (JsonObject a : doc["apps"].as<JsonArray>()) {
            storeItems_Store.push_back({a["name"].as<String>(),
                                        a["url"].as<String>(),
                                        a["file"].as<String>(),
                                        a["desc"].as<String>(),
                                        a["license"].as<String>(),
                                        a["author"].as<String>()});
        }
    }
    http.end();
}

bool downloadApp_Store(String url, String file) {
    downloadProgress_Store = 0;
    visualProgress_Store = 0;

    setView_Store(VIEW_INSTALLING_Store);

    if (!LittleFS.exists("/tmp")) LittleFS.mkdir("/tmp");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, url)) return false;
    if (http.GET() != HTTP_CODE_OK) return false;

    File f = LittleFS.open("/tmp/app.lua.part", "w");
    uint8_t buf[256];
    WiFiClient* s = http.getStreamPtr();

    int total = http.getSize();
    if (total <= 0) total = 1;

    int downloaded = 0;
    chunkCount_Store = 0;
    totalBytes_Store = total;

    while (downloadProgress_Store < 100 || visualProgress_Store < 99.5) {
        if (s->available()) {
            int len = s->available();
            int r = s->readBytes(buf, MIN(len, sizeof(buf)));
            f.write(buf, r);
            downloaded += r;

            currentBytes_Store = downloaded;
            chunkCount_Store++;

            downloadProgress_Store = MIN((downloaded * 100) / total, 100);
        }

        drawInstalling_Store();

        if (!http.connected() && !s->available() && downloadProgress_Store < 100) {
            break;
        }

        delay(15);
        yield();
    }

    f.close();
    http.end();

    if (!checkLuaSyntax_File("/tmp/app.lua.part")) {
        setView_Store(VIEW_ERROR_Store);
        return false;
    }

    if (hasSensitiveAPI_File("/tmp/app.lua.part")) {
        sensitiveDetected_Store = true;
        pendingInstall_Store = true;
        pendinFile_Store = file;
        setView_Store(VIEW_WARNING_Store);
        return true;
    }

    LittleFS.rename("/tmp/app.lua.part", "/bin/" + file);

    delay(500);
    setView_Store(VIEW_LIST_Store);
    return true;
}

bool checkLuaSyntax_File(const char* path) {
    lua_State* L = luaL_newstate();

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    size_t len = f.size();
    char* buf = new char[len + 1];

    f.readBytes(buf, len);
    buf[len] = 0;
    f.close();

    bool ok = luaL_loadbuffer(L, buf, len, path) == LUA_OK;

    delete[] buf;
    lua_close(L);
    return ok;
}

bool hasSensitiveAPI_File(const char* path) {
    File f = LittleFS.open(path, "r");

    while (f.available()) {
        String l = f.readStringUntil('\n');
        if (l.indexOf("c3_http_get") >= 0) return true;
        if (l.indexOf("c3_set_cpu_clock") >= 0) return true;
        if (l.indexOf("c3_file") >= 0) return true;
    }

    return false;
}

void runStore() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(58, 22, 13, 12, image_store_1_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(40, 34, "Loading Store...");

    display.sendBuffer();

    delay(500);

    display.clearBuffer();
    display.drawXBM(58, 22, 13, 12, image_store_1_bits);

    display.drawStr(22, 43, "Connecting to WiFi...");

    display.sendBuffer();

    WiFiManager wm;
    wm.setConfigPortalTimeout(1);

    if (!wm.autoConnect("ESP32C3", "123456789")) {
        setView_Store(VIEW_WIFI_ERROR_Store);
    } else {
        fetchStore_Store(
            "https://raw.githubusercontent.com/Ardyanptr/C3OS-AppStore/refs/heads/main/apps.json");
        setView_Store(VIEW_LIST_Store);
    }

    exitStore_Store = false;
    static bool installing = false;

    while (!exitStore_Store) {
        if (needRedraw_Store) {
            switch (storeView_Store) {
                case VIEW_LIST_Store:
                    drawList_Store();
                    break;
                case VIEW_OVERVIEW_Store:
                    drawOverview_Store();
                    break;
                case VIEW_INSTALLING_Store:
                    drawInstalling_Store();
                    break;
                case VIEW_WIFI_ERROR_Store:
                    drawWiFiError_Store();
                    break;
                case VIEW_WARNING_Store:
                    drawWarning_Store();
                    break;
            }
            needRedraw_Store = false;
        }

        if (storeView_Store == VIEW_INSTALLING_Store && !installing) {
            installing = true;

            auto& app = storeItems_Store[selected_Store];
            downloadApp_Store(app.url, app.file);

            installing = false;
        }

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        delay(5);
        yield();
    }
}
