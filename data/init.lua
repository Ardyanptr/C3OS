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

local function load_config()
    local config_path = "/cfg/user_prefs.lua"
    if c3_file_exists(config_path) then
        print("[INIT/CONFIG] Found user preferences!")
    else
        print("[INIT/CONFIG] No user preferences found!")
    end
end

print("-----------------------")
load_config()

sys.gc()
print(string.format("[C3OS-LUA] Initialization Complete (%.2f KB Used)", sys.mem()))
print("-----------------------\n")