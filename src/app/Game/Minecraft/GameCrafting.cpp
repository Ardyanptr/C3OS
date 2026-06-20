#include "GameCrafting.h"
#include <esp_task_wdt.h>

static GameCrafting* _craftInstance = nullptr;
static int* _cursorPtr = nullptr;
static int _TOTAL_VAL = 8;
static bool* _closedPtr = nullptr;
static GameInventory** _invPtr = nullptr;

// ─── Recipe Table ─────────────────────────────────────────
// 2x2 grid, row-major. 0 = wildcard/empty
// [0][1]
// [2][3]
const Recipe GameCrafting::recipes[] = {
    // Planks: 1 wood → 4 planks
    { {ITEM_WOOD, 0, 0, 0},           ITEM_PLANK,   4 },
    // Sticks: 2 planks vertical → 4 sticks
    { {ITEM_PLANK, 0, ITEM_PLANK, 0}, ITEM_STICK,   4 },
    // Sword: 2 stone + 1 stick
    { {ITEM_STONE, 0, ITEM_STONE, 0}, ITEM_SWORD,   1 },  // simplified
    // Pickaxe: stone top row + stick
    { {ITEM_STONE, ITEM_STONE, 0, ITEM_STICK}, ITEM_PICKAXE, 1 },
    // Axe: stone + stick diagonal
    { {ITEM_STONE, 0, ITEM_STICK, 0}, ITEM_AXE,     1 },
    // Torch: ore + stick
    { {ITEM_ORE, 0, ITEM_STICK, 0},   ITEM_TORCH,   4 },
    // Chest: 4 planks
    { {ITEM_PLANK, ITEM_PLANK, ITEM_PLANK, ITEM_PLANK}, ITEM_CHEST, 1 },
};
const int GameCrafting::recipeCount = sizeof(GameCrafting::recipes) / sizeof(Recipe);

bool GameCrafting::matchRecipe(const Recipe& r) {
    for (int i = 0; i < CRAFT_GRID; i++) {
        uint8_t need = r.input[i];
        if (need == ITEM_NONE) continue;  // wildcard — skip
        if (grid[i].isEmpty() || grid[i].id != need) return false;
    }
    return true;
}

void GameCrafting::checkRecipe() {
    output.clear();
    for (int i = 0; i < recipeCount; i++) {
        if (matchRecipe(recipes[i])) {
            output.id    = recipes[i].outputId;
            output.count = recipes[i].outputCount;
            return;
        }
    }
}

void GameCrafting::drawCraftingGUI(U8G2* u8g2, GameInventory* inv, int cursor) {
    u8g2->clearBuffer();

    // Title bar
    u8g2->drawBox(0, 0, 128, 10);
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_6x10_tr);
    u8g2->drawStr(2, 8, "Crafting");
    u8g2->setDrawColor(1);
    u8g2->drawHLine(0, 10, 128);

    // 2x2 grid (left side)
    const int gSlotW = 16, gSlotH = 14;
    const int gStartX = 8, gStartY = 14;

    for (int i = 0; i < CRAFT_GRID; i++) {
        int col = i % 2, row = i / 2;
        int x   = gStartX + col * (gSlotW + 2);
        int y   = gStartY + row * (gSlotH + 2);

        bool sel = (cursor == i);
        if (sel) {
            u8g2->drawBox(x - 1, y - 1, gSlotW + 2, gSlotH + 2);
            u8g2->setDrawColor(0);
        }
        u8g2->drawFrame(x, y, gSlotW, gSlotH);
        if (!grid[i].isEmpty()) {
            u8g2->setFont(u8g2_font_5x8_tr);
            char buf[2] = { itemName(grid[i].id)[0], 0 };
            u8g2->drawStr(x + 5, y + 10, buf);
            if (grid[i].count > 1) {
                u8g2->setFont(u8g2_font_4x6_tr);
                char cnt[3]; snprintf(cnt, 3, "%d", grid[i].count);
                u8g2->drawStr(x + 11, y + 13, cnt);
            }
        }
        if (sel) u8g2->setDrawColor(1);
    }

    // Arrow →
    u8g2->setFont(u8g2_font_6x10_tr);
    u8g2->drawStr(50, 26, ">");

    // Output slot
    {
        int ox = 60, oy = 17;
        bool sel = (cursor == 4);  // slot 4 = output
        if (sel) {
            u8g2->drawBox(ox - 1, oy - 1, gSlotW + 2, gSlotH + 2);
            u8g2->setDrawColor(0);
        }
        u8g2->drawFrame(ox, oy, gSlotW, gSlotH);
        if (!output.isEmpty()) {
            u8g2->setFont(u8g2_font_5x8_tr);
            char buf[2] = { itemName(output.id)[0], 0 };
            u8g2->drawStr(ox + 5, oy + 10, buf);
            if (output.count > 1) {
                u8g2->setFont(u8g2_font_4x6_tr);
                char cnt[3]; snprintf(cnt, 3, "%d", output.count);
                u8g2->drawStr(ox + 11, oy + 13, cnt);
            }
        }
        if (sel) u8g2->setDrawColor(1);
    }

    // Output label
    u8g2->setFont(u8g2_font_4x6_tr);
    if (!output.isEmpty()) {
        u8g2->drawStr(60, 37, itemName(output.id));
        char cnt[16]; snprintf(cnt, 16, "x%d", output.count);
        u8g2->drawStr(60, 43, cnt);
    } else {
        u8g2->drawStr(60, 37, "No recipe");
    }

    // Divider
    u8g2->drawVLine(82, 11, 52);

    // Inventory hotbar preview (right side, 3 slots vertical)
    const int iSlotW = 13, iSlotH = 11;
    const int iStartX = 86;
    u8g2->setFont(u8g2_font_4x6_tr);
    u8g2->drawStr(86, 12, "Bag");
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        int y   = 14 + i * (iSlotH + 2);
        bool sel = (cursor == 5 + i);
        if (sel) {
            u8g2->drawBox(iStartX - 1, y - 1, iSlotW + 2, iSlotH + 2);
            u8g2->setDrawColor(0);
        }
        u8g2->drawFrame(iStartX, y, iSlotW, iSlotH);
        if (!inv->slots[i].isEmpty()) {
            u8g2->setFont(u8g2_font_4x6_tr);
            char buf[2] = { itemName(inv->slots[i].id)[0], 0 };
            u8g2->drawStr(iStartX + 3, y + 8, buf);
        }
        if (sel) u8g2->setDrawColor(1);
    }

    // Controls
    u8g2->setFont(u8g2_font_4x6_tr);
    u8g2->drawStr(2, 56, "L/R:move A:place OK:craft/close");

    u8g2->sendBuffer();
}

