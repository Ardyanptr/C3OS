#include "GameChest.h"
#include <esp_task_wdt.h>

// ─── Static state for OneButton callbacks ────────────────
namespace ChestCB {
    static int*          cursor   = nullptr;
    static bool*         inChest  = nullptr;
    static bool*         closed   = nullptr;
    static ChestData*    chest    = nullptr;
    static GameInventory* inv     = nullptr;

    static void onL() {
        if (!cursor || !inChest) return;
        int limit = *inChest ? CHEST_SLOTS : HOTBAR_SLOTS;
        *cursor = (*cursor - 1 + limit) % limit;
    }
    static void onR() {
        if (!cursor || !inChest) return;
        int limit = *inChest ? CHEST_SLOTS : HOTBAR_SLOTS;
        *cursor = (*cursor + 1) % limit;
    }
    static void onA() {
        if (!cursor || !inChest || !chest || !inv) return;
        if (*inChest) {
            ItemStack& cs = chest->slots[*cursor];
            if (!cs.isEmpty()) {
                int leftover = inv->addItem(cs.id, cs.count);
                cs.count = leftover;
                if (leftover == 0) cs.clear();
            }
        } else {
            ItemStack& is = inv->slots[*cursor];
            if (!is.isEmpty()) {
                for (int i = 0; i < CHEST_SLOTS; i++) {
                    if (chest->slots[i].isEmpty()) {
                        chest->slots[i] = is;
                        is.clear();
                        break;
                    }
                }
            }
        }
    }
    static void onALong() {
        if (!inChest || !cursor) return;
        *inChest = !(*inChest);
        *cursor  = 0;
    }
    static void onOK() {
        if (closed) *closed = true;
    }
}

// ─────────────────────────────────────────────────────────
void GameChest::generateLoot(ChestData& chest) {
    struct LootEntry { uint8_t id; int minC, maxC, chance; };
    static const LootEntry table[] = {
        { ITEM_COIN,    2, 8,  90 },
        { ITEM_TORCH,   1, 4,  70 },
        { ITEM_SWORD,   1, 1,  30 },
        { ITEM_PICKAXE, 1, 1,  40 },
        { ITEM_STONE,   3, 10, 60 },
        { ITEM_ORE,     1, 3,  40 },
        { ITEM_HEART,   1, 2,  25 },
        { ITEM_PLANK,   2, 6,  55 },
    };
    int filled = 0;
    for (auto& e : table) {
        if (filled >= CHEST_SLOTS) break;
        if (random(100) < e.chance) {
            chest.slots[filled].id    = e.id;
            chest.slots[filled].count = random(e.minC, e.maxC + 1);
            filled++;
        }
    }
}

void GameChest::placeChest(int wx, int wy, bool withLoot) {
    if (chestCount >= MAX_CHESTS) return;
    if (findChest(wx, wy) >= 0) return;
    ChestData& c = chests[chestCount];
    c.worldX = wx; c.worldY = wy; c.active = true;
    for (int i = 0; i < CHEST_SLOTS; i++) c.slots[i].clear();
    if (withLoot) generateLoot(c);
    chestCount++;
}

int GameChest::findChest(int wx, int wy) {
    for (int i = 0; i < chestCount; i++) {
        if (chests[i].active && chests[i].worldX == wx && chests[i].worldY == wy)
            return i;
    }
    return -1;
}

void GameChest::drawChestGUI(U8G2* u8g2, ChestData& chest,
                              GameInventory* inv, int cursor, bool inChest) {
    u8g2->clearBuffer();

    u8g2->drawBox(0, 0, 128, 10);
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_6x10_tr);
    u8g2->drawStr(2, 8, inChest ? "Chest" : "Inventory");
    u8g2->setDrawColor(1);
    u8g2->drawHLine(0, 10, 128);

    const int cSlotW = 12, cSlotH = 11;
    const int cStartX = 2, cStartY = 14;

    for (int i = 0; i < CHEST_SLOTS; i++) {
        int col = i % 5, row = i / 5;
        int x   = cStartX + col * (cSlotW + 1);
        int y   = cStartY + row * (cSlotH + 2);
        bool sel = (inChest && cursor == i);
        if (sel) { u8g2->drawBox(x-1, y-1, cSlotW+2, cSlotH+2); u8g2->setDrawColor(0); }
        u8g2->drawFrame(x, y, cSlotW, cSlotH);
        if (!chest.slots[i].isEmpty()) {
            u8g2->setFont(u8g2_font_4x6_tr);
            char buf[2] = { itemName(chest.slots[i].id)[0], 0 };
            u8g2->drawStr(x+3, y+8, buf);
            if (chest.slots[i].count > 1) {
                char cnt[3]; snprintf(cnt, 3, "%d", chest.slots[i].count);
                u8g2->drawStr(x+7, y+10, cnt);
            }
        }
        if (sel) u8g2->setDrawColor(1);
    }

    u8g2->drawVLine(70, 11, 52);

    const int iSlotW = 14, iSlotH = 12;
    const int iStartX = 74;
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        int y   = 13 + i * (iSlotH + 2);
        bool sel = (!inChest && cursor == i);
        if (sel) { u8g2->drawBox(iStartX-1, y-1, iSlotW+2, iSlotH+2); u8g2->setDrawColor(0); }
        u8g2->drawFrame(iStartX, y, iSlotW, iSlotH);
        if (!inv->slots[i].isEmpty()) {
            u8g2->setFont(u8g2_font_4x6_tr);
            char buf[2] = { itemName(inv->slots[i].id)[0], 0 };
            u8g2->drawStr(iStartX+3, y+9, buf);
        }
        if (sel) u8g2->setDrawColor(1);
    }

    u8g2->setFont(u8g2_font_4x6_tr);
    u8g2->drawStr(74, 56, "Bag");
    u8g2->drawStr(2, 62, "A:take LongA:swap OK:close");
    u8g2->drawStr(inChest ? 2 : 74, 11, ">");
    u8g2->sendBuffer();
}

void GameChest::openGUI(U8G2* u8g2,
                        OneButton* btnL, OneButton* btnR,
                        OneButton* btnA, OneButton* btnOK,
                        GameInventory* inv, int chestIndex) {
    ChestData& chest = chests[chestIndex];

    int  cursor  = 0;
    bool inChest = true;
    bool closed  = false;

    // Point static state to locals
    ChestCB::cursor  = &cursor;
    ChestCB::inChest = &inChest;
    ChestCB::closed  = &closed;
    ChestCB::chest   = &chest;
    ChestCB::inv     = inv;

    btnL->attachClick(ChestCB::onL);
    btnR->attachClick(ChestCB::onR);
    btnA->attachClick(ChestCB::onA);
    btnA->attachLongPressStart(ChestCB::onALong);
    btnOK->attachClick(ChestCB::onOK);

    while (!closed) {
        esp_task_wdt_reset();
        btnL->tick();
        btnR->tick();
        btnA->tick();
        btnOK->tick();
        drawChestGUI(u8g2, chest, inv, cursor, inChest);
        delay(10);
    }

    btnL->attachClick(nullptr);
    btnR->attachClick(nullptr);
    btnA->attachClick(nullptr);
    btnA->attachLongPressStart(nullptr);
    btnOK->attachClick(nullptr);

    ChestCB::cursor = nullptr;
    ChestCB::closed = nullptr;
}