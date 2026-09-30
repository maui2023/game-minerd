#ifndef SNAKES_AND_LADDERS_H
#define SNAKES_AND_LADDERS_H

#include <Arduino.h>
#include <LovyanGFX.hpp>

// Jumlah petak papan
#define TOTAL_TILES 30
#define BOARD_COLS 6
#define BOARD_ROWS 5

struct TilePos {
    int x, y;
};

class SnakesAndLaddersGame {
public:
    int screenW;
    int screenH;
    int playerPos;     // 1 hingga 30
    int botPos;        // 1 hingga 30
    int oldPlayerPos;
    int oldBotPos;
    bool isPlayerTurn; // true = Player, false = Bot
    int lastDice;
    bool isRolling;
    unsigned long rollStartTime;
    char statusMsg[48];
    bool gameWon;
    int winner; // 1 = Player, 2 = Bot

    bool needBoardRedraw; // Flag redraw penuh papan
    bool needPanelRedraw; // Flag redraw panel kawalan kanan

    // Butang sentuh "BALING DADU"
    int btnX, btnY, btnW, btnH;

    SnakesAndLaddersGame(int w = 320, int h = 240) {
        screenW = w;
        screenH = h;
        btnX = 4;
        btnY = 124;
        btnW = 98;
        btnH = 46;
        reset();
    }

    void reset() {
        playerPos = 1;
        botPos = 1;
        oldPlayerPos = 1;
        oldBotPos = 1;
        isPlayerTurn = true;
        lastDice = 1;
        isRolling = false;
        gameWon = false;
        winner = 0;
        needBoardRedraw = true;
        needPanelRedraw = true;
        strncpy(statusMsg, "Tap butang baling!", sizeof(statusMsg) - 1);
    }

    TilePos getTileCoordinates(int tileNum) {
        if (tileNum < 1) tileNum = 1;
        if (tileNum > TOTAL_TILES) tileNum = TOTAL_TILES;

        int index = tileNum - 1;
        int rowFromBottom = index / BOARD_COLS;
        int col = index % BOARD_COLS;

        if (rowFromBottom % 2 == 1) {
            col = (BOARD_COLS - 1) - col;
        }

        int row = (BOARD_ROWS - 1) - rowFromBottom;

        int tileW = 34;
        int tileH = 39;
        int boardStartX = 6;
        int boardStartY = 28;

        TilePos pos;
        pos.x = boardStartX + col * tileW;
        pos.y = boardStartY + row * tileH;
        return pos;
    }

    int checkSnakesAndLadders(int pos, bool isPlayer) {
        // TANGGA
        if (pos == 3) {
            strncpy(statusMsg, isPlayer ? "P1: Tangga (+8)!" : "Bot: Tangga (+8)!", sizeof(statusMsg) - 1);
            return 11;
        }
        if (pos == 8) {
            strncpy(statusMsg, isPlayer ? "P1: Halving Pump (+9)!" : "Bot: Halving (+9)!", sizeof(statusMsg) - 1);
            return 17;
        }
        if (pos == 15) {
            strncpy(statusMsg, isPlayer ? "P1: Lightning (+11)!" : "Bot: Lightning (+11)!", sizeof(statusMsg) - 1);
            return 26;
        }
        if (pos == 21) {
            strncpy(statusMsg, isPlayer ? "P1: ATH Breakout (+8)!" : "Bot: ATH (+8)!", sizeof(statusMsg) - 1);
            return 29;
        }

        // ULAR
        if (pos == 14) {
            strncpy(statusMsg, isPlayer ? "P1: Ular FUD (-10)!" : "Bot: Ular FUD (-10)!", sizeof(statusMsg) - 1);
            return 4;
        }
        if (pos == 19) {
            strncpy(statusMsg, isPlayer ? "P1: Bear Dip (-10)!" : "Bot: Bear Dip (-10)!", sizeof(statusMsg) - 1);
            return 9;
        }
        if (pos == 24) {
            strncpy(statusMsg, isPlayer ? "P1: Crypto Winter (-12)!" : "Bot: Winter (-12)!", sizeof(statusMsg) - 1);
            return 12;
        }
        if (pos == 28) {
            strncpy(statusMsg, isPlayer ? "P1: Whale Dump (-12)!" : "Bot: Dump (-12)!", sizeof(statusMsg) - 1);
            return 16;
        }

        return pos;
    }

    void rollDice() {
        if (gameWon) {
            reset();
            return;
        }
        if (isRolling) return;

        isRolling = true;
        rollStartTime = millis();
        needPanelRedraw = true;
    }

    bool handleTouch(int touchX, int touchY) {
        if (gameWon) {
            reset();
            return true;
        }

        if (touchX >= 200 && touchY >= 25) {
            if (isPlayerTurn && !isRolling) {
                rollDice();
                return true;
            }
        }
        return false;
    }

