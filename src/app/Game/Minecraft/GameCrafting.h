#ifndef GAME_CRAFTING_H
#define GAME_CRAFTING_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include "GameItem.h"
#include "GameInventory.h"

#define CRAFT_GRID 4   // 2x2

struct Recipe {
    uint8_t input[CRAFT_GRID];   // 2x2 grid, 0 = any/empty
    uint8_t outputId;
    uint8_t outputCount;
};

class GameCrafting {
public:
    void openGUI(U8G2* u8g2,
                 OneButton* btnL, OneButton* btnR,
                 OneButton* btnA, OneButton* btnOK,
                 GameInventory* inv);

private:
    ItemStack grid[CRAFT_GRID];   // 2x2 crafting input
    ItemStack output;

    static const Recipe recipes[];
    static const int    recipeCount;

    void checkRecipe();
    void drawCraftingGUI(U8G2* u8g2, GameInventory* inv, int cursor);
    bool matchRecipe(const Recipe& r);
};

#endif