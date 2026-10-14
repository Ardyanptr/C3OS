-- hello: greet someone, e.g. hello Ardyan
c3_cls()
c3_update()
local who = arg[1] or "World"
print("Hello, " .. who .. "!")
c3_set_font(2)
c3_print("Hello,", 0, 10)
c3_print(tostring(who) .. "!", 10, 25)
c3_update()