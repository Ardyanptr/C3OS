#include "GameClient.h"

GameClient::GameClient(int bL, int bR, int bA, int bOK)
    : pinL(bL), pinR(bR), pinA(bA), pinOK(bOK) {}

void GameClient::addModification(int x, int y, uint8_t type) {
    for (int i = 0; i < modCount; i++) {
        if (mods[i].x == x && mods[i].y == y) {
            mods[i].type = type;
            return;
        }
    }
    if (modCount < MAX_MODS) {
        mods[modCount] = {x, y, type};
        modCount++;
    }
}

uint8_t GameClient::getBlockAt(int x, int y) {
    if (y < 0 || y >= 64) return 0;
    for (int i = 0; i < modCount; i++) {
        if (mods[i].x == x && mods[i].y == y) return mods[i].type;
    }

    float noiseH = sin(x * 0.12f) * 6.0f + cos(x * 0.04f) * 10.0f;
    int surfaceY = 32 + (int)noiseH;

    // 1. POHON (Setiap 25 blok)
    if (x % 25 == 0) {
        if (y < surfaceY && y >= surfaceY - 3) return 4;                          // Batang (Wood)
        if (y < surfaceY - 3 && y >= surfaceY - 5 && abs(x % 25) <= 1) return 5;  // Daun (Leaf)
    }

    // 2. VILLAGE DENGAN PINTU (Agar bisa lewat/explore)
    if (x % 150 >= 0 && x % 150 <= 10) {
        int relX = x % 150;
        // Atap & Lantai
        if (y == surfaceY - 6) return 7;
        if (y == surfaceY - 1) return 7;
        // Tembok tapi ada PINTU (kosong di relX 4-5)
        if ((relX == 0 || relX == 10) && (y < surfaceY - 1 && y > surfaceY - 6)) {
            if (y < surfaceY - 4) return 4;  // Hanya tembok atas, bawahnya lubang pintu
        }
    }

    // 3. VEIN CAVES
    if (y > surfaceY + 4 && (sin(x * 0.2f) * cos(y * 0.2f)) > 0.6f) return 0;

    if (y < surfaceY) return 0;
    if (y == surfaceY) return 2;
    if (y < surfaceY + 4) return 1;
    return 3;
}

void GameClient::drawClouds() {
    for (int i = 0; i < 5; i++) {
        int cx = (i * 40) - (int)(camX * 2) % 200;  // Gerak pelan (parallax)
        display.drawBox(cx, 10 + (i % 2 * 5), 15, 4);
        display.drawBox(cx + 4, 8 + (i % 2 * 5), 7, 2);
    }
}

void GameClient::spawnMobs() {
    for (int i = 0; i < MAX_MOBS; i++) {
        mobs[i].active = true;
        mobs[i].x = playerX + (i * 25) + 20;
        mobs[i].vx = 0.04f;
        mobs[i].type = (i % 2 == 0) ? 1 : 2;
    }
}

void GameClient::drawHUD(int fps) {
    display.setFont(u8g2_font_4x6_tf);
    display.setCursor(2, 6);
    display.print("FPS:");
    display.print(fps);
    display.print(" X:");
    display.print((int)playerX);
    display.print(" Y:");
    display.print((int)playerY);
}

void GameClient::begin() {
    spawnMobs();
    playerY = 10;  // Start dari atas biar discan pas landing
}

void GameClient::runGame() {
    unsigned long lastTime = millis();
    while (true) {
        appHeartBeat();

        float dt = (millis() - lastTime) / 1000.0f;
        lastTime = millis();
        if (dt > 0.1f) dt = 0.016f;

        // --- INPUT & AUTO-JUMP ---
        float nextX = playerX;
        if (digitalRead(pinL) == LOW) nextX -= 14.0f * dt;
        if (digitalRead(pinR) == LOW) nextX += 14.0f * dt;

        // Logika Auto-Jump: Jika ada halangan 1 blok di depan, lompat otomatis
        if (nextX != playerX) {
            int checkX = (nextX > playerX) ? (int)(nextX + 0.9f) : (int)(nextX - 0.1f);
            if (getBlockAt(checkX, (int)playerY) != 0 && onGround) {
                velY = -17.0f;  // Auto jump
            }
            playerX = nextX;
        }

        // --- PHYSICS (Loncat bebas) ---
        velY += 48.0f * dt;
        playerY += velY * dt;

        // Collision Ground
        if (getBlockAt((int)(playerX + 0.3f), (int)(playerY + 1.0f)) != 0 ||
            getBlockAt((int)(playerX + 0.7f), (int)(playerY + 1.0f)) != 0) {
            if (velY > 0) {
                playerY = (int)playerY;
                velY = 0;
                onGround = true;
            }
        } else {
            onGround = false;
        }

        // Manual Jump (Tombol OK) - Bisa kapan saja jika mau 'Double Jump' ringan
        if (digitalRead(pinOK) == LOW && (onGround || velY > 0)) {
            velY = -18.0f;
            onGround = false;
            vTaskDelay(2);
        }

        // --- MINING (Tombol A) ---
        if (digitalRead(pinA) == LOW) {
            addModification((int)(playerX + 0.5f), (int)(playerY + 1.1f), 0);
        }

        // Camera
        camX += (playerX - 8 - camX) * 0.1f;
        camY += (playerY - 4 - camY) * 0.1f;

        display.clearBuffer();
        drawClouds();  // Render Awan

        // Render World
        for (int x = (int)camX; x < (int)camX + 18; x++) {
            for (int y = (int)camY; y < (int)camY + 10; y++) {
                uint8_t b = getBlockAt(x, y);
                if (b == 0) continue;
                int sx = (x - camX) * 8;
                int sy = (y - camY) * 8;
                if (b == 2) display.drawBox(sx, sy, 8, 2);
                if (b == 5) {  // Daun pakai pattern
                    display.drawFrame(sx + 1, sy + 1, 6, 6);
                } else {
                    display.drawFrame(sx, sy, 8, 8);
                }
            }
        }

        // Render Mobs (Sederhana)
        for (int i = 0; i < MAX_MOBS; i++) {
            mobs[i].x += mobs[i].vx;
            int mx = (mobs[i].x - camX) * 8;
            if (mx > -10 && mx < 130) {
                display.drawFrame(mx + 2, (playerY - camY) * 8, 4, 8);
            }
        }

        // Player XOR
        display.setDrawColor(2);
        display.drawBox((playerX - camX) * 8 + 2, (playerY - camY) * 8, 4, 8);
        display.setDrawColor(1);

        drawHUD(1.0f / dt);
        display.sendBuffer();

        if (digitalRead(pinL) == LOW && digitalRead(pinOK) == LOW && digitalRead(pinA) == LOW) break;
        vTaskDelay(1);
    }
}