--[[

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

]]--

---@diagnostic disable: undefined-global

_OS_VERSION = "1.1.0b"

print("\n[C3OS-LUA] System Link Initialized")
print("C3OS Version: " .. _OS_VERSION .. "\n")

collectgarbage("incremental", 110, 200)

sys = {
    mem = function()
        return collectgarbage("count")
    end,

    gc = function()
        collectgarbage("generational")
    end,

    explore = function()
        if runFileManager then runFileManager() else print("Error: FM Not Linked Properly") end
    end
}

print("-----------------------")

c3_cls()
c3_update()

c3_drawRFrame(23, 13, 83, 40, 3)
c3_drawBitmap(28, 17, 7, 8, "image_Rpc_active_bits")

c3_set_font(1)
c3_print("Initial Setup", 37, 24)

c3_draw_frame(27, 44, 75, 3)
c3_update()

c3_set_font(4)
c3_print("Prep:", 27, 40)
c3_print("User Data...", 47, 40)

c3_update()

local function load_config()
    local config_path = "/cfg/user_prefs.lua"
    if c3_file_exists(config_path) then
        print("[INIT/CONFIG] Found user preferences!")
    else
        print("[INIT/CONFIG] No user preferences found!")
    end
end

c3_draw_line(28, 45, 65, 45)
c3_update()

c3_sleep(2500)

local checkFile_ = c3_file_exists("/bin")
if checkFile_ then
    print("[INIT/BIN] Found bin folder!")
end

load_config()

c3_draw_line(28, 45, 100, 45);
c3_update()

c3_sleep(100)

sys.gc()
print(string.format("[C3OS-LUA] Initialization Complete (%.2f KB Used)", sys.mem()))
print("-----------------------\n")