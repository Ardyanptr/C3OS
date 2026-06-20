#include "Pong.h"

#include <Arduino.h>

#include "../../component/button.h"
#include "../../component/draw.h"
#include "OneButton.h"
#include "U8g2lib.h"
#include "esp_task_wdt.h"

float smoothBallX = 64.0f;
float smoothBallY = 32.0f;
float smoothPaddle1Y = 32.0f;
float smoothPaddle2Y = 32.0f;
float animationSpeed = 0.3f;

extern void drawMenu();

bool okPressed = false, actionPressed = false;

void runPongGame() {
    struct {
        int paddleY = 25, aiPaddleY = 25;
        int ballX = 64, ballY = 32;
        float ballSpeedX = 1.0f, ballSpeedY = 0.2f;
        int playerScore = 0, aiScore = 0;
        bool gameRunning = false;
        unsigned long lastUpdate = 0;
    } game;

    smoothBallX = game.ballX;
    smoothBallY = game.ballY;
    smoothPaddle1Y = game.paddleY;
    smoothPaddle2Y = game.aiPaddleY;

    btnOK.attachClick([]() { okPressed = true; });
    btnAction.attachClick([]() { actionPressed = true; });

    // Check if the game running or not
    if (game.gameRunning == false) game.gameRunning = true;

    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();

        unsigned long now = millis();
        if (game.gameRunning && now - game.lastUpdate > 20) {
            game.lastUpdate = now;

            game.ballX += game.ballSpeedX;
            game.ballY += game.ballSpeedY;

            if (game.ballY <= 0) {
                game.ballY = 0;
                game.ballSpeedY = -game.ballSpeedY;
            }
            if (game.ballY >= 63) {
                game.ballY = 63;
                game.ballSpeedY = -game.ballSpeedY;
            }

            if (game.ballX <= 3 && game.ballX >= 0 && game.ballY >= game.paddleY &&
                game.ballY <= game.paddleY + 15) {
                game.ballX = 3;
                game.ballSpeedX = fabs(game.ballSpeedX);
                int hitPos = game.ballY - game.paddleY - 7;
                game.ballSpeedY = hitPos / 3.0f;
                if (fabs(game.ballSpeedY) < 1.0f) game.ballSpeedY = (random(2) ? 1.0f : -1.0f);
            }

            if (game.ballX >= 125 && game.ballX <= 128 && game.ballY >= game.aiPaddleY &&
                game.ballY <= game.aiPaddleY + 15) {
                game.ballX = 125;
                game.ballSpeedX = -fabs(game.ballSpeedX);
                int hitPos = game.ballY - game.aiPaddleY - 7;
                game.ballSpeedY = hitPos / 3.0f;
                if (fabs(game.ballSpeedY) < 1.0f) game.ballSpeedY = (random(2) ? 1.0f : -1.0f);
            }

            if (game.ballX < 0) {
                game.aiScore++;
                game.ballX = 64;
                game.ballY = 32;
                smoothBallX = 64;
                smoothBallY = 32;
                game.ballSpeedX = 1.0f;
                game.ballSpeedY = 0.3f;
            }
            if (game.ballX > 128) {
                game.playerScore++;
                game.ballX = 64;
                game.ballY = 32;
                smoothBallX = 64;
                smoothBallY = 32;
                game.ballSpeedX = -2.0f;
                game.ballSpeedY = 0.5f;
            }

            float diff = (game.ballY - (game.aiPaddleY + 7.5f));
            game.aiPaddleY += constrain(diff * 0.3f, -2.0f, 2.0f);
            if (game.aiPaddleY < 0) game.aiPaddleY = 0;
            if (game.aiPaddleY > 49) game.aiPaddleY = 49;

            smoothBallX += (game.ballX - smoothBallX) * animationSpeed;
            smoothBallY += (game.ballY - smoothBallY) * animationSpeed;
            smoothPaddle2Y += (game.aiPaddleY - smoothPaddle2Y) * animationSpeed;
        }

        btnOK.tick();
        btnAction.tick();

        const int BUTTON_UP_PIN = 0;
        const int BUTTON_DOWN_PIN = 1;

        if (digitalRead(BUTTON_UP_PIN) == LOW) {
            game.paddleY = max(0, game.paddleY - 3);
        }
        if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
            game.paddleY = min(49, game.paddleY + 3);
        }
        if (okPressed) {
            if (game.playerScore >= 5 || game.aiScore >= 5) {
                game.playerScore = 0;
                game.aiScore = 0;
            }
            game.gameRunning = !game.gameRunning;
            okPressed = false;
        }
        if (actionPressed) break;

        smoothPaddle1Y += (game.paddleY - smoothPaddle1Y) * animationSpeed;

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.setCursor(20, 10);
        display.print(game.playerScore);
        display.setCursor(100, 10);
        display.print(game.aiScore);

        for (int y = 0; y < 64; y += 4)
            display.drawPixel(64, y);

        display.drawBox(2, (int)smoothPaddle1Y, 2, 15);
        display.drawBox(126, (int)smoothPaddle2Y, 2, 15);
        display.drawDisc((int)smoothBallX, (int)smoothBallY, 2);

        if (!game.gameRunning) {
            display.setFont(u8g2_font_5x8_tf);
            display.setCursor(40, 40);
            display.print("OK: START");
            display.setCursor(35, 50);
            display.print("ACTION: EXIT");
        }

        if (game.playerScore >= 5) {
            display.setFont(u8g2_font_6x10_tf);
            display.setCursor(40, 35);
            display.print("YOU WIN!");
            game.gameRunning = false;
        }
        if (game.aiScore >= 5) {
            display.setFont(u8g2_font_6x10_tf);
            display.setCursor(45, 35);
            display.print("AI WINS!");
            game.gameRunning = false;
        }

        display.sendBuffer();
        delay(16);
        yield();
        vTaskDelay(1);
    }

    btnOK.attachClick(nullptr);
    btnAction.attachClick(nullptr);
    drawMenu();
}
