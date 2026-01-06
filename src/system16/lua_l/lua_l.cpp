#include "lua_l.h"

#define LUA_32BITS
#define LUA_C89_NUMBERS

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <OneButton.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

#include "button_state.h"
#include "esp_task_wdt.h"

// Guard
static bool luaRunning = false;

// MUX
portMUX_TYPE btnMux = portMUX_INITIALIZER_UNLOCKED;

// Display
int l_c3_display_print(lua_State* L) {
    const char* txt = luaL_checkstring(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);

    display.drawStr(x, y, txt);
    return 0;
}

int l_c3_display_cls(lua_State* L) {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tr);
    return 0;
}

int l_c3_display_update(lua_State* L) {
    display.sendBuffer();
    return 0;
}

int l_c3_drawFrame(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);

    display.drawFrame(x, y, w, h);
    return 0;
}

int l_c3_drawBox(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);

    display.drawBox(x, y, w, h);
    return 0;
}

int l_c3_drawLine(lua_State* L) {
    int x0 = luaL_checkinteger(L, 1);
    int y0 = luaL_checkinteger(L, 2);
    int x1 = luaL_checkinteger(L, 3);
    int y1 = luaL_checkinteger(L, 4);

    display.drawLine(x0, y0, x1, y1);
    return 0;
}

int l_c3_drawHLine(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);

    display.drawHLine(x, y, w);
    return 0;
}

int l_c3_setFont(lua_State* L) {
    int fontId = luaL_checkinteger(L, 1);

    switch (fontId) {
        case 1:
            display.setFont(u8g2_font_5x7_tr);
            break;
        case 2:
            display.setFont(u8g2_font_6x10_tf);
            break;
        case 3:
            display.setFont(u8g2_font_ncenB14_tr);
            break;
        default:
            display.setFont(u8g2_font_5x7_tr);
            break;
    }

    return 0;
}

// Print
int l_c3_print(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    Serial.println(msg);
    return 0;
}

// Power Managing
int l_c3_get_into_powersave(lua_State* L) {
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.mode(WIFI_OFF);
    btStop();

    return 0;
}

int l_c3_get_into_performance(lua_State* L) {
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.mode(WIFI_AP_STA);
    btStart();

    return 0;
}

int l_c3_set_cpu_clock(lua_State* L) {
    int mhz = luaL_checkinteger(L, 1);
    if (mhz >= 10 && mhz <= 160) {
        setCpuFrequencyMhz(mhz);
        return 0;
    }

    return 0;
}

// Storage
int l_c3_file_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, LittleFS.exists(path));
    return 1;
}

// Accessories
int l_c3_get_heap_kb(lua_State* L) {
    lua_pushinteger(L, ESP.getFreeHeap() / 1024);
    return 1;
}

int l_c3_get_heap(lua_State* L) {
    lua_pushinteger(L, ESP.getFreeHeap());
    return 1;
}

int l_c3_sleep(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    unsigned long t0 = millis();

    while (millis() - t0 < ms) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        esp_task_wdt_reset();
        vTaskDelay(1);
    }

    return 0;
}

// WiFi
int l_c3_http_get(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            lua_pushstring(L, payload.c_str());
        } else {
            lua_pushnil(L);
        }

        http.end();
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// Button

// Enum: 1 = click, 2 = double, 3 = long

void evUp() { btnUp_Event = BTN_CLICK; }
void evDown() { btnDown_Event = BTN_CLICK; }
void evOK() { btnOK_Event = BTN_CLICK; }
void evAction() { btnAction_Event = BTN_CLICK; }

void initButtons() {
    btnOK.setPressTicks(800);
    btnOK.setClickTicks(500);

    btnUp.attachClick(evUp);
    btnDown.attachClick(evDown);
    btnOK.attachClick(evOK);
    btnAction.attachClick(evAction);

    btnOK.attachDoubleClick([]() { btnOK_Event = BTN_DOUBLE; });
    btnOK.attachLongPressStop([]() { btnOK_Event = BTN_LONG; });

    btnAction.attachLongPressStop([]() { btnAction_Event = BTN_LONG; });
}

