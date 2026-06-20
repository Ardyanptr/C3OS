#include "GameClient_Doom.h"

#include "app/Game/GameMode.h"

#include <esp_task_wdt.h>

GameClient_Doom::GameClient_Doom(int bL, int bR, int bA, int bOK)
    : pinL(bL), pinR(bR), pinA(bA), pinOK(bOK) {}

void GameClient_Doom::begin() {
    runGameMode();

    pinMode(pinL, INPUT_PULLUP);
    pinMode(pinR, INPUT_PULLUP);
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinOK, INPUT_PULLUP);

    health = 100;
    hasKey = false;
    score = 0;
    damageFlash = 0;

    posX = 1.5f;
    posY = 1.5f;
    dirX = -1.0f;
    dirY = 0.0f;
    planeX = 0.0f;
    planeY = 0.66f;
    health = 100;

    for (int i = 0; i <= 100; i += 5) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x12_tr);
        display.drawStr(35, 25, "LOADING...");
        display.drawFrame(24, 35, 80, 6);
        display.drawBox(26, 37, (i * 76) / 100, 2);
        display.sendBuffer();
        delay(20);
    }

    initMap();
}

void GameClient_Doom::initMap() {
    uint8_t tempMap[MAP_SIZE][MAP_SIZE] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1},
        {1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1},
        {1, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1},
        {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1},
        {1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1},
        {1, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 1, 0, 1, 0, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1},
        {1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
    memcpy(worldMap, tempMap, sizeof(worldMap));
}

void GameClient_Doom::takeDamage(int amount) {
    health -= amount;
    damageFlash = 2;
    if (health <= 0) isRunning = false;
}

void GameClient_Doom::shoot() {
    isFiring = true;
    fireFrame = 4;

    float rayDirX = dirX;
    float rayDirY = dirY;
    int mapX = int(posX);
    int mapY = int(posY);

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

    int distance = 0;

    while (hit == 0 && distance < 8) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
        }

        if (worldMap[mapX][mapY] == 1 || worldMap[mapX][mapY] == 4) hit = 1;
        if (worldMap[mapX][mapY] == 2) {
            worldMap[mapX][mapY] = 0;
            score += 100;
            hit = 2;
        }
        distance++;
    }
}

bool GameClient_Doom::isNearEnemy() {
    int pX = int(posX);
    int pY = int(posY);

    for (int x = pX - 1; x <= pX + 1; x++) {
        for (int y = pY - 1; y <= pY + 1; y++) {
            if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
                if (worldMap[x][y] == 2) return true;
            }
        }
    }
    return false;
}

void GameClient_Doom::updateGame(float dt) {
    float moveSpeed = 4.0f * dt;
    float rotSpeed = 2.5f * dt;
    float lerpFactor = 10.0f * dt;

    float rotDir = 0;
    if (digitalRead(pinL) == LOW) rotDir = rotSpeed;
    if (digitalRead(pinR) == LOW) rotDir = -rotSpeed;

    if (rotDir != 0) {
        float oldDirX = dirX;
        dirX = dirX * cos(rotDir) - dirY * sin(rotDir);
        dirY = oldDirX * sin(rotDir) + dirY * cos(rotDir);
        float oldPlaneX = planeX;
        planeX = planeX * cos(rotDir) - planeY * sin(rotDir);
        planeY = oldPlaneX * sin(rotDir) + planeY * cos(rotDir);
    }

    if (digitalRead(pinOK) == LOW) {
        float nextX = posX + dirX * moveSpeed;
        float nextY = posY + dirY * moveSpeed;

        if (worldMap[int(nextX)][int(posY)] == 0) posX = nextX;
        if (worldMap[int(posX)][int(nextY)] == 0) posY = nextY;

        viewBobbing += 0.2f;
    } else {
        viewBobbing *= 0.8f;
    }

    if (digitalRead(pinA) == LOW && !isFiring) {
        shoot();
    }

    if (random(0, 50) == 0 && isNearEnemy()) takeDamage(5);
}

void GameClient_Doom::renderHUD() {
    int bob = sin(viewBobbing) * 3;
    int gunX = 64;
    int gunY = 64 + bob;

    display.setDrawColor(1);

    if (isFiring) {
        gunY += 6;
        display.drawDisc(gunX, 35, 5 + random(0, 4));
        fireFrame--;
        if (fireFrame <= 0) isFiring = false;
    }

    display.drawBox(gunX - 4, gunY - 15, 8, 15);
    display.drawBox(gunX - 6, gunY - 8, 12, 8);

    display.drawPixel(64, 32);
    display.drawHLine(61, 32, 2);
    display.drawHLine(65, 32, 2);
    display.drawVLine(64, 29, 2);
    display.drawVLine(64, 33, 2);

    display.drawBox(0, 52, 128, 12);
    display.setDrawColor(0);
    display.setFont(u8g2_font_4x6_tr);

    display.setCursor(4, 61);
    display.print("HP:");
    display.print(health);

    display.setCursor(90, 61);
    display.print("PTS:");
    display.print(score);

    if (hasKey) display.drawDisc(64, 58, 2);
    display.setDrawColor(1);
}

void GameClient_Doom::renderScene() {
    if (damageFlash > 0) {
        display.sendBuffer();
        display.firstPage();
        do {
            display.setDrawColor(1);
            display.drawBox(0, 0, 128, 64);
        } while (display.nextPage());
        damageFlash--;
        return;
    }

    display.clearBuffer();

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

        if (hit == 2) {
            int enemyHeight = lineHeight / 2;

            int enemyStart = SCREEN_H / 2 - enemyHeight / 2;
            int enemyEnd = SCREEN_H / 2 + enemyHeight / 2;

            int hover = sin(millis() * 0.005f) * 3;
            enemyStart += hover;
            enemyEnd += hover;

            if (enemyStart < 0) enemyStart = 0;
            if (enemyEnd >= SCREEN_H) enemyEnd = SCREEN_H - 1;

            display.drawVLine(x, enemyStart, enemyEnd - enemyStart);

            display.drawPixel(x, enemyStart);
            display.drawPixel(x, enemyEnd);
        } else if (hit == 3) {
            display.drawBox(x, drawStart + 20, 2, 10);
        } else if (hit == 4) {
            if ((drawStart + x) % 4 == 0) display.drawVLine(x, drawStart, drawEnd - drawStart);
        } else if (hit == 5) {
            display.drawBox(x, drawStart, 2, drawEnd - drawStart);
        } else {
            if (side == 1)
                display.drawVLine(x, drawStart, drawEnd - drawStart);
            else {
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
    runGameMode();

    begin();
    isRunning = true;
    unsigned long lastTime = millis();

    while (isRunning && health > 0) {
        appHeartBeat();
        esp_task_wdt_reset();

        if (digitalRead(pinA) == LOW && digitalRead(pinOK) == LOW) break;

        unsigned long now = millis();
        if (now - lastTime < 33) continue;

        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        updateGame(dt);
        renderScene();
        vTaskDelay(1);
    }

    if (health <= 0) showGameOver(false);

    drawMenu();
}