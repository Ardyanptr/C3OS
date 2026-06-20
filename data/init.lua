---@diagnostic disable: undefined-global

_OS_VERSION = "1.1.0b"
print("\n[C3OS-LUA] System Link Initialized")
print("C3OS Version: " .. _OS_VERSION .. "\n")
collectgarbage("incremental", 110, 200)
sys = {
    mem = function() return collectgarbage("count") end,
    gc = function() collectgarbage("generational") end,
    explore = function()
        if runFileManager then
            runFileManager()
        else
            print("Error: FM Not Linked Properly")
        end
    end
}
print("-----------------------")
c3_cls()
c3_update()
c3_drawRFrame(23, 13, 83, 40, 3)
c3_drawBitmap(28, 17, 7, 8, "image_Rpc_active_bits")
c3_set_font(1)
c3_print("Init...", 37, 24)
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
        print(
            "[INIT/CONFIG] No user preferences found!")
    end
end
c3_draw_line(28, 45, 65, 45)
c3_update()
local checkFile_ = c3_file_exists("/bin")
if checkFile_ then print("[INIT/BIN] Found bin folder!") end
load_config()
c3_draw_line(28, 45, 100, 45); c3_update()
c3_sleep(100)
print("[LUA/PROC] Initializing Background Process")
local rc_path = "/systemd/rclocal.lua"
print("[LUA/PROC] Finding RC Local...")
if c3_file_exists(rc_path) then
    print("[LUA/RCL] Found RC Local!")
    local code = c3_file_read(rc_path)
    if code then
        print("[RD/LUA/RCL] Executing RC Local...")
        local run_rc, err = load(code)
        if run_rc then
            print("[INIT] Executing RC Local...")
            pcall(run_rc)
        else
            print("[INIT/ERR] Syntax error in rclocal: " .. tostring(err))
        end
    end
else
    print("[INIT/WARN] rclocal.lua not found, skipping services!")
end
sys.gc()
print(string.format("[C3OS-LUA] Initialization Complete (%.2f KB Used)", sys.mem()))
print("-----------------------\n")
