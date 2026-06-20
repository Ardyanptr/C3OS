#ifndef GAME_MOB_H
#define GAME_MOB_H

#include <Arduino.h>
#include <functional>
#include "GameItem.h"

// Forward declarations only — full includes in .cpp
class U8G2;
class GameInventory;

#define MAX_MOBS     6
#define MOB_ZOMBIE   1
#define MOB_SKELETON 2
#define MOB_SLIME    3

struct MobData {
    float         x, y, vx, vy;
    int           type;
    bool          active;
    int           hp, maxHp;
    int           animPhase, walkCycle;
    bool          onGround;
    unsigned long lastAttack;
    unsigned long lastHitAnim;
    bool          isHit;
    uint8_t       dropId;
    int           dropCount;
};

class GameMob {
public:
    MobData mobs[MAX_MOBS];
    int     mobCount = 0;

    void spawnMob(float x, float y, int type);
    void spawnNightWave(float playerX);
    void clear();

    int  update(float playerX, float playerY, bool isNight,
                float dt, std::function<uint8_t(int,int)> getBlock);

    bool attackNearest(float playerX, float playerY, int damage,
                       GameInventory* inv);

    void draw(U8G2* u8g2, float camX, float camY);

private:
    void updateMob(MobData& m, float playerX, float playerY,
                   float dt, std::function<uint8_t(int,int)> getBlock);
    void drawMob(U8G2* u8g2, MobData& m, float camX, float camY);
    int  mobHp(int type);
    int  mobDamage(int type);
};

#endif