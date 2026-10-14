-- note: append a note to /notes.txt, e.g. note buy milk
local text = arg[1] or ""
if text == "" then
    print("Usage: note <your text>")
    print("e.g.   note buy more RAM")
    return
end
local existing = c3_file_read("/notes.txt") or ""
existing = existing .. text .. "\n"
c3_file_write("/notes.txt", existing)
print("Note saved: " .. text)
print("Read it back with: type /notes.txt")