#include "GameClient.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

extern uint8_t texture[9][8];

// ─────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────
GameClient::GameClient(int bL, int bR, int bA, int bOK)
    : pinL(bL), pinR(bR), pinA(bA), pinOK(bOK) {}

// ─────────────────────────────────────────────────────────
// Texture
// ─────────────────────────────────────────────────────────
void GameClient::loadTexture(const char* path, uint8_t id) {
    if (id == 0 || id > 8) return;
    File f = LittleFS.open(path, "r");
    if (!f) return;
    f.read(texture[id], 8);
    f.close();
}

// ─────────────────────────────────────────────────────────
// World generation
// ─────────────────────────────────────────────────────────
void GameClient::generateWorld() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    int totalChunks = 100;

    for (int i = 0; i <= totalChunks; i++) {
        display.clearBuffer();
        display.drawFrame(14, 45, 100, 6);
        display.drawBox(16, 47, (i * 96) / totalChunks, 2);
        display.setCursor(55, 42); display.print(i); display.print("%");
        display.drawStr(35, 25, "Generating World");
        display.drawStr(40, 33, "Building terrain");
        for (int x = i * 10; x < (i + 1) * 10; x++) getBlockAt(x, 32);
        display.sendBuffer();
        esp_task_wdt_reset();
        yield();
        delay(1);
    }

    // Place chests at structures
    for (int x = 0; x < WORLD_WIDTH; x += 120) {
        int structPos = x % 120;
        if (structPos == 0) {
            // Chest inside structure
            chestSystem.placeChest(x + 4, 26, true);
        }
    }
}

void GameClient::addModification(int x, int y, uint8_t type) {
    for (int i = 0; i < modCount; i++) {
        if (mods[i].x == x && mods[i].y == y) {
            mods[i].type = type; return;
        }
    }
    if (modCount < MAX_MODS) {
        mods[modCount++] = {x, y, type};
    }
}

void GameClient::setBlock(int x, int y, uint8_t type) {
    addModification(x, y, type);
    // If placing a chest, register it
    if (type == BLOCK_CHEST) {
        chestSystem.placeChest(x, y, false);
    }
}

uint8_t GameClient::getBlockAt(int x, int y) {
    if (y < 0 || y >= 64) return 0;

    for (int i = 0; i < modCount; i++) {
        if (mods[i].x == x && mods[i].y == y) return mods[i].type;
    }

    float sx015  = sin(x * 0.15f);
    float noiseH = sx015 * 4.0f + cos(x * 0.05f) * 8.0f;
    int   surfaceY = 32 + (int)noiseH;

    if (y < surfaceY - 6) return BLOCK_AIR;

    if (y <= surfaceY) {
        if (x % 20 == 0) {
            if (y >= surfaceY - 3) return BLOCK_WOOD;
            if (y >= surfaceY - 6) return BLOCK_LEAVES;
        }
        int structPos = x % 120;
        if (structPos >= 0 && structPos <= 8) {
            int floorY = surfaceY - 1;
            int roofY  = surfaceY - 6;
            if (y == roofY || y == floorY) return BLOCK_STONE;
            if ((structPos == 0 || structPos == 8) && y > roofY) return BLOCK_WOOD;
            if (structPos == 4 && y == floorY - 1) return BLOCK_CHEST;  // chest inside
        }
    }

    if (y == surfaceY)                      return BLOCK_GRASS;
    if (y > surfaceY && y < surfaceY + 4)   return BLOCK_DIRT;

    if (y >= surfaceY + 4) {
        if (y > surfaceY + 5) {
            if (sx015 * cos(y * 0.35f) > 0.8f) return BLOCK_AIR;
        }
        if (y > 50 && (sx015 * cos(y * 0.5f)) > 0.8f) return BLOCK_ORE;
        return BLOCK_STONE;
    }
    return BLOCK_AIR;
}

