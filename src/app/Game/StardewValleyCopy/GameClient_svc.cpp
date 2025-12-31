#include "GameClient_svc.h"

bool isGameRunning_StardewValleyCopy = true;

GameClient_svc::GameClient_svc(int bL, int bR, int bA, int bOK)
    : pinL(bL), pinR(bR), pinA(bA), pinOK(bOK) {}

void GameClient_svc::initMap() {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (i == 0 || i == 15 || j == 0 || j == 15)
                worldMap[i][j] = 3;  // Pagar
            else
                worldMap[i][j] = 0;  // Rumput
        }
    }
}

void GameClient_svc::drawTile(int tx, int ty, int x, int y) {
    uint8_t type = worldMap[tx][ty];
    switch (type) {
        case 0:  // Grass (titik kecil)
            display.drawPixel(x + 4, y + 4);
            break;
        case 1:  // Tilled Soil (kotak garis-garis)
            display.drawFrame(x + 1, y + 1, 6, 6);
            break;
        case 2:  // Planted (titik tebal di tengah)
            display.drawBox(x + 3, y + 3, 3, 3);
            break;
        case 3:  // Fence (X)
            display.drawLine(x, y, x + 8, y + 8);
            display.drawLine(x + 8, y, x, y + 8);
            break;
    }
}

void GameClient_svc::drawPlayer(int x, int y) {
    display.drawDisc(x, y, 3);       // Kepala/Tubuh
    display.drawVLine(x, y + 3, 2);  // Kaki
}

void GameClient_svc::begin() {
    pinMode(pinL, INPUT_PULLUP);
    pinMode(pinR, INPUT_PULLUP);
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinOK, INPUT_PULLUP);
    initMap();
}

void GameClient_svc::runGame() {
    isGameRunning_StardewValleyCopy = true;

    while (isGameRunning_StardewValleyCopy) {
        appHeartBeat();

        // EXIT LOGIC
        if (digitalRead(pinA) == LOW && digitalRead(pinOK) == LOW) isGameRunning_StardewValleyCopy = false;

        // INPUT MOVEMENT
        float speed = 1.5;
        if (digitalRead(pinL) == LOW) pX -= speed;
        if (digitalRead(pinR) == LOW) pX += speed;
        if (digitalRead(pinA) == LOW) pY -= speed;
        if (digitalRead(pinOK) == LOW) pY += speed;

        // ACTION (Contoh: Tekan L + R bersamaan untuk cangkul)
        // Kamu bisa memodifikasi tombol sesuai kebutuhan
        int gridX = (int)pX / 8;
        int gridY = (int)pY / 8;
        if (gridX >= 0 && gridX < 16 && gridY >= 0 && gridY < 16) {
            if (digitalRead(pinL) == LOW && digitalRead(pinR) == LOW) {
                if (worldMap[gridX][gridY] == 0)
                    worldMap[gridX][gridY] = 1;  // Cangkul
                else if (worldMap[gridX][gridY] == 1)
                    worldMap[gridX][gridY] = 2;  // Tanam
            }
        }

        display.clearBuffer();

        // CAMERA CALCULATION (Player selalu di tengah layar)
        int camX = (int)pX - 64;
        int camY = (int)pY - 32;

        // DRAW TILES
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                int screenX = (i * 8) - camX;
                int screenY = (j * 8) - camY;
                // Hanya gambar tile yang ada di layar (Culling)
                if (screenX > -8 && screenX < 128 && screenY > -8 && screenY < 64) {
                    drawTile(i, j, screenX, screenY);
                }
            }
        }

        // DRAW PLAYER (Di posisi tengah layar relatif)
        drawPlayer(64, 32);

        // HUD
        display.setFont(u8g2_font_4x6_tf);
        display.setCursor(2, 60);
        display.print("X:");
        display.print((int)pX);
        display.print(" Y:");
        display.print((int)pY);
        display.setCursor(80, 60);
        display.print("FPS:");
        display.print(1000 / 16);  // Perkiraan

        display.sendBuffer();
        delay(10);
    }

    drawMenu();
}