#include "GameClient.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include "app/Game/GameMode.h"

void runMultiplayerGame(IPAddress ip);

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
    if (y < 0 || y >= 64) return 0;  // Batas dunia

    // 1. Cek Modifikasi Pemain (Block yang diletakkan/dihancurkan)
    for (int i = 0; i < modCount; i++) {
        if (mods[i].x == x && mods[i].y == y) return mods[i].type;
    }

    // 2. Prosedural Terrain (Ketinggian Tanah)
    // Menggabungkan beberapa gelombang agar tidak monoton
    float noiseH = sin(x * 0.15f) * 4.0f + cos(x * 0.05f) * 8.0f + sin(x * 0.02f) * 2.0f;
    int surfaceY = 32 + (int)noiseH;

    // 3. Sistem Gua (Caves)
    // Menggunakan noise 2D sederhana untuk melubangi tanah
    float caveNoise = (sin(x * 0.25f) * cos(y * 0.35f));
    if (y > surfaceY + 5 && caveNoise > 0.7f) return 0;  // Lubang gua

    // 4. Struktur: Pohon (Muncul setiap ~20 block)
    if (x % 20 == 0) {
        // Batang pohon (Log)
        if (y < surfaceY && y >= surfaceY - 3) return 4;
        // Daun (Leaves) - dibuat sedikit lebih lebar
        if (y < surfaceY - 3 && y >= surfaceY - 6) {
            if (abs(x % 20) <= 1) return 5;
        }
    }

    // 5. Struktur: Dungeon / Abandoned House (Muncul jarang ~120 block)
    int structPos = x % 120;
    if (structPos >= 0 && structPos <= 8) {
        int floorY = surfaceY - 1;
        int roofY = surfaceY - 6;

        if (y == roofY || y == floorY) return 3;  // Atap & Lantai Stone
        if ((structPos == 0 || structPos == 8) && (y < floorY && y > roofY)) {
            return 4;  // Tembok Kayu
        }
    }

    // 6. Layering Tanah (Mirip Minecraft)
    if (y < surfaceY) return 0;      // Udara (Air)
    if (y == surfaceY) return 2;     // Rumput (Grass)
    if (y < surfaceY + 4) return 1;  // Tanah (Dirt)

    // 7. Ore Distribution (Batubara/Besi di kedalaman)
    if (y > 50 && (sin(x * 0.5f) * cos(y * 0.5f)) > 0.8f) return 6;  // Anggap 6 adalah Ore

    return 3;  // Batu (Stone) di paling bawah
}

void GameClient::drawClouds() {
    for (int i = 0; i < 5; i++) {
        int cx = (i * 40) - (int)(camX * 2) % 200;
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
}

void GameClient::loadSettings() {
    File file = LittleFS.open("/GameClient/Minecraft/settings.json");
    if (!file) return;

    StaticJsonDocument<512> doc;
    deserializeJson(doc, file);

    this->gravity = doc["gravity"] | 48.0f;
    this->velJump = doc["jump_force"] | -18.0f;
    this->moveSpeed = doc["move_speed"] | 14.0f;
    this->currentBiome = doc["biome"].as<String>();

    file.close();
}

void GameClient::begin() {
    runGameMode();

    int menuIndex = 0;
    bool selected = false;

    while (!selected) {
        display.clearBuffer();
        display.setFontMode(1);
        display.setBitmapMode(1);
        display.setFont(u8g2_font_profont17_tr);
        display.drawStr(24, 20, "MINECRAFT");

        display.drawRBox(29, 26, 71, 9, 3);

        if (menuIndex == 0)
            display.drawRBox(29, 26, 71, 9, 3);
        else
            display.drawRFrame(29, 26, 71, 9, 3);

        display.setDrawColor(menuIndex == 0 ? 2 : 1);
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(41, 33, "Singleplayer");

        display.setDrawColor(1);

        if (menuIndex == 1)
            display.drawRBox(29, 37, 71, 9, 3);
        else
            display.drawRFrame(29, 37, 71, 9, 3);

        display.setDrawColor(menuIndex == 1 ? 2 : 1);
        display.drawStr(42, 44, "Multiplayer");

        display.setDrawColor(1);
        display.drawStr(1, 63, "C3OS(R) C++");
        display.sendBuffer();

        if (digitalRead(pinL) == LOW || digitalRead(pinR) == LOW) {
            menuIndex = !menuIndex;
            delay(150);
        }

        if (digitalRead(pinOK) == LOW) {
            selected = true;
            delay(200);
        }

        yield();
    }

    if (menuIndex == 0) {
        loadSettings();
        spawnMobs();
        playerY = 10;
        runGame();

        begin();
    } else {
        startMultiplayerSearch();
        begin();
    }
}

void GameClient::startMultiplayerSearch() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(22, 7, "Connecting to WiFi...");
    display.sendBuffer();

    WiFiManager wm;
    wm.setConfigPortalTimeout(1);

    if (!wm.autoConnect("ESP32C3", "123456789")) {
        display.setFont(u8g2_font_4x6_tr);

        display.drawStr(32, 20, "Connection Error");
        display.drawStr(30, 32, "Unable to connect");
        display.drawStr(30, 38, "to saved network!");
        display.drawStr(32, 49, "Try Again Later!");
        delay(2000);
        begin();
    }

    WiFiUDP udp;
    udp.begin(12346);

    display.clearBuffer();
    display.drawStr(10, 30, "Scanning Server...");
    display.sendBuffer();

    IPAddress broadcastIP(255, 255, 255, 255);
    udp.beginPacket(broadcastIP, 12345);
    udp.write((const uint8_t*)"MC_C3_PING", 10);
    udp.endPacket();

    unsigned long start = millis();
    bool found = false;

    while (millis() - start < 3000) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char reply[64];
            udp.read(reply, 64);

            IPAddress serverIP = udp.remoteIP();

            playerX = 50.0f;
            playerY = 25.0f;

            runMultiplayerGame(serverIP);
            found = true;
            break;
        }
        yield();
    }

    if (!found) {
        display.clearBuffer();
        display.drawStr(30, 35, "No Server Found!");
        display.sendBuffer();
        delay(2000);
    }
}