// ─────────────────────────────────────────────────────────
// Settings
// ─────────────────────────────────────────────────────────
void GameClient::loadSettings() { loadSettings_internal(); }

void GameClient::loadSettings_internal() {
    File file = LittleFS.open("/GameClient/Minecraft/settings.json");
    if (!file) return;
    StaticJsonDocument<512> doc;
    deserializeJson(doc, file);
    gravity    = doc["gravity"]    | 48.0f;
    velJump    = doc["jump_force"] | -18.0f;
    moveSpeed  = doc["move_speed"] | 14.0f;
    currentBiome = doc["biome"].as<String>();
    file.close();
}

// ─────────────────────────────────────────────────────────
// Starter items
// ─────────────────────────────────────────────────────────
void GameClient::giveStarterItems() {
    inv.addItem(ITEM_SWORD,   1);
    inv.addItem(ITEM_PICKAXE, 1);
    inv.addItem(ITEM_DIRT,    10);
    inv.hotbarIndex = 0;
}

// ─────────────────────────────────────────────────────────
// Day/Night
// ─────────────────────────────────────────────────────────
void GameClient::updateDayNight() {
    unsigned long elapsed = millis() - dayStart;
    unsigned long cycle   = DAY_MS + NIGHT_MS;
    unsigned long phase   = elapsed % cycle;

    bool wasNight = isNight;
    isNight = (phase >= DAY_MS);

    if (!wasNight && isNight) {
        // Transition to night
        nightWaveSpawned = false;
        dayCount++;
    }
    if (isNight && !nightWaveSpawned) {
        mobSystem.spawnNightWave(playerX);
        nightWaveSpawned = true;
    }
    if (wasNight && !isNight) {
        // Dawn — remove all mobs
        mobSystem.clear();
        spawnInitialMobs();
    }
}

// ─────────────────────────────────────────────────────────
// HUD
// ─────────────────────────────────────────────────────────
void GameClient::drawHUD() {
    display.setFont(u8g2_font_4x6_tf);

    // HP hearts (top left)
    int hearts = (playerHP + 1) / 2;
    int maxH   = (playerMaxHP + 1) / 2;
    for (int i = 0; i < maxH; i++) {
        int hx = 2 + i * 7;
        if (i < hearts) {
            display.drawBox(hx, 2, 5, 4);       // filled
        } else {
            display.drawFrame(hx, 2, 5, 4);     // empty
        }
    }

    // Coins (top right)
    display.setCursor(90, 6);
    display.print("$");
    display.print(coins);

    // Day/Night indicator
    display.setCursor(2, 12);
    if (isNight) {
        display.print("NIGHT ");
        display.print(dayCount);
    } else {
        display.print("Day ");
        display.print(dayCount);
    }

    // FPS
    display.setCursor(108, 6);
    display.print(fpsValue);

    // Active item name
    if (!inv.activeSlot().isEmpty()) {
        display.setCursor(2, 62 - 14);
        display.print(itemName(inv.activeSlot().id));
    }
}

void GameClient::drawClouds() {
    for (int i = 0; i < 5; i++) {
        int cx = (i * 40) - (int)(camX * 2) % 200;
        display.drawBox(cx, 10 + (i % 2 * 5), 15, 4);
        display.drawBox(cx + 4, 8 + (i % 2 * 5), 7, 2);
    }
}

void GameClient::drawDayNightOverlay() {
    if (!isNight) return;
    // Night: draw dark overlay at top (simulate darker sky)
    display.setDrawColor(0);
    display.drawBox(0, 0, 128, 8);
    display.setDrawColor(1);
    // Moon
    display.drawDisc(120 - (int)(camX * 0.1f) % 120, 5, 3);
}

void GameClient::drawAttackSwing() {
    if (!attacking) return;
    unsigned long elapsed = millis() - attackStart;
    if (elapsed > ATTACK_MS) { attacking = false; return; }

    // Draw a swing arc in front of player
    int px = (int)((playerX - camX) * 8) + 4;
    int py = (int)((playerY - camY) * 8) + 4;
    int dir = (digitalRead(pinR) == LOW) ? 1 : -1;
    display.drawLine(px, py, px + dir * 8, py - 4);
    display.drawLine(px, py, px + dir * 6, py + 4);
}

