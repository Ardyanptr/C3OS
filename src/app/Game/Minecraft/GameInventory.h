#ifndef GAME_INVENTORY_H
#define GAME_INVENTORY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include "GameItem.h"

class GameInventory {
public:
    ItemStack slots[INV_SLOTS];   // 0..2 = hotbar, 3..8 = bag
    int       hotbarIndex = 0;    // active hotbar slot

    GameInventory();

    // Add item, returns leftover count (0 = all added)
    int  addItem(uint8_t id, int count = 1);
    bool removeItem(uint8_t id, int count = 1);
    bool hasItem(uint8_t id, int count = 1) const;

    ItemStack& activeSlot() { return slots[hotbarIndex]; }

    // Hotbar navigation
    void nextHotbar();
    void prevHotbar();

    // Draw hotbar (bottom of screen, always visible)
    void drawHotbar(U8G2* u8g2);

    // Draw full inventory GUI (blocking, returns when closed)
    // returns false if player pressed close
    bool openGUI(U8G2* u8g2, OneButton* btnL, OneButton* btnR,
                 OneButton* btnA, OneButton* btnOK);

private:
    void drawFullGUI(U8G2* u8g2, int cursor);
};

#endif