void GameClient::runMultiplayerGame(IPAddress serverIP) {
    WiFiUDP udp;
    udp.begin(12346);
    char packetBuffer[255];
    String currentMap = "";

    float vY = 0;
    bool onG = false;

    while (true) {
        vY += 0.25;
        if (vY > 2.5) vY = 2.5;
        playerY += vY;

        udp.beginPacket(serverIP, 12345);
        udp.printf("POS:%.1f,%.1f", playerX, playerY);
        udp.endPacket();

        int packetSize = udp.parsePacket();
        if (packetSize) {
            int len = udp.read(packetBuffer, 254);
            packetBuffer[len] = '\0';
            String msg = String(packetBuffer);
            if (msg.startsWith("MAP:")) currentMap = msg.substring(4);
        }

        if (currentMap.length() >= 180) {
            char blockUnder = currentMap[5 * 18 + 9];
            char blockBody = currentMap[4 * 18 + 9];

            char blockFront = (digitalRead(pinR) == LOW) ? currentMap[4 * 18 + 10] : currentMap[4 * 18 + 8];

            if (blockUnder != '0') {
                if (vY > 0) {
                    vY = 0;
                    onG = true;
                    playerY = floor(playerY);
                }
            } else {
                onG = false;
            }

            if (blockBody != '0') {
                playerY -= 1.0;
                vY = 0;
            }

            if (blockFront != '0' && onG) {
                vY = -2.2;
                onG = false;
            }
        }

        display.clearBuffer();

        display.setDrawColor(2);
        display.drawBox(64, 32, 4, 7);
        display.setDrawColor(1);
        display.sendBuffer();

        if (digitalRead(pinL) == LOW) playerX -= 0.5;
        if (digitalRead(pinR) == LOW) playerX += 0.5;
        if (digitalRead(pinL) == LOW && digitalRead(pinOK) == LOW) break;

        if (digitalRead(pinA) == LOW && digitalRead(pinOK) == LOW) {
            delay(250);
            break;
        }

        delay(15);
    }
}

void GameClient::runGame() {
    unsigned long lastTime = millis();
    while (true) {
        appHeartBeat();

        float dt = (millis() - lastTime) / 1000.0f;
        lastTime = millis();
        if (dt > 0.1f) dt = 0.016f;

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

        velY += gravity * dt;
        playerY += velY * dt;

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

        if (digitalRead(pinOK) == LOW && (onGround || velY > 0)) {
            velY = velJump;
            onGround = false;
            vTaskDelay(2);
        }

        if (digitalRead(pinA) == LOW) {
            addModification((int)(playerX + 0.5f), (int)(playerY + 1.1f), 0);
        }

        camX += (playerX - 8 - camX) * 0.1f;
        camY += (playerY - 4 - camY) * 0.1f;

        display.clearBuffer();
        drawClouds();

        for (int x = (int)camX; x < (int)camX + 18; x++) {
            for (int y = (int)camY; y < (int)camY + 10; y++) {
                uint8_t b = getBlockAt(x, y);
                if (b == 0) continue;
                int sx = (x - camX) * 8;
                int sy = (y - camY) * 8;
                if (b == 2) display.drawBox(sx, sy, 8, 2);
                if (b == 5) {
                    display.drawFrame(sx + 1, sy + 1, 6, 6);
                } else {
                    display.drawFrame(sx, sy, 8, 8);
                }
            }
        }

        for (int i = 0; i < MAX_MOBS; i++) {
            mobs[i].x += mobs[i].vx;
            int mx = (mobs[i].x - camX) * 8;
            if (mx > -10 && mx < 130) {
                display.drawFrame(mx + 2, (playerY - camY) * 8, 4, 8);
            }
        }

        display.setDrawColor(2);
        display.drawBox((playerX - camX) * 8 + 2, (playerY - camY) * 8, 4, 8);
        display.setDrawColor(1);

        drawHUD(1.0f / dt);
        display.sendBuffer();

        if (digitalRead(pinL) == LOW && digitalRead(pinOK) == LOW && digitalRead(pinA) == LOW) break;
        vTaskDelay(1);
    }
}