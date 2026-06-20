#ifndef GAME_CLIENT_SVC_H
#define GAME_CLIENT_SVC_H

#include "config/config.h"

extern void appHeartBeat();
extern void drawMenu();

class GameClient_svc {
   private:
    float pX = 64.0, pY = 64.0;  // Posisi Player di dunia luas
    int toolMode = 0;            // 0: Hoe, 1: Seeds
    int pinL, pinR, pinA, pinOK;

    // Map 16x16 (0: Grass, 1: Tilled Soil, 2: Planted, 3: Wall/Fence)
    uint8_t worldMap[16][16];

    void initMap();
    void drawPlayer(int x, int y);
    void drawTile(int tx, int ty, int screenX, int screenY);

   public:
    GameClient_svc(int bL, int bR, int bA, int bOK);
    void begin();
    void runGame();
};
#endif