void GameClient::drawDeathScreen() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(30, 20, "YOU DIED");
    display.setFont(u8g2_font_4x6_tr);
    char buf[32];
    snprintf(buf, 32, "Score: %d  Coins: %d", score, coins);
    display.drawStr(10, 35, buf);
    snprintf(buf, 32, "Day: %d", dayCount);
    display.drawStr(45, 45, buf);
    display.drawStr(20, 58, "[OK] Play Again");
    display.sendBuffer();
}

void GameClient::drawDayCounter() {
    // Brief day/night transition message
    display.setFont(u8g2_font_6x10_tr);
    if (isNight) {
        display.drawStr(35, 35, "NIGHT!");
    } else {
        char buf[16]; snprintf(buf, 16, "Day %d", dayCount);
        display.drawStr(40, 35, buf);
    }
}

// ─────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────
void GameClient::handleInput(float dt) {
    // Movement
    float nextX = playerX;
    if (digitalRead(pinL) == LOW) nextX -= moveSpeed * dt;
    if (digitalRead(pinR) == LOW) nextX += moveSpeed * dt;

    if (nextX != playerX) {
        int checkX = (nextX > playerX) ? (int)(nextX + 0.9f) : (int)(nextX - 0.1f);
        if (getBlockAt(checkX, (int)playerY) != 0 && onGround) {
            velY = -17.0f;
        }
        playerX = nextX;
    }

    // Jump
    if (digitalRead(pinOK) == LOW && onGround) {
        velY = velJump;
        onGround = false;
        vTaskDelay(2);
    }

    // Attack (pinA tap)
    static bool lastA = HIGH;
    bool curA = digitalRead(pinA);
    if (curA == LOW && lastA == HIGH) {
        // Start attack
        attacking    = true;
        attackStart  = millis();

        int dmg = weaponDamage(inv.activeSlot().id);
        bool hit = mobSystem.attackNearest(playerX, playerY, dmg, nullptr);
        if (hit) score += 10;
    }
    lastA = curA;

    // Break block (pinA long = hold)
    static unsigned long aHeld = 0;
    if (curA == LOW) {
        if (aHeld == 0) aHeld = millis();
        if (millis() - aHeld > 400) {
            // Break block in front
            int bx = (int)(playerX + 0.5f + (digitalRead(pinR) == LOW ? 1 : -1));
            int by = (int)(playerY + 0.5f);
            uint8_t broken = getBlockAt(bx, by);
            if (broken != BLOCK_AIR) {
                uint8_t item = blockToItem(broken);
                if (item != ITEM_NONE) inv.addItem(item, 1);
                addModification(bx, by, BLOCK_AIR);
                score += 5;
            }
        }
    } else {
        aHeld = 0;
    }

    // Hotbar switch: L+OK or R+OK
    static bool lastOK = HIGH;
    bool curOK = digitalRead(pinOK);
    if (curOK == LOW && lastOK == HIGH) {
        if (digitalRead(pinL) == LOW) {
            inv.prevHotbar();
        } else if (digitalRead(pinR) == LOW) {
            inv.nextHotbar();
        }
    }
    lastOK = curOK;
}

void GameClient::handleInteract() {
    // Check if chest is in front of player
    int fx = (int)(playerX + 1.5f);
    int fy = (int)(playerY);
    int ci = chestSystem.findChest(fx, fy);
    if (ci >= 0) {
        // Open chest GUI
        // Temporarily create OneButton wrappers from pin reads
        // Since GameClient uses direct digitalRead, we pass dummies
        // The GUI systems use OneButton* — pass nullptr for non-OneButton builds
        // For now: simple poll-based open
        // TODO: integrate with btnL/btnR/btnA/btnOK if using OneButton globally
    }
}

