#ifndef NES_GAME_H
#define NES_GAME_H

#include <Arduino.h>
#include <LovyanGFX.hpp>

// Struktur entiti untuk Game Retro 8-bit NES
struct NesLaser {
    float x, y;
    bool active;
};

struct NesEnemy {
    float x, y;
    float vx, vy;
    int type; // 0 = Asteroid, 1 = Enemy Ship, 2 = Bitcoin Gem
    bool active;
};

struct NesParticle {
    float x, y;
    float vx, vy;
    uint16_t color;
    uint8_t life;
};

class NesGameEngine {
public:
    // Keadaan Butang Maya Touch Pad
    bool btnUpPressed = false;
    bool btnDownPressed = false;
    bool btnLeftPressed = false;
    bool btnRightPressed = false;
    bool btnAPressed = false;
    bool btnBPressed = false;
    bool btnSelectPressed = false;
    bool btnStartPressed = false;

    // Keadaan Permainan
    float playerX = 160.0f;
    float playerY = 120.0f;
    float playerSpeed = 3.2f;
    int score = 0;
    int lives = 3;
    bool isPaused = false;
    bool isGameOver = false;

    static const int MAX_LASERS = 12;
    NesLaser lasers[MAX_LASERS];

    static const int MAX_ENEMIES = 8;
    NesEnemy enemies[MAX_ENEMIES];

    static const int MAX_PARTICLES = 24;
    NesParticle particles[MAX_PARTICLES];

    unsigned long lastSpawnTime = 0;
    unsigned long lastFireTime = 0;
    unsigned long lastFrameTime = 0;
    int frameCounter = 0;

    // Dimensi Skrin Retro NES (Di tengah skrin 320x240)
    const int scrX = 52;
    const int scrY = 25;
    const int scrW = 216;
    const int scrH = 138;

    bool needFullBezelRedraw = true;

    NesGameEngine() {
        reset();
    }

    void reset() {
        playerX = scrX + scrW / 2;
        playerY = scrY + scrH - 24;
        score = 0;
        lives = 3;
        isPaused = false;
        isGameOver = false;

        for (int i = 0; i < MAX_LASERS; i++) lasers[i].active = false;
        for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
        for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;

        needFullBezelRedraw = true;
    }

