---@diagnostic disable: undefined-global

troops = {}

function troops.check_system()
    local essential_files = {
        "/cfg/update.json",
        "/security/troops.lua",
        "/init.lua"
    }

    for _, f in ipairs(essential_files) do
        if not c3_file_exists(f) then
            print("[TROOPS] CRITICAL: " .. f .. " component is missing")
            return false
        end
    end

    if c3_get_heap_kb() < 10 then
        print("[TROOPS] CRITICAL: Not enough RAM!")
        return false
    end

    return true
end

print("[TROOPS] Guardian Loaded")