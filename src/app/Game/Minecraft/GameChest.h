#ifndef GAME_CHEST_H
#define GAME_CHEST_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include "GameItem.h"
#include "GameInventory.h"

#define MAX_CHESTS 8

struct ChestData {
    int       worldX;
    int       worldY;
    ItemStack slots[CHEST_SLOTS];
    bool      active = false;
};

class GameChest {
public:
    ChestData chests[MAX_CHESTS];
    int       chestCount = 0;

    // Register a chest at world position, optionally pre-fill loot
    void placeChest(int wx, int wy, bool withLoot = false);

    // Find chest at world pos, returns index or -1
    int  findChest(int wx, int wy);

    // Open chest GUI (blocking), player can move items to/from inventory
    void openGUI(U8G2* u8g2,
                 OneButton* btnL, OneButton* btnR,
                 OneButton* btnA, OneButton* btnOK,
                 GameInventory* inv, int chestIndex);

private:
    void drawChestGUI(U8G2* u8g2, ChestData& chest,
                      GameInventory* inv, int cursor, bool inChest);
    void generateLoot(ChestData& chest);
};

#endif