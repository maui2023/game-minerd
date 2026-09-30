#ifndef SPACE_INVADERS_H
#define SPACE_INVADERS_H

#include <Arduino.h>
#include <LovyanGFX.hpp>

struct Bullet {
    int x, y;
    bool active;
};

struct Alien {
    int x, y;
    bool alive;
};

class SpaceInvadersGame {
public:
    int screenW;
    int screenH;
    int playerX;
    int playerY;
    int playerSpeed;
    int score;
    int lives;
    bool gameOver;

    static const int MAX_BULLETS = 4;
    Bullet bullets[MAX_BULLETS];

    static const int ALIEN_ROWS = 3;
    static const int ALIEN_COLS = 6;
    Alien aliens[ALIEN_ROWS][ALIEN_COLS];
    int alienDir;
    int alienSpeedX;
    int alienStepDown;
    unsigned long lastAlienMove;

    SpaceInvadersGame(int w = 320, int h = 240) {
        screenW = w;
        screenH = h;
        reset();
    }

    void reset() {
        score = 0;
        lives = 3;
        gameOver = false;
        playerX = screenW / 2 - 10;
        playerY = screenH - 35;
        playerSpeed = 6;

        for (int i = 0; i < MAX_BULLETS; i++) {
            bullets[i].active = false;
        }

        alienDir = 1;
        alienSpeedX = 4;
        alienStepDown = 8;
        lastAlienMove = millis();

        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                aliens[r][c].x = 30 + c * 40;
                aliens[r][c].y = 40 + r * 22;
                aliens[r][c].alive = true;
            }
        }
    }

    void moveLeft() {
        playerX -= playerSpeed;
        if (playerX < 5) playerX = 5;
    }

    void moveRight() {
        playerX += playerSpeed;
        if (playerX > screenW - 25) playerX = screenW - 25;
    }

    void fire() {
        if (gameOver) {
            reset();
            return;
        }
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].x = playerX + 8;
                bullets[i].y = playerY - 4;
                bullets[i].active = true;
                break;
            }
        }
    }

    void update() {
        if (gameOver) return;

        // Gerakkan peluru
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active) {
                bullets[i].y -= 8;
                if (bullets[i].y < 25) { // Had atas di bawah HUD
                    bullets[i].active = false;
                }

                // Semakan perlanggaran peluru dengan alien
                for (int r = 0; r < ALIEN_ROWS; r++) {
                    for (int c = 0; c < ALIEN_COLS; c++) {
                        if (aliens[r][c].alive) {
                            if (bullets[i].x >= aliens[r][c].x && bullets[i].x <= aliens[r][c].x + 18 &&
                                bullets[i].y >= aliens[r][c].y && bullets[i].y <= aliens[r][c].y + 14) {
                                aliens[r][c].alive = false;
                                bullets[i].active = false;
                                score += 50;
                                break;
                            }
                        }
                    }
                    if (!bullets[i].active) break;
                }
            }
        }

        // Gerakkan armada alien secara berperingkat
        if (millis() - lastAlienMove > 350) {
            lastAlienMove = millis();
            bool changeDir = false;

            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    if (aliens[r][c].alive) {
                        aliens[r][c].x += alienDir * alienSpeedX;
                        if (aliens[r][c].x > screenW - 30 || aliens[r][c].x < 10) {
                            changeDir = true;
                        }
                        // Jika alien sampai ke kapal pemain
                        if (aliens[r][c].y >= playerY - 10) {
                            gameOver = true;
                        }
                    }
                }
            }

            if (changeDir) {
                alienDir = -alienDir;
                for (int r = 0; r < ALIEN_ROWS; r++) {
                    for (int c = 0; c < ALIEN_COLS; c++) {
                        aliens[r][c].y += alienStepDown;
                    }
                }
            }
        }
    }

    template<typename T>
    void render(T& gfx) {
        // Lukis Kapal Pemain (Hijau / Emas Satoshi)
        gfx.fillRect(playerX + 6, playerY, 6, 4, TFT_GOLD);
        gfx.fillRect(playerX + 2, playerY + 4, 14, 6, TFT_GREEN);
        gfx.fillRect(playerX, playerY + 8, 18, 4, TFT_DARKGREEN);

        // Lukis Peluru Pemain
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active) {
                gfx.fillRect(bullets[i].x, bullets[i].y, 3, 6, TFT_YELLOW);
            }
        }

        // Lukis Armada Alien (Ungu / Merah)
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                if (aliens[r][c].alive) {
                    int ax = aliens[r][c].x;
                    int ay = aliens[r][c].y;
                    uint16_t color = (r == 0) ? TFT_MAGENTA : ((r == 1) ? TFT_CYAN : TFT_ORANGE);
                    gfx.fillRect(ax + 2, ay, 12, 10, color);
                    gfx.fillRect(ax, ay + 3, 16, 4, color);
                    gfx.drawPixel(ax + 4, ay + 4, TFT_BLACK);
                    gfx.drawPixel(ax + 11, ay + 4, TFT_BLACK);
                }
            }
        }

        // Paparan Skor di bawah
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.drawString("SCORE: " + String(score), 10, screenH - 14);
        gfx.drawString("LIVES: " + String(lives), screenW - 70, screenH - 14);

        if (gameOver) {
            gfx.fillRect(screenW / 2 - 80, screenH / 2 - 25, 160, 45, TFT_NAVY);
            gfx.drawRect(screenW / 2 - 80, screenH / 2 - 25, 160, 45, TFT_RED);
            gfx.setTextColor(TFT_RED, TFT_NAVY);
            gfx.drawCenterString("GAME OVER", screenW / 2, screenH / 2 - 18);
            gfx.setTextColor(TFT_WHITE, TFT_NAVY);
            gfx.drawCenterString("Tekan Tembak Semula", screenW / 2, screenH / 2);
        }
    }
};

#endif // SPACE_INVADERS_H
