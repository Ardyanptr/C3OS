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

//

// Main Handler
void runLuaScript(const char* path) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "c3_print", l_c3_display_print);
    lua_register(L, "c3_cls", l_c3_display_cls);
    lua_register(L, "c3_update", l_c3_display_update);

    File file = LittleFS.open(path, "r");
    if (file) {
        String content = file.readString();
        file.close();

        if (luaL_dostring(L, content.c_str()) != LUA_OK) {
            Serial.println(lua_tostring(L, -1));
        }
    }

    lua_close(L);
}