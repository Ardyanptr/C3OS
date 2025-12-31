#include "GameClient_Doom.h"

GameClient_Doom::GameClient_Doom(int bL, int bR, int bA, int bOK)
    : pinL(bL), pinR(bR), pinA(bA), pinOK(bOK) {}

void GameClient_Doom::begin() {
    pinMode(pinL, INPUT_PULLUP);
    pinMode(pinR, INPUT_PULLUP);
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinOK, INPUT_PULLUP);

    // PERBAIKAN: Spawn di area kosong (1.5, 1.5)
    posX = 1.5f;
    posY = 1.5f;

    dirX = -1.0f;
    dirY = 0.0f;
    planeX = 0.0f;
    planeY = 0.66f;

    health = 100;
    hasKey = false;
    score = 0;
    damageFlash = 0;

    initMap();
}

void GameClient_Doom::initMap() {
    // 1=Wall, 2=Enemy, 3=KEY, 4=LOCKED DOOR, 5=EXIT PORTAL
    uint8_t tempMap[MAP_SIZE][MAP_SIZE] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1},
        {1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1},
        {1, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1},  // Ada KUNCI (3) di sini
        {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 1, 0, 0, 1},  // Ada MUSUH (2)
        {1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1},  // Ada PINTU TERKUNCI (4)
        {1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  // Ada EXIT (5) di balik pintu
        {1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1},
        {1, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  // Ada MUSUH (2)
        {1, 0, 1, 0, 1, 0, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1},  // Banyak MUSUH
        {1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
    memcpy(worldMap, tempMap, sizeof(worldMap));
}

void GameClient_Doom::takeDamage(int amount) {
    health -= amount;
    damageFlash = 2;                     // Efek layar kedip 2 frame
    if (health <= 0) isRunning = false;  // Game Over logic nanti
}

void GameClient_Doom::shoot() {
    isFiring = true;
    fireFrame = 4;

    // Raycast untuk tembak
    float rayDirX = dirX;
    float rayDirY = dirY;
    int mapX = int(posX);
    int mapY = int(posY);

    // Logic DDA simple untuk hitscan
    float sideDistX, sideDistY;
    float deltaDistX = std::abs(1 / rayDirX);
    float deltaDistY = std::abs(1 / rayDirY);
    int stepX, stepY;

    if (rayDirX < 0) {
        stepX = -1;
        sideDistX = (posX - mapX) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0f - posX) * deltaDistX;
    }
    if (rayDirY < 0) {
        stepY = -1;
        sideDistY = (posY - mapY) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0f - posY) * deltaDistY;
    }

    int hit = 0;
    // Jarak tembak maks 6 blok biar gak terlalu OP
    int distance = 0;

    while (hit == 0 && distance < 8) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
        }

        if (worldMap[mapX][mapY] == 1 || worldMap[mapX][mapY] == 4) hit = 1;  // Tembok
        if (worldMap[mapX][mapY] == 2) {                                      // Kena Musuh
            worldMap[mapX][mapY] = 0;                                         // Musuh Mati
            score += 100;
            hit = 2;
        }
        distance++;
    }
}

void GameClient_Doom::updateGame(float dt) {
    // 1. Controls
    // Kecepatan putar & jalan disesuaikan biar enak
    float moveSpeed = 3.0f * dt;
    float rotSpeed = 2.2f * dt;

    // ROTASI (Kiri / Kanan)
    if (digitalRead(pinL) == LOW) {
        float oldDirX = dirX;
        dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
        dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
        planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
    if (digitalRead(pinR) == LOW) {
        float oldDirX = dirX;
        dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
        dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
        planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }

    // MAJU (Tombol OK) dengan WALL SLIDING
    if (digitalRead(pinOK) == LOW) {
        // Cek Sumbu X dulu
        float nextX = posX + dirX * moveSpeed;
        int blockX = worldMap[int(nextX)][int(posY)];

        // Logika Tabrakan X
        if (blockX == 0) {
            posX = nextX;
        }  // Kosong, maju
        else if (blockX == 3) {
            hasKey = true;
            worldMap[int(nextX)][int(posY)] = 0;
            posX = nextX;
            score += 500;
        }  // Kunci
        else if (blockX == 4 && hasKey) {
            worldMap[int(nextX)][int(posY)] = 0;
            posX = nextX;
        }  // Pintu
        else if (blockX == 5) {
            isRunning = false;
            showGameOver(true);
            return;
        }  // Menang

        // Cek Sumbu Y terpisah (biar bisa sliding)
        float nextY = posY + dirY * moveSpeed;
        int blockY = worldMap[int(posX)][int(nextY)];

        // Logika Tabrakan Y
        if (blockY == 0) {
            posY = nextY;
        } else if (blockY == 3) {
            hasKey = true;
            worldMap[int(posX)][int(nextY)] = 0;
            posY = nextY;
            score += 500;
        } else if (blockY == 4 && hasKey) {
            worldMap[int(posX)][int(nextY)] = 0;
            posY = nextY;
        } else if (blockY == 5) {
            isRunning = false;
            showGameOver(true);
            return;
        }
    }

    // SHOOT (Tombol A)
    if (digitalRead(pinA) == LOW && !isFiring) {
        shoot();
    }

    // 2. Enemy Logic (Kena damage kalau dekat musuh)
    int pX = int(posX);
    int pY = int(posY);

    // Cek area sekitar 3x3
    bool nearEnemy = false;
    for (int x = pX - 1; x <= pX + 1; x++) {
        for (int y = pY - 1; y <= pY + 1; y++) {
            if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
                if (worldMap[x][y] == 2) nearEnemy = true;
            }
        }
    }

    if (nearEnemy) {
        // 1 dari 15 kesempatan kena damage per frame (biar gak instan mati)
        if (random(0, 15) == 0) takeDamage(5);
    }
}