    template<typename T>
    void update(T& gfx) {
        if (isRolling) {
            lastDice = random(1, 7);
            needPanelRedraw = true;

            if (millis() - rollStartTime > 550) {
                isRolling = false;

                if (isPlayerTurn) {
                    oldPlayerPos = playerPos;
                    playerPos += lastDice;
                    if (playerPos >= TOTAL_TILES) {
                        playerPos = TOTAL_TILES;
                        gameWon = true;
                        winner = 1;
                        strncpy(statusMsg, "🎉 ANDA MENANG ATH!", sizeof(statusMsg) - 1);
                    } else {
                        int newPos = checkSnakesAndLadders(playerPos, true);
                        if (newPos == playerPos) {
                            snprintf(statusMsg, sizeof(statusMsg), "P1: [%d] -> Petak %d", lastDice, playerPos);
                        }
                        playerPos = newPos;
                        isPlayerTurn = false;
                    }
                    drawSingleTile(gfx, oldPlayerPos);
                    drawSingleTile(gfx, playerPos);
                } else {
                    oldBotPos = botPos;
                    botPos += lastDice;
                    if (botPos >= TOTAL_TILES) {
                        botPos = TOTAL_TILES;
                        gameWon = true;
                        winner = 2;
                        strncpy(statusMsg, "Satoshi Bot Menang!", sizeof(statusMsg) - 1);
                    } else {
                        int newPos = checkSnakesAndLadders(botPos, false);
                        if (newPos == botPos) {
                            snprintf(statusMsg, sizeof(statusMsg), "Bot: [%d] -> Petak %d", lastDice, botPos);
                        }
                        botPos = newPos;
                        isPlayerTurn = true;
                    }
                    drawSingleTile(gfx, oldBotPos);
                    drawSingleTile(gfx, botPos);
                }
                needPanelRedraw = true;
            }
        } else if (!isPlayerTurn && !gameWon) {
            static unsigned long botWait = 0;
            if (botWait == 0) botWait = millis();
            if (millis() - botWait > 700) {
                botWait = 0;
                rollDice();
            }
        }
    }

    template<typename T>
    void drawSingleTile(T& gfx, int i) {
        if (i < 1 || i > TOTAL_TILES) return;

        TilePos p = getTileCoordinates(i);
        int w = 33;
        int h = 38;

        uint16_t bgColor = ((i / BOARD_COLS + i % BOARD_COLS) % 2 == 0) 
                           ? gfx.color565(30, 41, 59)   // Dark Slate
                           : gfx.color565(51, 65, 85);  // Slate Blue

        if (i == 3 || i == 8 || i == 15 || i == 21) {
            bgColor = gfx.color565(16, 120, 60); // Hijau Tangga
        } else if (i == 14 || i == 19 || i == 24 || i == 28) {
            bgColor = gfx.color565(140, 30, 30); // Merah Ular
        } else if (i == 30) {
            bgColor = gfx.color565(180, 140, 20); // Emas ATH
        }

        gfx.fillRect(p.x, p.y, w, h, bgColor);
        gfx.drawRect(p.x, p.y, w, h, gfx.color565(71, 85, 105));

        gfx.setTextSize(1);
        gfx.setTextColor(TFT_WHITE, bgColor);
        gfx.drawString(String(i), p.x + 3, p.y + 3);

        if (i == 3 || i == 8 || i == 15 || i == 21) {
            gfx.setTextColor(TFT_GREENYELLOW, bgColor);
            gfx.drawString("^", p.x + w - 10, p.y + 3);
        } else if (i == 14 || i == 19 || i == 24 || i == 28) {
            gfx.setTextColor(TFT_RED, bgColor);
            gfx.drawString("v", p.x + w - 10, p.y + 3);
        } else if (i == 30) {
            gfx.setTextColor(TFT_YELLOW, bgColor);
            gfx.drawString("ATH", p.x + 7, p.y + 16);
        }

        // Token P1 (Cyan)
        if (playerPos == i) {
            gfx.fillCircle(p.x + 10, p.y + 24, 7, TFT_CYAN);
            gfx.drawCircle(p.x + 10, p.y + 24, 7, TFT_WHITE);
            gfx.setTextColor(TFT_BLACK, TFT_CYAN);
            gfx.drawString("P", p.x + 8, p.y + 20);
        }

        // Token Bot (Red)
        if (botPos == i) {
            gfx.fillCircle(p.x + 23, p.y + 24, 7, TFT_RED);
            gfx.drawCircle(p.x + 23, p.y + 24, 7, TFT_YELLOW);
            gfx.setTextColor(TFT_WHITE, TFT_RED);
            gfx.drawString("B", p.x + 21, p.y + 20);
        }
    }

    template<typename T>
    void drawFullBoard(T& gfx) {
        for (int i = 1; i <= TOTAL_TILES; i++) {
            drawSingleTile(gfx, i);
        }
        needBoardRedraw = false;
    }

