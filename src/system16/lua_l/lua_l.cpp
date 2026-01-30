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
#include "icons/icon.h"
#include "system16/MemFusion/MemFusion.h"

static bool luaRunning = false;

portMUX_TYPE btnMux = portMUX_INITIALIZER_UNLOCKED;

// Start of panic
bool halt = true;

void lua_not_respond() {
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawRFrame(23, 12, 82, 39, 5);

    display.drawXBM(26, 15, 9, 8, image_Alert_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(38, 21, "Not Responding");

    display.drawStr(26, 30, "App doesn't respond");
    display.drawStr(26, 36, "Try opneing again!");

    display.setDrawColor(1);
    display.drawRFrame(26, 39, 24, 9, 3);

    display.drawStr(61, 46, "Exit");
    display.sendBuffer();

    btnOK.attachClick([]() {
        display.clearBuffer();
        display.sendBuffer();

        halt = false;
        delay(100);
    });

    while (halt) {
        delay(10);
        btnOK.tick();
    }

    drawMenu();
}

// End Of Panic

const char* lua_stream_reader(lua_State* L, void* data, size_t* size) {
    UARTLuaReader* reader = (UARTLuaReader*)data;
    static char buffer[128];

    if (reader->done) return NULL;

    unsigned long start = millis();
    while (Serial1.available() == 0) {
        if (millis() - start > 2000) {
            reader->done = true;
            return NULL;
        }
    }

    size_t i = 0;
    while (Serial1.available() > 0 && i < sizeof(buffer)) {
        char c = Serial1.read();

        if (c == 4) {
            reader->done = true;
        }
        buffer[i++] = c;
    }

    *size = i;
    return buffer;
}

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

int l_c3_drawRFrame(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    int r = luaL_checkinteger(L, 5);

    display.drawRFrame(x, y, w, h, r);
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
        case 4:
            display.setFont(u8g2_font_4x6_tr);
            break;
        default:
            display.setFont(u8g2_font_5x7_tr);
            break;
    }

    return 0;
}

int l_c3_drawBitmap(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    const char* name = luaL_checkstring(L, 5);

    const uint8_t* bitmapPointer = nullptr;

    if (strcmp(name, "image_Rpc_active_bits") == 0) {
        bitmapPointer = image_Rpc_active_bits;
    }

    if (bitmapPointer != nullptr) {
        display.drawXBM(x, y, w, h, bitmapPointer);
    } else {
        Serial.printf("[LUA/UI/ERROR]: Can't find bitmap %s!\n", name);
    }

    return 0;
}

int l_c3_print(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    Serial.println(msg);
    return 0;
}

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

int l_c3_file_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, LittleFS.exists(path));
    return 1;
}

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
    btnOK.attachLongPressStart([]() { btnOK_Event = BTN_LONG; });

    btnAction.attachLongPressStart([]() { btnAction_Event = BTN_LONG; });
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
        lua_pushinteger(L, 0);
        return 1;
    }

    BtnEvent out;
    portENTER_CRITICAL(&btnMux);
    out = *ev;
    *ev = BTN_NONE;
    portEXIT_CRITICAL(&btnMux);

    if (out != 0) Serial.printf("C++ sending event %d to Lua\n", out);

    lua_pushinteger(L, (int)out);
    return 1;
}

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
        DynamicJsonDocument doc(2048);
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
    lua_register(L, "c3_drawRFrame", l_c3_drawRFrame);
    lua_register(L, "c3_draw_box", l_c3_drawBox);
    lua_register(L, "c3_draw_line", l_c3_drawLine);
    lua_register(L, "c3_draw_hline", l_c3_drawHLine);
    lua_register(L, "c3_set_font", l_c3_setFont);
    lua_register(L, "c3_drawBitmap", l_c3_drawBitmap);

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

    String luaCode = swapPull(99);

    luaRunning = true;

    initButtons();

    portENTER_CRITICAL(&btnMux);
    btnUp_Event = BTN_NONE;
    btnDown_Event = BTN_NONE;
    btnOK_Event = BTN_NONE;
    btnAction_Event = BTN_NONE;
    portEXIT_CRITICAL(&btnMux);

    if (Settings::instance->get().memFusion == 0) {
        char fixedPath[128];
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

        int loadStatus = luaL_loadbuffer(L, buf, len, fixedPath);

        delete[] buf;

        if (loadStatus == LUA_OK) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                String err = lua_tostring(L, -1);
                lua_close(L);
                luaRunning = false;
                lua_not_respond();
            }
        } else {
            String err = lua_tostring(L, -1);
            lua_close(L);
            luaRunning = false;
            lua_not_respond();
        }

        lua_close(L);
        luaRunning = false;
    } else {
        Serial1.println("avr32:fusion-on:64");
        delay(100);  // Beri waktu ESP8266 untuk mount LittleFS

        File file = LittleFS.open(path, "r");
        if (file) {
            Serial1.println("mem:push:99:start");
            // Tunggu sampai ESP8266 menjawab STATUS:READY
            delay(50);

            while (file.available()) {
                String line = file.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) {
                    Serial1.print("mem:push:99:add:");
                    Serial1.println(line);
                    delay(5);  // Jeda sangat singkat
                }
            }
            file.close();
            Serial1.println("mem:push:99:end");

            unsigned long startWait = millis();
            while (millis() - startWait < 1000) {
                if (Serial1.available()) {
                    String res = Serial1.readStringUntil('\n');
                    if (res.indexOf("STATUS:PUSH_OK") != -1) break;
                }
            }
        }

        while (Serial1.available()) Serial1.read();
        Serial1.println("mem:pull:99");

        UARTLuaReader readerState = {false};

        if (lua_load(L, lua_stream_reader, &readerState, path, NULL) != LUA_OK) {
            Serial.printf("Streaming Load Error: %s\n", lua_tostring(L, -1));
        } else {
            if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
                Serial.printf("Exec Error: %s\n", lua_tostring(L, -1));
            }
        }
    }

    portENTER_CRITICAL(&btnMux);
    btnUp_Event = BTN_NONE;
    btnDown_Event = BTN_NONE;
    btnOK_Event = BTN_NONE;
    btnAction_Event = BTN_NONE;
    portEXIT_CRITICAL(&btnMux);
    luaRunning = false;

    delay(100);
}