void GameClient_Doom::renderHUD() {
    // HUD Pistol
    int gunX = 64;
    int gunY = 64;
    display.setDrawColor(1);

    // Animasi Pistol
    if (isFiring) {
        gunY += 4;                                     // Recoil ke bawah
        display.drawDisc(gunX, 40, 6 + random(0, 3));  // Muzzle flash besar
        fireFrame--;
        if (fireFrame <= 0) isFiring = false;
    }

    display.drawBox(gunX - 5, gunY - 20, 10, 20);  // Gun body

    // Crosshair
    display.drawHLine(60, 32, 9);
    display.drawVLine(64, 28, 9);

    // Status Bar Bawah
    display.setFont(u8g2_font_4x6_tr);

    // Health Bar
    display.setCursor(2, 60);
    display.print("HP:");
    display.drawFrame(16, 54, 30, 6);
    int hpBar = map(health, 0, 100, 0, 28);
    display.drawBox(17, 55, hpBar, 4);

    // Key Indicator
    if (hasKey) {
        display.drawStr(100, 60, "KEY:OK");
    } else {
        display.drawStr(100, 60, "KEY:NO");
    }
}

void GameClient_Doom::renderScene() {
    // Jika kena damage, invert layar (efek sakit)
    if (damageFlash > 0) {
        display.sendBuffer();  // Render kosong
        display.firstPage();   // Paksa refresh layar putih/hitam
        do {
            display.setDrawColor(1);
            display.drawBox(0, 0, 128, 64);
        } while (display.nextPage());
        damageFlash--;
        return;
    }

    display.clearBuffer();

    // --- RAYCASTING LOOP (Sama seperti sebelumnya, disederhanakan) ---
    for (int x = 0; x < SCREEN_W; x += 2) {
        float cameraX = 2 * x / (float)SCREEN_W - 1;
        float rayDirX = dirX + planeX * cameraX;
        float rayDirY = dirY + planeY * cameraX;

        int mapX = int(posX);
        int mapY = int(posY);
        float sideDistX, sideDistY;
        float deltaDistX = std::abs(1 / rayDirX);
        float deltaDistY = std::abs(1 / rayDirY);
        int stepX, stepY;
        int hit = 0;
        int side;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0f - posX) * deltaDistX;
        }
        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0f - posY) * deltaDistY;
        }

        while (hit == 0) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            if (worldMap[mapX][mapY] > 0) hit = worldMap[mapX][mapY];
        }

        float perpWallDist;
        if (side == 0)
            perpWallDist = (mapX - posX + (1 - stepX) / 2) / rayDirX;
        else
            perpWallDist = (mapY - posY + (1 - stepY) / 2) / rayDirY;

        int lineHeight = (int)(SCREEN_H / perpWallDist);
        int drawStart = -lineHeight / 2 + SCREEN_H / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_H / 2;
        if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

        // Render Tipe Blok
        if (hit == 2) {                                                            // MUSUH
            for (int i = drawStart; i < drawEnd; i += 2) display.drawPixel(x, i);  // Efek hantu
        } else if (hit == 3) {                                                     // KUNCI (Garis-garis melayang)
            display.drawBox(x, drawStart + 20, 2, 10);
        } else if (hit == 4) {  // PINTU (Kotak-kotak silang)
            if ((drawStart + x) % 4 == 0) display.drawVLine(x, drawStart, drawEnd - drawStart);
        } else if (hit == 5) {  // EXIT (Putih total)
            display.drawBox(x, drawStart, 2, drawEnd - drawStart);
        } else {  // TEMBOK BIASA
            if (side == 1)
                display.drawVLine(x, drawStart, drawEnd - drawStart);
            else {  // Dithering
                for (int i = drawStart; i < drawEnd; i += 3) display.drawPixel(x, i);
            }
        }
    }

    renderHUD();
    display.sendBuffer();
}

void GameClient_Doom::showGameOver(bool win) {
    display.clearBuffer();
    display.setFont(u8g2_font_9x15_tf);

    if (win) {
        display.drawStr(25, 30, "YOU WON!");
        display.setFont(u8g2_font_5x8_tf);
        display.setCursor(25, 45);
        display.print("Score: ");
        display.print(score);
    } else {
        display.drawStr(20, 30, "GAME OVER");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(30, 45, "You died.");
    }

    display.sendBuffer();
    delay(3000);
}

void GameClient_Doom::runGame() {
    begin();
    isRunning = true;
    unsigned long lastTime = millis();

    while (isRunning && health > 0) {
        appHeartBeat();

        if (digitalRead(pinA) == LOW && digitalRead(pinOK) == LOW) break;  // Exit combo

        unsigned long now = millis();
        if (now - lastTime < 33) continue;  // Cap FPS ~30

        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        updateGame(dt);
        renderScene();
        vTaskDelay(1);
    }

    if (health <= 0) showGameOver(false);

    drawMenu();
}