void GameCrafting::openGUI(U8G2* u8g2,
                           OneButton* btnL, OneButton* btnR,
                           OneButton* btnA, OneButton* btnOK,
                           GameInventory* inv) {
    // 1. Inisialisasi variabel lokal
    for (int i = 0; i < CRAFT_GRID; i++) grid[i].clear();
    output.clear();

    static int localCursor = 0; // pakai static agar aman dalam callback
    static bool localClosed = false;
    static GameInventory* localInv = inv;
    localCursor = 0;
    localClosed = false;
    localInv = inv;

    // 2. Set static pointers agar bisa diakses lambda []
    _craftInstance = this;
    _cursorPtr = &localCursor;
    _closedPtr = &localClosed;
    _invPtr = &localInv;

    // 3. Callback Button
    btnL->attachClick([]() {
        *_cursorPtr = (*_cursorPtr - 1 + _TOTAL_VAL) % _TOTAL_VAL;
    });

    btnR->attachClick([]() {
        *_cursorPtr = (*_cursorPtr + 1) % _TOTAL_VAL;
    });

    btnA->attachClick([]() {
        int& cur = *_cursorPtr;
        GameInventory* invRef = *_invPtr;
        
        if (cur >= 5) {
            int invIdx = cur - 5;
            if (!invRef->slots[invIdx].isEmpty()) {
                for (int i = 0; i < CRAFT_GRID; i++) {
                    if (_craftInstance->grid[i].isEmpty()) {
                        _craftInstance->grid[i] = invRef->slots[invIdx];
                        invRef->slots[invIdx].clear();
                        break;
                    }
                }
            }
        } else if (cur < CRAFT_GRID) {
            if (!_craftInstance->grid[cur].isEmpty()) {
                invRef->addItem(_craftInstance->grid[cur].id, _craftInstance->grid[cur].count);
                _craftInstance->grid[cur].clear();
            }
        }
        _craftInstance->checkRecipe();
    });

    btnOK->attachClick([]() {
        int& cur = *_cursorPtr;
        GameInventory* invRef = *_invPtr;

        if (cur == 4 && !_craftInstance->output.isEmpty()) {
            for (int i = 0; i < CRAFT_GRID; i++) {
                if (!_craftInstance->grid[i].isEmpty()) {
                    _craftInstance->grid[i].count--;
                    if (_craftInstance->grid[i].count == 0) _craftInstance->grid[i].clear();
                }
            }
            invRef->addItem(_craftInstance->output.id, _craftInstance->output.count);
            _craftInstance->checkRecipe();
        } else {
            for (int i = 0; i < CRAFT_GRID; i++) {
                if (!_craftInstance->grid[i].isEmpty()) {
                    invRef->addItem(_craftInstance->grid[i].id, _craftInstance->grid[i].count);
                    _craftInstance->grid[i].clear();
                }
            }
            *_closedPtr = true;
        }
    });

    // 4. Main Loop
    while (!localClosed) {
        esp_task_wdt_reset();
        btnL->tick();
        btnR->tick();
        btnA->tick();
        btnOK->tick();
        drawCraftingGUI(u8g2, localInv, localCursor);
        delay(10);
    }

    // Cleanup agar tidak sengaja terpanggil di luar menu ini
    btnL->attachClick(nullptr);
    btnR->attachClick(nullptr);
    btnA->attachClick(nullptr);
    btnOK->attachClick(nullptr);
}