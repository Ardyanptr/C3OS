#ifndef GAME_ITEM_H
#define GAME_ITEM_H

#include <Arduino.h>

// ─── Item IDs ────────────────────────────────────────────
#define ITEM_NONE       0
#define ITEM_DIRT       1
#define ITEM_GRASS      2
#define ITEM_STONE      3
#define ITEM_WOOD       4
#define ITEM_LEAVES     5
#define ITEM_ORE        6
#define ITEM_CHEST      7
#define ITEM_TORCH      8
#define ITEM_SWORD      9
#define ITEM_PICKAXE    10
#define ITEM_AXE        11
#define ITEM_COIN       12
#define ITEM_HEART      13
#define ITEM_PLANK      14
#define ITEM_STICK      15

#define ITEM_COUNT      16
#define INV_SLOTS       9   // 3 hotbar + 6 bag
#define HOTBAR_SLOTS    3
#define CHEST_SLOTS     10  // 2x5

struct ItemStack {
    uint8_t id    = ITEM_NONE;
    uint8_t count = 0;

    bool isEmpty() const { return id == ITEM_NONE || count == 0; }
    void clear()         { id = ITEM_NONE; count = 0; }
};

// Item display names
inline const char* itemName(uint8_t id) {
    switch (id) {
        case ITEM_DIRT:    return "Dirt";
        case ITEM_GRASS:   return "Grass";
        case ITEM_STONE:   return "Stone";
        case ITEM_WOOD:    return "Wood";
        case ITEM_LEAVES:  return "Leaves";
        case ITEM_ORE:     return "Ore";
        case ITEM_CHEST:   return "Chest";
        case ITEM_TORCH:   return "Torch";
        case ITEM_SWORD:   return "Sword";
        case ITEM_PICKAXE: return "Pickaxe";
        case ITEM_AXE:     return "Axe";
        case ITEM_COIN:    return "Coin";
        case ITEM_HEART:   return "Heart";
        case ITEM_PLANK:   return "Plank";
        case ITEM_STICK:   return "Stick";
        default:           return "???";
    }
}

// Block ID to item ID mapping
inline uint8_t blockToItem(uint8_t blockId) {
    switch (blockId) {
        case 1: return ITEM_STONE;
        case 2: return ITEM_GRASS;
        case 3: return ITEM_DIRT;
        case 4: return ITEM_WOOD;
        case 5: return ITEM_LEAVES;
        case 6: return ITEM_ORE;
        case 7: return ITEM_CHEST;
        default: return ITEM_NONE;
    }
}

// Is item a weapon?
inline bool isWeapon(uint8_t id) {
    return id == ITEM_SWORD || id == ITEM_PICKAXE || id == ITEM_AXE;
}

// Weapon damage
inline int weaponDamage(uint8_t id) {
    switch (id) {
        case ITEM_SWORD:   return 3;
        case ITEM_AXE:     return 2;
        case ITEM_PICKAXE: return 1;
        default:           return 1; // fist
    }
}

// Is item placeable as block?
inline bool isPlaceable(uint8_t id) {
    return id >= ITEM_DIRT && id <= ITEM_CHEST;
}

// Item to block ID
inline uint8_t itemToBlock(uint8_t id) {
    switch (id) {
        case ITEM_DIRT:   return 3;
        case ITEM_GRASS:  return 2;
        case ITEM_STONE:  return 1;
        case ITEM_WOOD:   return 4;
        case ITEM_LEAVES: return 5;
        case ITEM_ORE:    return 6;
        case ITEM_CHEST:  return 7;
        default:          return 0;
    }
}

#endif