    // Render Panel Kanan (Dadu, Butang, Skor) ke dalam Sprite
    template<typename S>
    void renderControlPanel(S& sprite) {
        sprite.fillScreen(sprite.color565(15, 23, 42)); // Midnight Dark
        sprite.drawRect(0, 0, sprite.width(), sprite.height(), sprite.color565(56, 189, 248)); // Cyan border

        // Skor Kedudukan
        sprite.setTextSize(1);
        sprite.setTextColor(TFT_CYAN, sprite.color565(15, 23, 42));
        sprite.drawString("P1: " + String(playerPos) + "/30", 6, 8);

        sprite.setTextColor(TFT_RED, sprite.color565(15, 23, 42));
        sprite.drawString("BOT: " + String(botPos) + "/30", 6, 22);

        // Garisan pemisah
        sprite.drawFastHLine(4, 36, sprite.width() - 8, sprite.color565(51, 65, 85));

        // Indikator giliran
        if (isPlayerTurn) {
            sprite.setTextColor(TFT_GREENYELLOW, sprite.color565(15, 23, 42));
            sprite.drawString(">> GILIRAN ANDA", 6, 42);
        } else {
            sprite.setTextColor(TFT_ORANGE, sprite.color565(15, 23, 42));
            sprite.drawString("Giliran BOT...", 6, 42);
        }

        // Dadu 3D
        int diceX = (sprite.width() - 44) / 2;
        int diceY = 60;
        int diceSize = 44;
        sprite.fillRoundRect(diceX, diceY, diceSize, diceSize, 6, TFT_WHITE);
        sprite.drawRoundRect(diceX, diceY, diceSize, diceSize, 6, TFT_BLACK);
        drawDicePips(sprite, diceX, diceY, diceSize, lastDice);

        // Butang Sentuh Baling Dadu
        uint16_t btnColor = isPlayerTurn ? sprite.color565(22, 163, 74) : sprite.color565(75, 85, 99);
        sprite.fillRoundRect(btnX, btnY, btnW, btnH, 8, btnColor);
        sprite.drawRoundRect(btnX, btnY, btnW, btnH, 8, TFT_WHITE);

        sprite.setTextColor(TFT_WHITE, btnColor);
        if (gameWon) {
            sprite.drawCenterString("MAIN LAGI", btnX + btnW / 2, btnY + 12);
            sprite.drawCenterString("[ TAP ]", btnX + btnW / 2, btnY + 26);
        } else if (isRolling) {
            sprite.drawCenterString("MEMUTAR...", btnX + btnW / 2, btnY + 18);
        } else if (isPlayerTurn) {
            sprite.drawCenterString("TAP DADU 🎲", btnX + btnW / 2, btnY + 12);
            sprite.drawCenterString("SENTUH SINI", btnX + btnW / 2, btnY + 26);
        } else {
            sprite.drawCenterString("TUNGGU BOT...", btnX + btnW / 2, btnY + 18);
        }

        // Mesej Status
        sprite.fillRect(4, 178, sprite.width() - 8, 20, sprite.color565(24, 34, 58));
        sprite.setTextColor(TFT_YELLOW, sprite.color565(24, 34, 58));
        sprite.drawCenterString(statusMsg, sprite.width() / 2, 183);

        needPanelRedraw = false;
    }

    template<typename T>
    void drawDicePips(T& gfx, int x, int y, int size, int val) {
        int r = 3;
        int c = x + size / 2;
        int m = y + size / 2;
        int l = x + 11;
        int rt = x + size - 11;
        int t = y + 11;
        int b = y + size - 11;

        if (val == 1) {
            gfx.fillCircle(c, m, r + 1, TFT_RED);
        } else if (val == 2) {
            gfx.fillCircle(l, t, r, TFT_BLACK);
            gfx.fillCircle(rt, b, r, TFT_BLACK);
        } else if (val == 3) {
            gfx.fillCircle(l, t, r, TFT_BLACK);
            gfx.fillCircle(c, m, r, TFT_BLACK);
            gfx.fillCircle(rt, b, r, TFT_BLACK);
        } else if (val == 4) {
            gfx.fillCircle(l, t, r, TFT_BLACK);
            gfx.fillCircle(rt, t, r, TFT_BLACK);
            gfx.fillCircle(l, b, r, TFT_BLACK);
            gfx.fillCircle(rt, b, r, TFT_BLACK);
        } else if (val == 5) {
            gfx.fillCircle(l, t, r, TFT_BLACK);
            gfx.fillCircle(rt, t, r, TFT_BLACK);
            gfx.fillCircle(c, m, r, TFT_BLACK);
            gfx.fillCircle(l, b, r, TFT_BLACK);
            gfx.fillCircle(rt, b, r, TFT_BLACK);
        } else if (val == 6) {
            gfx.fillCircle(l, t, r, TFT_BLACK);
            gfx.fillCircle(rt, t, r, TFT_BLACK);
            gfx.fillCircle(l, m, r, TFT_BLACK);
            gfx.fillCircle(rt, m, r, TFT_BLACK);
            gfx.fillCircle(l, b, r, TFT_BLACK);
            gfx.fillCircle(rt, b, r, TFT_BLACK);
        }
    }
};

#endif // SNAKES_AND_LADDERS_H
