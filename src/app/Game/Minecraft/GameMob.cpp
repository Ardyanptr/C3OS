#include "GameMob.h"
#include "GameInventory.h"   // full include only in .cpp
#include <U8g2lib.h>         // full include only in .cpp

int GameMob::mobHp(int type) {
    switch (type) {
        case MOB_ZOMBIE:   return 6;
        case MOB_SKELETON: return 4;
        case MOB_SLIME:    return 3;
        default:           return 4;
    }
}

int GameMob::mobDamage(int type) {
    switch (type) {
        case MOB_ZOMBIE:   return 2;
        case MOB_SKELETON: return 1;
        case MOB_SLIME:    return 1;
        default:           return 1;
    }
}

void GameMob::spawnMob(float x, float y, int type) {
    if (mobCount >= MAX_MOBS) return;
    MobData& m  = mobs[mobCount++];
    m.x = x; m.y = y; m.vx = 0; m.vy = 0;
    m.type      = type;
    m.active    = true;
    m.hp        = mobHp(type);
    m.maxHp     = m.hp;
    m.animPhase = 0; m.walkCycle = 0;
    m.onGround  = false;
    m.lastAttack  = 0; m.lastHitAnim = 0;
    m.isHit     = false;
    switch (type) {
        case MOB_ZOMBIE:   m.dropId = ITEM_COIN;  m.dropCount = random(1,4); break;
        case MOB_SKELETON: m.dropId = ITEM_STICK; m.dropCount = random(1,3); break;
        default:           m.dropId = ITEM_NONE;  m.dropCount = 0;           break;
    }
}

void GameMob::spawnNightWave(float playerX) {
    int count = random(2, 4);
    for (int i = 0; i < count && mobCount < MAX_MOBS; i++) {
        float side = (random(2) == 0) ? -20.0f : 20.0f;
        int   type = (random(3) == 0) ? MOB_SKELETON :
                     (random(2) == 0) ? MOB_SLIME    : MOB_ZOMBIE;
        spawnMob(playerX + side + random(-5, 5), 0, type);
    }
}

void GameMob::clear() {
    mobCount = 0;
    for (int i = 0; i < MAX_MOBS; i++) mobs[i].active = false;
}

void GameMob::updateMob(MobData& m, float playerX, float playerY,
                        float dt, std::function<uint8_t(int,int)> getBlock) {
    if (!m.active) return;

    unsigned long now = millis();
    if (m.isHit && now - m.lastHitAnim > 200) m.isHit = false;

    m.vy += 48.0f * dt;
    if (m.vy > 10) m.vy = 10;

    float dx    = playerX - m.x;
    float speed = (m.type == MOB_SLIME) ? 2.5f : 4.0f;
    m.vx = (abs(dx) > 1.0f) ? ((dx > 0) ? speed : -speed) : 0;

    if (m.onGround) {
        int frontX = (int)(m.x + (m.vx > 0 ? 1.2f : -0.2f));
        if (getBlock(frontX, (int)m.y) != 0) m.vy = -15.0f;
    }

    float nx = m.x + m.vx * dt;
    if (getBlock((int)(nx + 0.5f), (int)m.y) == 0) m.x = nx;

    float ny = m.y + m.vy * dt;
    if (getBlock((int)(m.x + 0.3f), (int)(ny + 1.0f)) != 0 ||
        getBlock((int)(m.x + 0.7f), (int)(ny + 1.0f)) != 0) {
        if (m.vy > 0) { m.y = (int)m.y; m.vy = 0; m.onGround = true; }
    } else {
        m.y = ny; m.onGround = false;
    }

    m.walkCycle++;
    if (m.walkCycle > 8) { m.walkCycle = 0; m.animPhase = (m.animPhase + 1) % 2; }
}

