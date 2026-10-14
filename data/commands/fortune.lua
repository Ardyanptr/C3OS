-- fortune: print a random quote
local quotes = {
    "The best thing about a boolean is even if you're wrong, you're only off by a bit.",
    "There are only 10 types of people: those who understand binary and those who don't.",
    "Hardware bugs are like a box of chocolates... you never know what you're gonna get.",
    "A processor is a device for executing instructions. Sarcasm is optional.",
    "In C we trust, everything else is just a pointer.",
    "I would love to change the world, but they don't supply the source code."
}
math.randomseed(os.time())
local pick = quotes[math.random(#quotes)]
print(pick)
c3_cls()
c3_update()
c3_set_font(2)
c3_print("Fortune", 0, 10)
c3_set_font(4)
c3_print(pick, 0, 25)
c3_update()