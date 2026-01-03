#include "lua_l.h"

#define LUA_32BITS
#define LUA_C89_NUMBERS

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

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

int l_c3_drawLine(lua_State* L) {
    int x0 = luaL_checkinteger(L, 1);
    int y0 = luaL_checkinteger(L, 2);
    int x1 = luaL_checkinteger(L, 3);
    int y1 = luaL_checkinteger(L, 4);

    display.drawLine(x0, y0, x1, y1);
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

int l_c3_sleep(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

// Main Handler
void runLuaScript(const char* path) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "c3_print", l_c3_display_print);
    lua_register(L, "c3_cls", l_c3_display_cls);
    lua_register(L, "c3_update", l_c3_display_update);
    lua_register(L, "c3_draw_frame", l_c3_drawFrame);
    lua_register(L, "c3_draw_line", l_c3_drawLine);
    lua_register(L, "c3_set_font", l_c3_setFont);

    lua_register(L, "print", l_c3_print);

    lua_register(L, "c3_get_into_powersave", l_c3_get_into_powersave);
    lua_register(L, "c3_get_into_performance", l_c3_get_into_performance);
    lua_register(L, "c3_set_cpu_clock", l_c3_set_cpu_clock);

    lua_register(L, "c3_get_heap_kb", l_c3_get_heap_kb);
    lua_register(L, "c3_sleep", l_c3_sleep);

    lua_register(L, "c3_file_exists", l_c3_file_exists);

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
        Serial.printf("Lua Error: %s\n", lua_tostring(L, -1));
    }

    lua_close(L);
    delete[] buf;
}