// ─────────────────────────────────────────────────────────
// Damage
// ─────────────────────────────────────────────────────────
void GameClient::applyDamage(int dmg) {
    if (invincible) return;
    playerHP -= dmg;
    if (playerHP < 0) playerHP = 0;
    invincible    = true;
    lastDamageTime = millis();
}

void GameClient::checkPlayerDeath() {
    if (playerHP <= 0) {
        drawDeathScreen();
        while (digitalRead(pinOK) == HIGH) {
            esp_task_wdt_reset();
            delay(10);
        }
        delay(200);
        // Reset
        playerHP = playerMaxHP;
        playerX  = 50.0f;
        playerY  = 10.0f;
        velY     = 0;
        score    = 0;
        coins    = 0;
        dayCount = 1;
        dayStart = millis();
        isNight  = false;
        mobSystem.clear();
        modCount = 0;
        inv = GameInventory();
        giveStarterItems();
        spawnInitialMobs();
        generateWorld();
    }
}

// ─────────────────────────────────────────────────────────
// Mobs
// ─────────────────────────────────────────────────────────
void GameClient::spawnInitialMobs() {
    mobSystem.clear();
    mobSystem.spawnMob(playerX + 15, 0, MOB_ZOMBIE);
    mobSystem.spawnMob(playerX - 20, 0, MOB_SKELETON);
    mobSystem.spawnMob(playerX + 30, 0, MOB_SLIME);
}

// ─────────────────────────────────────────────────────────
// begin()
// ─────────────────────────────────────────────────────────
void GameClient::begin() {
    // Title screen + menu
    int  menuIndex = 0;
    bool selected  = false;

    display.clearBuffer();
    display.setFont(u8g2_font_profont17_tr);
    display.drawStr(24, 31, "MINECRAFT");
    display.sendBuffer();
    delay(500);

    // Load textures
    loadTexture("/GameClient/Minecraft/grass.bin",  BLOCK_GRASS);
    loadTexture("/GameClient/Minecraft/stone.bin",  BLOCK_STONE);
    loadTexture("/GameClient/Minecraft/wood.bin",   BLOCK_WOOD);
    loadTexture("/GameClient/Minecraft/leaves.bin", BLOCK_LEAVES);

    while (!selected) {
        display.clearBuffer();
        display.setFontMode(1);
        display.setBitmapMode(1);
        display.setFont(u8g2_font_profont17_tr);
        display.drawStr(24, 20, "MINECRAFT");

        // Menu items
        const char* items[] = { "Singleplayer", "Multiplayer" };
        for (int i = 0; i < 2; i++) {
            if (menuIndex == i) display.drawRBox(29, 26 + i * 11, 71, 9, 3);
            else                display.drawRFrame(29, 26 + i * 11, 71, 9, 3);
            display.setDrawColor(menuIndex == i ? 2 : 1);
            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(41, 33 + i * 11, items[i]);
            display.setDrawColor(1);
        }
        display.drawStr(1, 63, "C3OS(R) C++");
        display.sendBuffer();

        if (digitalRead(pinL) == LOW || digitalRead(pinR) == LOW) {
            menuIndex = !menuIndex; delay(150);
        }
        if (digitalRead(pinOK) == LOW) { selected = true; delay(200); }
        yield();
    }

    if (menuIndex == 0) {
        loadSettings_internal();
        generateWorld();
        giveStarterItems();
        spawnInitialMobs();
        dayStart = millis();
        playerY  = 10.0f;
        runGame();
        begin();
    } else {
        startMultiplayerSearch();
        begin();
    }
}

