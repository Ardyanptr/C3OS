#ifndef GAME_CLIENT_H
#define GAME_CLIENT_H

#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;

extern void drawMenu();
extern void appHeartBeat();

#define TILE_SIZE 8
#define MAX_MOBS 8
#define MAX_MODS 60

struct ModifiedBlock {
    int x, y;
    uint8_t type;
};

class GameClient {
   private:
    int pinL, pinR, pinA, pinOK;
    float playerX = 50.0f, playerY = 0.0f;
    float velY = 0.0f, camX = 0.0f, camY = 0.0f;
    bool onGround = false;

    ModifiedBlock mods[MAX_MODS];
    int modCount = 0;

    struct MobData {
        float x, y, vx;
        int type;
        bool active;
    } mobs[MAX_MOBS];

    uint8_t getBlockAt(int x, int y);
    void addModification(int x, int y, uint8_t type);
    void handleInteractions();
    void spawnMobs();
    void drawHUD(int fps);
    void drawClouds();

   public:
    GameClient(int bL, int bR, int bA, int bOK);
    void begin();
    void runGame();
};

#endif