int l_c3_btn(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    volatile BtnEvent* ev = nullptr;

    if (!strcmp(name, "up"))
        ev = &btnUp_Event;
    else if (!strcmp(name, "down"))
        ev = &btnDown_Event;
    else if (!strcmp(name, "ok"))
        ev = &btnOK_Event;
    else if (!strcmp(name, "action"))
        ev = &btnAction_Event;

    if (!ev) {
        lua_pushinteger(L, BTN_NONE);
        return 1;
    }

    BtnEvent out;
    portENTER_CRITICAL(&btnMux);
    out = *ev;
    *ev = BTN_NONE;
    portEXIT_CRITICAL(&btnMux);

    lua_pushinteger(L, out);
    return 1;
}

// Weather
int l_c3_get_weather(lua_State* L) {
    WiFiManager wm;

    display.clearBuffer();
    display.sendBuffer();

    unsigned long t0 = millis();

    while (!wm.autoConnect("ESP32C3", "123456789")) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();
        delay(1);

        if (millis() - t0 > 10000) break;
    }

    const char* city = luaL_checkstring(L, 1);
    const char* apiKey = "4fbfd2ca772680021b44841cc923d442";
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + ",ID&units=metric&appid=" + String(apiKey);

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, http.getStream())) {
            http.end();
            lua_pushnil(L);
            return 1;
        }

        lua_newtable(L);

        lua_pushstring(L, "temp");
        lua_pushnumber(L, doc["main"]["temp"]);
        lua_settable(L, -3);

        lua_pushstring(L, "desc");
        lua_pushstring(L, doc["weather"][0]["description"]);
        lua_settable(L, -3);

        lua_pushstring(L, "hum");
        lua_pushinteger(L, doc["main"]["humidity"]);
        lua_settable(L, -3);

        http.end();
        return 1;
    }

    http.end();
    return 0;
}

// Main Handler
void runLuaScript(const char* path) {
    if (luaRunning) return;
    luaRunning = true;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "c3_print", l_c3_display_print);
    lua_register(L, "c3_cls", l_c3_display_cls);
    lua_register(L, "c3_update", l_c3_display_update);
    lua_register(L, "c3_draw_frame", l_c3_drawFrame);
    lua_register(L, "c3_draw_box", l_c3_drawBox);
    lua_register(L, "c3_draw_line", l_c3_drawLine);
    lua_register(L, "c3_draw_hline", l_c3_drawHLine);
    lua_register(L, "c3_set_font", l_c3_setFont);

    lua_register(L, "print", l_c3_print);

    lua_register(L, "c3_get_into_powersave", l_c3_get_into_powersave);
    lua_register(L, "c3_get_into_performance", l_c3_get_into_performance);
    lua_register(L, "c3_set_cpu_clock", l_c3_set_cpu_clock);

    lua_register(L, "c3_get_heap_kb", l_c3_get_heap_kb);
    lua_register(L, "c3_get_heap", l_c3_get_heap);
    lua_register(L, "c3_sleep", l_c3_sleep);
    lua_register(L, "c3_btn", l_c3_btn);

    lua_register(L, "c3_file_exists", l_c3_file_exists);

    lua_register(L, "c3_http_get", l_c3_http_get);
    lua_register(L, "c3_get_weather", l_c3_get_weather);

    char fixedPath[64];
    if (path[0] != '/')
        snprintf(fixedPath, sizeof(fixedPath), "/%s", path);
    else
        strncpy(fixedPath, path, sizeof(fixedPath) - 1);

    File file = LittleFS.open(fixedPath, "r");
    if (!file) {
        Serial.printf("Lua file not found! %s\n", fixedPath);
        return;
    }

    size_t len = file.size();
    char* buf = new char[len + 1];
    file.readBytes(buf, len);
    buf[len] = 0;
    file.close();

    if (luaL_loadbuffer(L, buf, len, fixedPath) || lua_pcall(L, 0, 0, 0)) {
        panic(PANIC_LUA_ERROR, lua_tostring(L, -1));
    }

    lua_close(L);
    delete[] buf;

    luaRunning = false;
}