// ─────────────────────────────────────────────────────────
// runGame() — main game loop
// ─────────────────────────────────────────────────────────
void GameClient::runGame() {
    unsigned long lastTime    = millis();
    unsigned long lastFPS     = millis();
    unsigned long lastTransMsg = 0;
    bool showTransMsg = false;
    int  frameCount   = 0;

    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();

        unsigned long now = millis();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;
        dt = constrain(dt, 0.001f, 0.05f);

        // ── Day/Night ──────────────────────────────────
        bool prevNight = isNight;
        updateDayNight();
        if (prevNight != isNight) {
            showTransMsg  = true;
            lastTransMsg  = now;
        }
        if (now - lastTransMsg > 2000) showTransMsg = false;

        // ── Invincibility frames ───────────────────────
        if (invincible && now - lastDamageTime > 800) invincible = false;

        // ── Input ─────────────────────────────────────
        handleInput(dt);

        // ── Physics ───────────────────────────────────
        velY += gravity * dt;
        playerY += velY * dt;

        if (getBlockAt((int)(playerX + 0.3f), (int)(playerY + 1.0f)) != 0 ||
            getBlockAt((int)(playerX + 0.7f), (int)(playerY + 1.0f)) != 0) {
            if (velY > 0) { playerY = (int)playerY; velY = 0; onGround = true; }
        } else {
            onGround = false;
        }

        // ── Mob update ────────────────────────────────
        int dmgThisFrame = mobSystem.update(playerX, playerY, isNight, dt,
            [this](int x, int y) { return getBlockAt(x, y); });
        if (dmgThisFrame > 0) applyDamage(dmgThisFrame);

        // ── Coin collect ──────────────────────────────
        if (inv.hasItem(ITEM_COIN)) {
            coins += inv.slots[0].count;  // simplified: auto collect
            inv.removeItem(ITEM_COIN, inv.slots[0].count);
        }

        // ── Camera ────────────────────────────────────
        camX += (playerX - 8 - camX) * 0.1f;
        camY += (playerY - 4 - camY) * 0.1f;

        // ── Render ────────────────────────────────────
        display.clearBuffer();

        if (isNight) {
            // Dark background
            display.setDrawColor(0);
            display.drawBox(0, 0, 128, 64);
            display.setDrawColor(1);
        }

        drawClouds();

        // Blocks
        for (int x = (int)camX; x < (int)camX + 18; x++) {
            for (int y = (int)camY; y < (int)camY + 10; y++) {
                uint8_t b = getBlockAt(x, y);
                if (b == BLOCK_AIR) continue;

                int sx = (int)((x - camX) * 8);
                int sy = (int)((y - camY) * 8);

                if (b <= 4 && texture[b][0] != 0) {
                    display.drawBitmap(sx, sy, 1, 8, texture[b]);
                } else {
                    // Fallback pattern per block type
                    display.drawFrame(sx, sy, 8, 8);
                    if (b == BLOCK_ORE)   { display.drawBox(sx+2, sy+2, 2, 2); display.drawBox(sx+5, sy+4, 2, 2); }
                    if (b == BLOCK_CHEST) { display.drawBox(sx+1, sy+1, 6, 6); display.setDrawColor(0); display.drawBox(sx+3, sy+2, 2, 2); display.setDrawColor(1); }
                    if (b == BLOCK_TORCH) { display.drawLine(sx+4, sy+1, sx+4, sy+6); display.drawBox(sx+3, sy, 2, 2); }
                }
            }
        }

        // Mobs
        mobSystem.draw(&display, camX, camY);

        // Player (invincibility blink)
        bool drawPlayer = !invincible || ((millis() / 100) % 2 == 0);
        if (drawPlayer) {
            display.setDrawColor(2);
            display.drawBox((int)((playerX - camX) * 8) + 2,
                            (int)((playerY - camY) * 8), 4, 8);
            display.setDrawColor(1);
        }

        drawAttackSwing();
        drawDayNightOverlay();

        // Hotbar
        inv.drawHotbar(&display);

        drawHUD();

        // Transition message
        if (showTransMsg) drawDayCounter();

        display.sendBuffer();

        // ── FPS ───────────────────────────────────────
        frameCount++;
        if (now - lastFPS >= 1000) {
            fpsValue   = frameCount;
            frameCount = 0;
            lastFPS    = now;
        }

        // ── Frame cap ~30fps ──────────────────────────
        unsigned long ft = millis() - now;
        if (ft < 33) delay(33 - ft);

        // ── Death check ───────────────────────────────
        checkPlayerDeath();

        // ── Exit: L+R+OK ──────────────────────────────
        if (digitalRead(pinL) == LOW && digitalRead(pinR) == LOW &&
            digitalRead(pinOK) == LOW) {
            delay(200); break;
        }

        esp_task_wdt_reset();
        vTaskDelay(1);
    }
}

