#include "FlappyBird.h"

#include <Arduino.h>

#include "esp_task_wdt.h"

static bool jumpPressed_FlappyBird = false;
static int okPressed_FlappyBird = 0;

void runFlappyBird() {
    struct {
        float bird_y = 32;
        float vel = 0;
        float gravity = 0.07;

        int pipeX = 128;
        int gapY = 28;
        int gapSize = 30;

        int score = 0;
        bool running = true;

        unsigned long lastFrame = 0;
    } game;

    btnUp.attachClick([]() { jumpPressed_FlappyBird = true; });

    btnOK.attachClick([]() { okPressed_FlappyBird = 1; });
    btnOK.attachLongPressStart([]() { okPressed_FlappyBird = 2; });

    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();

        btnUp.tick();
        btnOK.tick();

        if (okPressed_FlappyBird == 2) break;

        unsigned long now = millis();
        if (now - game.lastFrame < 16) continue;
        game.lastFrame = now;

        if (game.running) {
            if (jumpPressed_FlappyBird) {
                game.vel = -2.6;
                jumpPressed_FlappyBird = false;
            }

            game.vel += game.gravity;
            game.bird_y += game.vel;

            if (game.bird_y < 0) game.bird_y = 0;
            if (game.bird_y > 63) {
                game.bird_y = 63;
                game.running = false;
            }

            game.pipeX -= 2;
            if (game.pipeX < -10) {
                game.pipeX = 128;
                game.gapY = random(10, 40);
                game.score++;
            }

            if (game.pipeX < 18 && game.pipeX > 10) {
                if (game.bird_y < game.gapY || game.bird_y > game.gapY + game.gapSize) {
                    game.running = false;
                }
            }
        } else {
            if (okPressed_FlappyBird == 1) {
                game = {};
                game.gravity = 0.07;
                game.bird_y = 32;
                game.vel = 0;
                jumpPressed_FlappyBird = false;
                okPressed_FlappyBird = 0;
            }
        }

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);

        display.drawDisc(15, (int)game.bird_y, 3);

        display.drawBox(game.pipeX, 0, 10, game.gapY);
        display.drawBox(game.pipeX, game.gapY + game.gapSize, 10, 64);

        display.setCursor(55, 10);
        display.print(game.score);

        if (!game.running) {
            display.setFont(u8g2_font_5x8_tf);
            display.setCursor(40, 35);
            display.print("GAME OVER!");
            display.setCursor(5, 48);
            display.print("OK=Retry  |  Hold=Exit");
        }

        display.sendBuffer();
        yield();
        vTaskDelay(1);
    }

    detachCallback();
    okPressed_FlappyBird = 0;
    drawMenu();
}
