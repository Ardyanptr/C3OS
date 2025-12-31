#ifndef GAME_CLIENT_DOOM_H
#define GAME_CLIENT_DOOM_H

#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;

extern void drawMenu();
extern void appHeartBeat();

#define MAP_SIZE 16
#define SCREEN_W 128
#define SCREEN_H 64

class GameClient_Doom {
   private:
    int pinL, pinR, pinA, pinOK;

    // Player State
    float posX, posY;
    float dirX, dirY;
    float planeX, planeY;
    int health;
    bool hasKey;

    // Game State
    bool isRunning;
    bool isFiring;
    int fireFrame;
    int damageFlash;  // Efek layar kedip pas kena damage
    int score;

    uint8_t worldMap[MAP_SIZE][MAP_SIZE];

    void initMap();
    void updateGame(float dt);
    void renderScene();
    void renderHUD();
    void shoot();
    void takeDamage(int amount);
    void showGameOver(bool win);

   public:
    GameClient_Doom(int bL, int bR, int bA, int bOK);
    void begin();
    void runGame();
};

#endif