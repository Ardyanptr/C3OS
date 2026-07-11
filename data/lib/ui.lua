---@diagnositc disable: undefined-global

local ui = {}

function ui.message(text)
    c3_cls()
    c3_print(text, 0, 0)
    c3_update()
end

function ui.progress(text1, text2, progress)
    c3_cls()
    c3_drawRFrame(22, 13, 84, 39, 3)
    c3_drawRFrame(27, 41, 74, 7, 2)
    c3_set_font(4)

    c3_print("Hang On", 27, 23)

    c3_print(text1, 27, 30)
    c3_print(text2, 27, 36)

    c3_drawRBox(27, 41, map(progress, 0, 100, 0, 73), 7, 2)
    c3_update()
end

return ui