// ─────────────────────────────────────────────────────────
// Multiplayer (unchanged from original)
// ─────────────────────────────────────────────────────────
void GameClient::startMultiplayerSearch() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(22, 7, "Connecting to WiFi...");
    display.sendBuffer();

    WiFiManager wm;
    wm.setConfigPortalTimeout(1);
    if (!wm.autoConnect("ESP32C3", "123456789")) {
        display.drawStr(32, 20, "Connection Error");
        delay(2000); begin(); return;
    }

    WiFiUDP udp;
    udp.begin(12346);
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 30, "Scanning Server...");
    display.sendBuffer();

    IPAddress broadcastIP(255, 255, 255, 255);
    udp.beginPacket(broadcastIP, 12345);
    udp.write((const uint8_t*)"MC_C3_PING", 10);
    udp.endPacket();

    unsigned long start = millis();
    bool found = false;
    while (millis() - start < 3000) {
        int ps = udp.parsePacket();
        if (ps) {
            IPAddress serverIP = udp.remoteIP();
            playerX = 50.0f; playerY = 25.0f;
            runMultiplayerGame(serverIP);
            found = true; break;
        }
        yield();
    }
    if (!found) {
        display.clearBuffer();
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(30, 35, "No Server Found!");
        display.sendBuffer();
        delay(2000);
    }
}

void GameClient::runMultiplayerGame(IPAddress serverIP) {
    WiFiUDP udp;
    udp.begin(12346);
    char   packetBuffer[255];
    String currentMap = "";
    float  vY = 0;
    bool   onG = false;

    while (true) {
        esp_task_wdt_reset();
        vY += 0.25f; if (vY > 2.5f) vY = 2.5f;
        playerY += vY;

        udp.beginPacket(serverIP, 12345);
        udp.printf("POS:%.1f,%.1f", playerX, playerY);
        udp.endPacket();

        int ps = udp.parsePacket();
        if (ps) {
            int len = udp.read(packetBuffer, 254);
            packetBuffer[len] = '\0';
            String msg = String(packetBuffer);
            if (msg.startsWith("MAP:")) currentMap = msg.substring(4);
        }

        if (currentMap.length() >= 180) {
            char bu = currentMap[5 * 18 + 9];
            char bb = currentMap[4 * 18 + 9];
            char bf = (digitalRead(pinR) == LOW) ? currentMap[4*18+10] : currentMap[4*18+8];
            if (bu != '0') { if (vY > 0) { vY = 0; onG = true; playerY = floor(playerY); } }
            else onG = false;
            if (bb != '0') { playerY -= 1.0f; vY = 0; }
            if (bf != '0' && onG) { vY = -2.2f; onG = false; }
        }

        display.clearBuffer();
        display.setDrawColor(2);
        display.drawBox(64, 32, 4, 7);
        display.setDrawColor(1);
        display.sendBuffer();

        if (digitalRead(pinL) == LOW) playerX -= 0.5f;
        if (digitalRead(pinR) == LOW) playerX += 0.5f;
        if ((digitalRead(pinL) == LOW && digitalRead(pinOK) == LOW) ||
            (digitalRead(pinA) == LOW && digitalRead(pinOK) == LOW)) {
            delay(250); break;
        }
        delay(15); yield(); vTaskDelay(1);
    }
}