    void spawnParticle(float x, float y, uint16_t color) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].life == 0) {
                particles[i].x = x;
                particles[i].y = y;
                particles[i].vx = ((float)(rand() % 40) - 20.0f) / 10.0f;
                particles[i].vy = ((float)(rand() % 40) - 20.0f) / 10.0f;
                particles[i].color = color;
                particles[i].life = 8 + (rand() % 8);
                break;
            }
        }
    }

    void spawnEnemy() {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].x = scrX + 10 + (rand() % (scrW - 20));
                enemies[i].y = scrY + 2;
                enemies[i].vx = ((float)(rand() % 20) - 10.0f) / 12.0f;
                enemies[i].vy = 0.8f + ((float)(rand() % 15) / 10.0f);
                enemies[i].type = (rand() % 10 == 0) ? 2 : (rand() % 3 == 0 ? 1 : 0);
                enemies[i].active = true;
                break;
            }
        }
    }

    void fireLaser() {
        if (millis() - lastFireTime < 180) return;
        lastFireTime = millis();

        for (int i = 0; i < MAX_LASERS; i++) {
            if (!lasers[i].active) {
                lasers[i].x = playerX;
                lasers[i].y = playerY - 6;
                lasers[i].active = true;
                break;
            }
        }
    }

    // Mengendalikan input sentuhan daripada skrin sentuh CYD (D-Pad, A, B, Start, Select)
    // Pulangkan nilai true jika butang [HUB / KELUAR] disentuh untuk kembali ke menu
    bool handleTouch(int touchX, int touchY, bool isTouching) {
        if (!isTouching) {
            btnUpPressed = false;
            btnDownPressed = false;
            btnLeftPressed = false;
            btnRightPressed = false;
            btnAPressed = false;
            btnBPressed = false;
            btnSelectPressed = false;
            btnStartPressed = false;
            return false;
        }

        // 1. Butang HUB / KELUAR di sudut atas kanan (x=260..318, y=0..24)
        if (touchX >= 260 && touchX <= 318 && touchY <= 24) {
            return true; // Isyarat keluar ke Game Hub
        }

        // 2. Butang Restart jika Game Over
        if (isGameOver && touchY >= scrY && touchY <= scrY + scrH) {
            reset();
            return false;
        }

        // 3. Kawalan D-Pad Maya (Sebelah Kiri: x=0..64, y=110..238)
        if (touchX <= 64 && touchY >= 110) {
            // UP
            if (touchY <= 155 && touchX >= 10 && touchX <= 45) {
                btnUpPressed = true;
            }
            // DOWN
            else if (touchY >= 195 && touchX >= 10 && touchX <= 45) {
                btnDownPressed = true;
            }
            // LEFT
            else if (touchX <= 26 && touchY >= 155 && touchY <= 195) {
                btnLeftPressed = true;
            }
            // RIGHT
            else if (touchX >= 30 && touchY >= 155 && touchY <= 195) {
                btnRightPressed = true;
            }
        }

        // 4. Kawalan Butang Tindakan A & B (Sebelah Kanan: x=256..320, y=120..238)
        if (touchX >= 256 && touchY >= 120) {
            // Butang A (Atas Kanan: x=286..318, y=135..185)
            if (touchX >= 284 && touchY <= 185) {
                btnAPressed = true;
                fireLaser();
            }
            // Butang B (Bawah Kiri: x=256..290, y=175..225)
            else if (touchX < 290 && touchY >= 175) {
                btnBPressed = true;
                fireLaser();
            }
        }

        // 5. Butang SELECT & START (Bawah Tengah: y=200..238)
        if (touchY >= 200) {
            // SELECT (x=80..135)
            if (touchX >= 80 && touchX <= 135) {
                btnSelectPressed = true;
                playerSpeed = (playerSpeed == 3.2f) ? 4.8f : 3.2f; // Turbo toggle
            }
            // START (x=185..240)
            else if (touchX >= 185 && touchX <= 240) {
                btnStartPressed = true;
                isPaused = !isPaused;
            }
        }

        return false;
    }

    template<typename T>
    void updateAndRender(T& gfx) {
        frameCounter++;

        // 1. Lukis rangka luar bezel konsol Famicom retro & butang sentuh (Hanya perlu lukis sekali atau bila status bertukar)
        if (needFullBezelRedraw) {
            drawBezelAndControls(gfx);
            needFullBezelRedraw = false;
        }

        if (isPaused) {
            gfx.fillRect(scrX + 40, scrY + 50, scrW - 80, 28, TFT_BLACK);
            gfx.drawRect(scrX + 40, scrY + 50, scrW - 80, 28, TFT_YELLOW);
            gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
            gfx.setTextSize(1);
            gfx.drawCenterString(">> GAME PAUSED <<", scrX + scrW / 2, scrY + 58);
            return;
        }

        if (isGameOver) {
            gfx.fillRect(scrX + 30, scrY + 45, scrW - 60, 42, TFT_MAROON);
            gfx.drawRect(scrX + 30, scrY + 45, scrW - 60, 42, TFT_WHITE);
            gfx.setTextColor(TFT_WHITE, TFT_MAROON);
            gfx.setTextSize(1);
            gfx.drawCenterString("GAME OVER", scrX + scrW / 2, scrY + 52);
            gfx.setTextColor(TFT_YELLOW, TFT_MAROON);
            gfx.drawCenterString("Sentuh Skrin Main Semula", scrX + scrW / 2, scrY + 68);
            return;
        }

        // 2. Logik Pergerakan Pemain melalui D-Pad Maya
        if (btnLeftPressed)  playerX -= playerSpeed;
        if (btnRightPressed) playerX += playerSpeed;
        if (btnUpPressed)    playerY -= playerSpeed;
        if (btnDownPressed)  playerY += playerSpeed;

        // Had sempadan skrin CRT
        if (playerX < scrX + 8) playerX = scrX + 8;
        if (playerX > scrX + scrW - 8) playerX = scrX + scrW - 8;
        if (playerY < scrY + 12) playerY = scrY + 12;
        if (playerY > scrY + scrH - 10) playerY = scrY + scrH - 10;

        // 3. Jana Musuh Baru
        if (millis() - lastSpawnTime > 1400) {
            spawnEnemy();
            lastSpawnTime = millis();
        }

        // 4. Bersihkan Skrin CRT (Warna Hitam Angkasa 8-Bit)
        gfx.startWrite();
        gfx.fillRect(scrX + 2, scrY + 2, scrW - 4, scrH - 4, gfx.color565(8, 12, 24));

        // Bintang latar belakang (Parallax Stars)
        for (int s = 0; s < 12; s++) {
            int sx = scrX + ((s * 37 + frameCounter * 2) % (scrW - 8)) + 4;
            int sy = scrY + ((s * 23 + frameCounter * (s % 2 + 1)) % (scrH - 8)) + 4;
            gfx.drawPixel(sx, sy, (s % 2 == 0) ? TFT_WHITE : TFT_CYAN);
        }

        // 5. Kemas Kini & Lukis Tembakan Laser
        for (int i = 0; i < MAX_LASERS; i++) {
            if (lasers[i].active) {
                lasers[i].y -= 5.5f;
                if (lasers[i].y < scrY + 4) {
                    lasers[i].active = false;
                } else {
                    gfx.drawFastVLine((int)lasers[i].x, (int)lasers[i].y, 5, TFT_YELLOW);
                    gfx.drawPixel((int)lasers[i].x + 1, (int)lasers[i].y + 2, TFT_WHITE);
                }
            }
        }

        // 6. Kemas Kini & Lukis Musuh / Bitcoin Gems
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                enemies[i].x += enemies[i].vx;
                enemies[i].y += enemies[i].vy;

                // Pantulan dinding
                if (enemies[i].x < scrX + 6 || enemies[i].x > scrX + scrW - 6) {
                    enemies[i].vx = -enemies[i].vx;
                }

                // Terkeluar bawah
                if (enemies[i].y > scrY + scrH - 4) {
                    enemies[i].active = false;
                    continue;
                }

                int ex = (int)enemies[i].x;
                int ey = (int)enemies[i].y;

                // Lukis mengikut jenis
                if (enemies[i].type == 0) {
                    // Asteroid 8-bit
                    gfx.fillRect(ex - 4, ey - 4, 8, 8, gfx.color565(180, 83, 9));
                    gfx.drawRect(ex - 4, ey - 4, 8, 8, TFT_YELLOW);
                } else if (enemies[i].type == 1) {
                    // Alien Ship 8-bit
                    gfx.fillRect(ex - 5, ey - 3, 10, 6, TFT_MAGENTA);
                    gfx.drawPixel(ex, ey + 4, TFT_RED);
                } else {
                    // Bitcoin Gem (+50 mata)
                    gfx.fillCircle(ex, ey, 4, TFT_GOLD);
                    gfx.setTextColor(TFT_BLACK, TFT_GOLD);
                    gfx.drawString("B", ex - 2, ey - 3);
                }

                // Pelanggaran Laser dengan Musuh
                for (int l = 0; l < MAX_LASERS; l++) {
                    if (lasers[l].active) {
                        if (abs(lasers[l].x - ex) < 8 && abs(lasers[l].y - ey) < 8) {
                            lasers[l].active = false;
                            enemies[i].active = false;
                            score += (enemies[i].type == 2) ? 50 : 20;

                            // Partikel letupan
                            spawnParticle(ex, ey, (enemies[i].type == 2) ? TFT_GOLD : TFT_ORANGE);
                            break;
                        }
                    }
                }

                // Pelanggaran Musuh dengan Pemain
                if (enemies[i].active && abs(playerX - ex) < 9 && abs(playerY - ey) < 9) {
                    enemies[i].active = false;
                    if (enemies[i].type == 2) {
                        score += 100; // Bonus gem
                        spawnParticle(ex, ey, TFT_GOLD);
                    } else {
                        lives--;
                        spawnParticle(playerX, playerY, TFT_RED);
                        if (lives <= 0) {
                            isGameOver = true;
                        }
                    }
                }
            }
        }

        // 7. Kemas Kini & Lukis Partikel Letupan
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (particles[p].life > 0) {
                particles[p].x += particles[p].vx;
                particles[p].y += particles[p].vy;
                particles[p].life--;
                gfx.drawPixel((int)particles[p].x, (int)particles[p].y, particles[p].color);
            }
        }

        // 8. Lukis Watak Pemain (Kapal Angkasa Retro 8-Bit)
        int px = (int)playerX;
        int py = (int)playerY;
        gfx.fillTriangle(px, py - 7, px - 6, py + 5, px + 6, py + 5, TFT_CYAN);
        gfx.fillRect(px - 2, py + 1, 4, 5, TFT_WHITE);
        gfx.drawPixel(px, py + 6, TFT_RED); // Enjin jet

        // 9. Status Bar Dalam Skrin Retro (Skor & Nyawa)
        gfx.fillRect(scrX + 4, scrY + 3, scrW - 8, 11, gfx.color565(15, 23, 42));
        gfx.setTextColor(TFT_GREENYELLOW, gfx.color565(15, 23, 42));
        gfx.setTextSize(1);
        char sBuf[48];
        snprintf(sBuf, sizeof(sBuf), "MATA:%04d | NYAWA:%d | NES-8BIT", score, lives);
        gfx.drawString(sBuf, scrX + 6, scrY + 4);

        gfx.endWrite();
    }

    template<typename T>
    void drawBezelAndControls(T& gfx) {
        gfx.startWrite();

        // Latar Belakang Konsol (Famicom Off-White / Dark Retro Bezel)
        gfx.fillScreen(gfx.color565(20, 24, 38));

        // Rangka CRT Luar Bezel (Kotak Kaca TV)
        gfx.drawRoundRect(scrX - 2, scrY - 2, scrW + 4, scrH + 4, 6, gfx.color565(71, 85, 105));
        gfx.drawRect(scrX, scrY, scrW, scrH, gfx.color565(56, 189, 248));

        // Header Atas: Butang [ HUB / KELUAR ]
        gfx.fillRoundRect(262, 2, 54, 18, 4, gfx.color565(225, 29, 72)); // Rose Red
        gfx.drawRoundRect(262, 2, 54, 18, 4, TFT_WHITE);
        gfx.setTextColor(TFT_WHITE, gfx.color565(225, 29, 72));
        gfx.setTextSize(1);
        gfx.drawCenterString("MENU", 262 + 27, 4);

        // Header Kiri Atas: Label Katrij NES
        gfx.setTextColor(TFT_GOLD, gfx.color565(20, 24, 38));
        gfx.drawString("NES RETRO: 22-in-1 (640KB)", 8, 6);

        // Sub-garisan Bezel TV
        gfx.setTextColor(TFT_SILVER, gfx.color565(20, 24, 38));
        gfx.drawCenterString(">> KAWALAN SKRIN SENTUH (D-PAD & A/B) <<", scrX + scrW / 2, scrY + scrH + 5);

        // ================= KAWALAN D-PAD KIRI =================
        int dPadCenterX = 26;
        int dPadCenterY = 180;
        int armW = 16;
        int armH = 20;

        // Salib Asas D-Pad
        gfx.fillRoundRect(dPadCenterX - armW / 2, dPadCenterY - armH - 4, armW, (armH * 2) + 8, 4, gfx.color565(30, 41, 59));
        gfx.fillRoundRect(dPadCenterX - armH - 4, dPadCenterY - armW / 2, (armH * 2) + 8, armW, 4, gfx.color565(30, 41, 59));
        gfx.drawRoundRect(dPadCenterX - armW / 2, dPadCenterY - armH - 4, armW, (armH * 2) + 8, 4, gfx.color565(71, 85, 105));
        gfx.drawRoundRect(dPadCenterX - armH - 4, dPadCenterY - armW / 2, (armH * 2) + 8, armW, 4, gfx.color565(71, 85, 105));

        // Anak panah D-Pad
        gfx.setTextColor(TFT_WHITE, gfx.color565(30, 41, 59));
        gfx.drawCenterString("^", dPadCenterX, dPadCenterY - 18);
        gfx.drawCenterString("v", dPadCenterX, dPadCenterY + 10);
        gfx.drawCenterString("<", dPadCenterX - 15, dPadCenterY - 4);
        gfx.drawCenterString(">", dPadCenterX + 15, dPadCenterY - 4);

        // ================= KAWALAN BUTANG KANAN (A & B) =================
        // Butang B (Kuning / Jingga)
        int btnBX = 274;
        int btnBY = 196;
        gfx.fillCircle(btnBX, btnBY, 14, gfx.color565(217, 119, 6)); // Amber
        gfx.drawCircle(btnBX, btnBY, 14, TFT_WHITE);
        gfx.setTextColor(TFT_WHITE, gfx.color565(217, 119, 6));
        gfx.drawString("B", btnBX - 3, btnBY - 4);

        // Butang A (Merah Famicom)
        int btnAX = 302;
        int btnAY = 162;
        gfx.fillCircle(btnAX, btnAY, 14, gfx.color565(220, 38, 38)); // Crimson Red
        gfx.drawCircle(btnAX, btnAY, 14, TFT_WHITE);
        gfx.setTextColor(TFT_WHITE, gfx.color565(220, 38, 38));
        gfx.drawString("A", btnAX - 3, btnAY - 4);

        // ================= BUTANG SELECT & START TENGAH =================
        // SELECT
        gfx.fillRoundRect(86, 208, 44, 18, 4, gfx.color565(51, 65, 85));
        gfx.drawRoundRect(86, 208, 44, 18, 4, TFT_SILVER);
        gfx.setTextColor(TFT_WHITE, gfx.color565(51, 65, 85));
        gfx.drawCenterString("SELECT", 86 + 22, 210);

        // START
        gfx.fillRoundRect(190, 208, 44, 18, 4, gfx.color565(51, 65, 85));
        gfx.drawRoundRect(190, 208, 44, 18, 4, TFT_SILVER);
        gfx.setTextColor(TFT_WHITE, gfx.color565(51, 65, 85));
        gfx.drawCenterString("START", 190 + 22, 210);

        gfx.endWrite();
    }
};

#endif // NES_GAME_H
