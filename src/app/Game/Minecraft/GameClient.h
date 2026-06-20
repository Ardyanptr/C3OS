#ifndef GAME_CLIENT_H
#define GAME_CLIENT_H

#include "config/config.h"
#include <functional>

#include "GameItem.h"
#include "GameInventory.h"
#include "GameChest.h"
#include "GameCrafting.h"
#include "GameMob.h"

extern void drawMenu();
extern void appHeartBeat();

#define TILE_SIZE    8
#define MAX_MODS     64
#define WORLD_WIDTH  400
#define WORLD_HEIGHT 48

// Block IDs
#define BLOCK_AIR    0
#define BLOCK_STONE  1
#define BLOCK_GRASS  2
#define BLOCK_DIRT   3
#define BLOCK_WOOD   4
#define BLOCK_LEAVES 5
#define BLOCK_ORE    6
#define BLOCK_CHEST  7
#define BLOCK_TORCH  8

struct ModifiedBlock {
    int     x, y;
    uint8_t type;
};

class GameClient {
public:
    GameClient(int bL, int bR, int bA, int bOK);
    void begin();
    void runGame();
    void loadSettings();
    void startMultiplayerSearch();
    void runMultiplayerGame(IPAddress serverIP);

private:
    // ── Pins ──────────────────────────────────────────────
    int pinL, pinR, pinA, pinOK;

    // ── Player ────────────────────────────────────────────
    float  playerX    = 50.0f;
    float  playerY    = 0.0f;
    float  velY       = 0.0f;
    float  camX       = 0.0f;
    float  camY       = 0.0f;
    bool   onGround   = false;

    int    playerHP   = 10;
    int    playerMaxHP= 10;
    unsigned long lastDamageTime = 0;
    bool   invincible = false;    // brief invincibility after hit
    int    score      = 0;
    int    coins      = 0;

    // Attack swing animation
    bool  attacking      = false;
    unsigned long attackStart = 0;
    static constexpr int ATTACK_MS = 200;

    // ── World ─────────────────────────────────────────────
    float  gravity    = 48.0f;
    float  velJump    = -18.0f;
    float  moveSpeed  = 14.0f;
    String currentBiome = "forest";

    ModifiedBlock mods[MAX_MODS];
    int           modCount = 0;

    // ── Day/Night ─────────────────────────────────────────
    unsigned long dayStart    = 0;
    static constexpr unsigned long DAY_MS   = 60000UL;  // 60s day
    static constexpr unsigned long NIGHT_MS = 40000UL;  // 40s night
    bool isNight = false;
    bool nightWaveSpawned = false;
    int  dayCount = 1;

    // ── Systems ───────────────────────────────────────────
    GameInventory inv;
    GameChest     chestSystem;
    GameCrafting  crafting;
    GameMob       mobSystem;

    // ── Textures ──────────────────────────────────────────
    uint8_t texture[9][8];

    // ── Methods ───────────────────────────────────────────
    void     generateWorld();
    uint8_t  getBlockAt(int x, int y);
    void     setBlock(int x, int y, uint8_t type);
    void     addModification(int x, int y, uint8_t type);

    void     loadTexture(const char* path, uint8_t id);
    void     loadSettings_internal();

    void     drawHUD();
    void     drawClouds();
    void     drawDayNightOverlay();
    void     drawAttackSwing();
    void     drawDeathScreen();
    void     drawDayCounter();

    void     updateDayNight();
    void     handleInput(float dt);
    void     handleInteract();     // open chest / crafting near player
    void     applyDamage(int dmg);
    void     checkPlayerDeath();

    void     spawnInitialMobs();
    void     giveStarterItems();

    int      fpsValue = 0;
};

#endif