#include "GameInventory.h"
#include <esp_task_wdt.h>

GameInventory::GameInventory() {
    for (int i = 0; i < INV_SLOTS; i++) slots[i].clear();
}

int GameInventory::addItem(uint8_t id, int count) {
    for (int i = 0; i < INV_SLOTS && count > 0; i++) {
        if (slots[i].id == id && slots[i].count < 99) {
            int add = min(count, 99 - (int)slots[i].count);
            slots[i].count += add; count -= add;
        }
    }
    for (int i = 0; i < INV_SLOTS && count > 0; i++) {
        if (slots[i].isEmpty()) {
            slots[i].id = id; slots[i].count = min(count, 99);
            count -= slots[i].count;
        }
    }
    return count;
}

bool GameInventory::removeItem(uint8_t id, int count) {
    if (!hasItem(id, count)) return false;
    for (int i = 0; i < INV_SLOTS && count > 0; i++) {
        if (slots[i].id == id) {
            int take = min((int)slots[i].count, count);
            slots[i].count -= take; count -= take;
            if (slots[i].count == 0) slots[i].clear();
        }
    }
    return true;
}

bool GameInventory::hasItem(uint8_t id, int count) const {
    int total = 0;
    for (int i = 0; i < INV_SLOTS; i++)
        if (slots[i].id == id) total += slots[i].count;
    return total >= count;
}

void GameInventory::nextHotbar() { hotbarIndex = (hotbarIndex + 1) % HOTBAR_SLOTS; }
void GameInventory::prevHotbar() { hotbarIndex = (hotbarIndex - 1 + HOTBAR_SLOTS) % HOTBAR_SLOTS; }

// ─── Hotbar draw ─────────────────────────────────────────
void GameInventory::drawHotbar(U8G2* u8g2) {
    const int slotW = 14, slotH = 12;
    const int startX = (128 - HOTBAR_SLOTS * (slotW + 2)) / 2;
    const int y = 64 - slotH - 1;

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        int x = startX + i * (slotW + 2);
        if (i == hotbarIndex) {
            u8g2->drawFrame(x-1, y-1, slotW+2, slotH+2);
            u8g2->drawFrame(x,   y,   slotW,   slotH);
        } else {
            u8g2->drawFrame(x, y, slotW, slotH);
        }
        if (!slots[i].isEmpty()) {
            u8g2->setFont(u8g2_font_4x6_tr);
            char buf[2] = { itemName(slots[i].id)[0], 0 };
            u8g2->drawStr(x+3, y+8, buf);
            if (slots[i].count > 1) {
                char cnt[4]; snprintf(cnt, 4, "%d", slots[i].count);
                u8g2->drawStr(x+8, y+11, cnt);
            }
        }
    }
}

// ─── Full GUI static state ────────────────────────────────
namespace InvCB {
    static int*           cursor   = nullptr;
    static bool*          closed   = nullptr;
    static GameInventory* self     = nullptr;

    static void onL() { if (cursor) *cursor = (*cursor - 1 + INV_SLOTS) % INV_SLOTS; }
    static void onR() { if (cursor) *cursor = (*cursor + 1) % INV_SLOTS; }
    static void onA() {
        // Drop one from cursor slot
        if (!self || !cursor) return;
        if (!self->slots[*cursor].isEmpty()) {
            self->slots[*cursor].count--;
            if (self->slots[*cursor].count == 0) self->slots[*cursor].clear();
        }
    }
    static void onOK() {
        if (!self || !cursor || !closed) return;
        if (*cursor < HOTBAR_SLOTS) self->hotbarIndex = *cursor;
        *closed = true;
    }
}

void GameInventory::drawFullGUI(U8G2* u8g2, int cursor) {
    u8g2->clearBuffer();

    u8g2->drawBox(0, 0, 128, 10);
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_6x10_tr);
    u8g2->drawStr(2, 8, "Inventory");
    u8g2->setDrawColor(1);
    u8g2->drawHLine(0, 10, 128);

    const int slotW = 16, slotH = 13;
    for (int i = 0; i < INV_SLOTS; i++) {
        int x, y;
        if (i < 3) {
            x = 4 + i * (slotW + 4); y = 14;
        } else {
            int bi = i - 3;
            x = 4 + (bi % 3) * (slotW + 4);
            y = (bi < 3) ? 29 : 44;
        }

        bool sel = (i == cursor);
        if (sel) { u8g2->drawBox(x-1, y-1, slotW+2, slotH+2); u8g2->setDrawColor(0); }
        u8g2->drawFrame(x, y, slotW, slotH);
        if (!slots[i].isEmpty()) {
            u8g2->setFont(u8g2_font_5x8_tr);
            char buf[2] = { itemName(slots[i].id)[0], 0 };
            u8g2->drawStr(x+4, y+9, buf);
            if (slots[i].count > 1) {
                u8g2->setFont(u8g2_font_4x6_tr);
                char cnt[4]; snprintf(cnt, 4, "%d", slots[i].count);
                u8g2->drawStr(x+10, y+12, cnt);
            }
        }
        if (sel) u8g2->setDrawColor(1);
    }

    u8g2->setFont(u8g2_font_4x6_tr);
    u8g2->drawStr(68, 20, slots[cursor].isEmpty() ? "(empty)" : itemName(slots[cursor].id));
    u8g2->drawStr(68, 30, "L/R:move");
    u8g2->drawStr(68, 37, "A:drop");
    u8g2->drawStr(68, 44, "OK:close");
    u8g2->drawStr(4, 12, "Hotbar");
    u8g2->sendBuffer();
}

bool GameInventory::openGUI(U8G2* u8g2, OneButton* btnL, OneButton* btnR,
                             OneButton* btnA, OneButton* btnOK) {
    int  cursor = hotbarIndex;
    bool closed = false;

    InvCB::cursor = &cursor;
    InvCB::closed = &closed;
    InvCB::self   = this;

    btnL->attachClick(InvCB::onL);
    btnR->attachClick(InvCB::onR);
    btnA->attachClick(InvCB::onA);
    btnOK->attachClick(InvCB::onOK);

    while (!closed) {
        esp_task_wdt_reset();
        btnL->tick();
        btnR->tick();
        btnA->tick();
        btnOK->tick();
        drawFullGUI(u8g2, cursor);
        delay(10);
    }

    btnL->attachClick(nullptr);
    btnR->attachClick(nullptr);
    btnA->attachClick(nullptr);
    btnOK->attachClick(nullptr);

    InvCB::cursor = nullptr;
    InvCB::closed = nullptr;
    InvCB::self   = nullptr;

    return true;
}