int GameMob::update(float playerX, float playerY, bool isNight,
                    float dt, std::function<uint8_t(int,int)> getBlock) {
    int dmg = 0;
    unsigned long now = millis();

    for (int i = 0; i < mobCount; i++) {
        MobData& m = mobs[i];
        if (!m.active) continue;
        updateMob(m, playerX, playerY, dt, getBlock);

        float dx = abs(m.x - playerX), dy = abs(m.y - playerY);
        if (dx < 1.5f && dy < 1.5f) {
            unsigned long cd = (m.type == MOB_ZOMBIE) ? 1500UL : 1000UL;
            if (now - m.lastAttack > cd) { dmg += mobDamage(m.type); m.lastAttack = now; }
        }
    }

    // Compact dead mobs
    int alive = 0;
    for (int i = 0; i < mobCount; i++) {
        if (mobs[i].active) mobs[alive++] = mobs[i];
    }
    mobCount = alive;
    return dmg;
}

bool GameMob::attackNearest(float playerX, float playerY, int damage,
                             GameInventory* inv) {
    float nearest = 999;
    int   nearIdx = -1;
    for (int i = 0; i < mobCount; i++) {
        if (!mobs[i].active) continue;
        float d = abs(mobs[i].x - playerX) + abs(mobs[i].y - playerY);
        if (d < nearest && d < 3.0f) { nearest = d; nearIdx = i; }
    }
    if (nearIdx < 0) return false;

    MobData& m = mobs[nearIdx];
    m.hp -= damage;
    m.isHit = true;
    m.lastHitAnim = millis();

    if (m.hp <= 0) {
        if (m.dropId != ITEM_NONE && inv) inv->addItem(m.dropId, m.dropCount);
        m.active = false;
    }
    return true;
}

void GameMob::drawMob(U8G2* u8g2, MobData& m, float camX, float camY) {
    int sx = (int)((m.x - camX) * 8);
    int sy = (int)((m.y - camY) * 8);
    if (sx < -12 || sx > 140) return;

    if (m.isHit) u8g2->setDrawColor(2);

    switch (m.type) {
        case MOB_ZOMBIE:
            u8g2->drawBox(sx+1, sy, 5, 6);
            u8g2->setDrawColor(m.isHit ? 2 : 0);
            u8g2->drawBox(sx+2, sy-4, 4, 3);
            u8g2->setDrawColor(m.isHit ? 2 : 1);
            u8g2->drawFrame(sx+2, sy-4, 4, 3);
            if (m.animPhase == 0) {
                u8g2->drawLine(sx, sy+1, sx-1, sy+3);
                u8g2->drawLine(sx+6, sy+1, sx+7, sy+3);
            } else {
                u8g2->drawLine(sx, sy+1, sx-1, sy+1);
                u8g2->drawLine(sx+6, sy+1, sx+7, sy+1);
            }
            break;

        case MOB_SKELETON:
            u8g2->drawFrame(sx+2, sy, 4, 5);
            u8g2->drawHLine(sx+2, sy+2, 4);
            u8g2->drawFrame(sx+2, sy-4, 3, 3);
            u8g2->drawLine(sx+3, sy+5, sx+2, sy+7+m.animPhase);
            u8g2->drawLine(sx+4, sy+5, sx+5, sy+7-m.animPhase);
            break;

        case MOB_SLIME: {
            int sq = m.animPhase;
            u8g2->drawBox(sx, sy+sq, 7, 5-sq);
            u8g2->setDrawColor(m.isHit ? 1 : 0);
            u8g2->drawBox(sx+1, sy+1+sq, 1, 1);
            u8g2->drawBox(sx+4, sy+1+sq, 1, 1);
            break;
        }
    }

    u8g2->setDrawColor(1);

    if (m.hp < m.maxHp) {
        u8g2->drawFrame(sx, sy-6, 8, 2);
        u8g2->drawBox(sx, sy-6, (m.hp * 8) / m.maxHp, 2);
    }
}

void GameMob::draw(U8G2* u8g2, float camX, float camY) {
    for (int i = 0; i < mobCount; i++) {
        if (mobs[i].active) drawMob(u8g2, mobs[i], camX, camY);
    }
}