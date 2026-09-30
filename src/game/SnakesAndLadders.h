#ifndef SNAKES_AND_LADDERS_H
#define SNAKES_AND_LADDERS_H

#include <Arduino.h>
#include <LovyanGFX.hpp>

// Jumlah petak papan (Ditambah 5 dari 30 -> 35 petak)
#define TOTAL_TILES 35
#define BOARD_COLS  7
#define BOARD_ROWS  5

struct TilePos {
    int x, y;
};

class SnakesAndLaddersGame {
public:
    int screenW;
    int screenH;
    int numPlayers;        // 1 hingga 4 pemain (1P, 2P, 3P, 4P)
    int playerPos[4];      // Kedudukan P1, P2, P3, P4 (1 hingga 35)
    int oldPlayerPos[4];
    int currentTurn;       // 0=P1, 1=P2 (atau Bot jika 1P), 2=P3, 3=P4
    int lastDice;
    bool isRolling;
    unsigned long rollStartTime;
    char statusMsg[48];
    bool gameWon;
    int winner;            // 1=P1, 2=P2/Bot, 3=P3, 4=P4

    bool needBoardRedraw;  // Flag redraw penuh papan
    bool needPanelRedraw;  // Flag redraw panel kawalan kanan

    SnakesAndLaddersGame(int w = 320, int h = 240) {
        screenW = w;
        screenH = h;
        numPlayers = 1; // Default 1 Player vs Bot
        reset();
    }

    void setNumPlayers(int count) {
        if (count < 1) count = 1;
        if (count > 4) count = 4;
        numPlayers = count;
        reset();
        snprintf(statusMsg, sizeof(statusMsg), "Mod %dP Aktif!", numPlayers);
    }

    void reset() {
        for (int i = 0; i < 4; i++) {
            playerPos[i] = 1;
            oldPlayerPos[i] = 1;
        }
        currentTurn = 0;
        lastDice = 1;
        isRolling = false;
        gameWon = false;
        winner = 0;
        needBoardRedraw = true;
        needPanelRedraw = true;
        if (numPlayers == 1) {
            strncpy(statusMsg, "Sentuh Dadu P1!", sizeof(statusMsg) - 1);
        } else {
            strncpy(statusMsg, "Giliran P1 Mula!", sizeof(statusMsg) - 1);
        }
    }

    bool canRoll() const {
        if (gameWon || isRolling) return false;
        if (numPlayers == 1 && currentTurn == 1) return false;
        return true;
    }

    TilePos getTileCoordinates(int tileNum) {
        if (tileNum < 1) tileNum = 1;
        if (tileNum > TOTAL_TILES) tileNum = TOTAL_TILES;

        int index = tileNum - 1;
        int rowFromBottom = index / BOARD_COLS;
        int col = index % BOARD_COLS;

        // Boustrophedon (zigzag)
        if (rowFromBottom % 2 == 1) {
            col = (BOARD_COLS - 1) - col;
        }

        int row = (BOARD_ROWS - 1) - rowFromBottom;

        int tileW = 33;
        int tileH = 41;
        int boardStartX = 5;
        int boardStartY = 28;

        TilePos pos;
        pos.x = boardStartX + col * tileW;
        pos.y = boardStartY + row * tileH;
        return pos;
    }

    int checkSnakesAndLadders(int pos, const char* pName) {
        // TANGGA (+9 hingga +10)
        if (pos == 3) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Tangga (+9)!", pName);
            return 12;
        }
        if (pos == 10) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Halving (+8)!", pName);
            return 18;
        }
        if (pos == 17) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Lightning (+9)!", pName);
            return 26;
        }
        if (pos == 24) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Bull Run (+9)!", pName);
            return 33;
        }

        // ULAR (-9 hingga -12)
        if (pos == 13) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Ular FUD (-9)!", pName);
            return 4;
        }
        if (pos == 20) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Bear Dip (-11)!", pName);
            return 9;
        }
        if (pos == 27) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Winter (-11)!", pName);
            return 16;
        }
        if (pos == 34) {
            snprintf(statusMsg, sizeof(statusMsg), "%s: Whale Dump (-12)!", pName);
            return 22;
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
        // Panel sisi kanan bermula di panelX = 240, panelY = 28 (w = 78, h = 204)
        if (touchX >= 240 && touchX <= 318) {
            // 1. Sentuh 4 Petak Pilihan Mod Pemain (y: 166..230)
            if (touchY >= 166 && touchY <= 197) {
                // Baris 1: 1P dan 2P
                if (touchX < 278) {
                    setNumPlayers(1);
                } else {
                    setNumPlayers(2);
                }
                return true;
            }
            if (touchY >= 198 && touchY <= 232) {
                // Baris 2: 3P dan 4P
                if (touchX < 278) {
                    setNumPlayers(3);
                } else {
                    setNumPlayers(4);
                }
                return true;
            }

            // 2. Sentuh Dadu Sahaja Untuk Putar (y: 72..128)
            if (touchY >= 72 && touchY <= 128) {
                if (gameWon) {
                    reset();
                    return true;
                }
                // Jika 1P dan sedang giliran Bot, abaikan sentuhan
                if (numPlayers == 1 && currentTurn != 0) {
                    return false;
                }
                if (!isRolling) {
                    rollDice();
                    return true;
                }
            }
        }

        // Sebarang tap di luar dadu dan butang mod tidak melakukan apa-apa
        return false;
    }

    template<typename T>
    void update(T& gfx) {
        if (isRolling) {
            lastDice = random(1, 7);
            needPanelRedraw = true;

            if (millis() - rollStartTime > 550) {
                isRolling = false;

                int activePlayerIdx = currentTurn;
                const char* pName;
                if (numPlayers == 1) {
                    pName = (activePlayerIdx == 0) ? "P1" : "Bot";
                } else {
                    static char pBuf[8];
                    snprintf(pBuf, sizeof(pBuf), "P%d", activePlayerIdx + 1);
                    pName = pBuf;
                }

                oldPlayerPos[activePlayerIdx] = playerPos[activePlayerIdx];
                playerPos[activePlayerIdx] += lastDice;

                if (playerPos[activePlayerIdx] >= TOTAL_TILES) {
                    playerPos[activePlayerIdx] = TOTAL_TILES;
                    gameWon = true;
                    winner = activePlayerIdx + 1;
                    if (numPlayers == 1 && activePlayerIdx == 1) {
                        strncpy(statusMsg, "Satoshi Bot Menang!", sizeof(statusMsg) - 1);
                    } else {
                        snprintf(statusMsg, sizeof(statusMsg), "🎉 %s MENANG ATH!", pName);
                    }
                } else {
                    int newPos = checkSnakesAndLadders(playerPos[activePlayerIdx], pName);
                    if (newPos == playerPos[activePlayerIdx]) {
                        snprintf(statusMsg, sizeof(statusMsg), "%s: [%d] -> Petak %d", pName, lastDice, playerPos[activePlayerIdx]);
                    }
                    playerPos[activePlayerIdx] = newPos;

                    // Tukar giliran pemain seterusnya
                    if (numPlayers == 1) {
                        currentTurn = (currentTurn == 0) ? 1 : 0;
                    } else {
                        currentTurn = (currentTurn + 1) % numPlayers;
                    }
                }

                drawSingleTile(gfx, oldPlayerPos[activePlayerIdx]);
                drawSingleTile(gfx, playerPos[activePlayerIdx]);
                needPanelRedraw = true;
            }
        } else if (numPlayers == 1 && currentTurn == 1 && !gameWon) {
            // Giliran Automatik Satoshi Bot dalam mod 1P
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
        int w = 32;
        int h = 40;

        uint16_t bgColor = ((i / BOARD_COLS + i % BOARD_COLS) % 2 == 0) 
                           ? gfx.color565(30, 41, 59)   // Dark Slate
                           : gfx.color565(51, 65, 85);  // Slate Blue

        if (i == 3 || i == 10 || i == 17 || i == 24) {
            bgColor = gfx.color565(16, 120, 60); // Hijau Tangga
        } else if (i == 13 || i == 20 || i == 27 || i == 34) {
            bgColor = gfx.color565(140, 30, 30); // Merah Ular
        } else if (i == 35) {
            bgColor = gfx.color565(180, 140, 20); // Emas ATH
        }

        gfx.fillRect(p.x, p.y, w, h, bgColor);
        gfx.drawRect(p.x, p.y, w, h, gfx.color565(71, 85, 105));

        gfx.setTextSize(1);
        gfx.setTextColor(TFT_WHITE, bgColor);
        gfx.drawString(String(i), p.x + 3, p.y + 2);

        if (i == 3 || i == 10 || i == 17 || i == 24) {
            // Papar nombor petak destinasi naik di bawah nombor petak
            const char* destNum = (i == 3)  ? "12" :
                                  (i == 10) ? "18" :
                                  (i == 17) ? "26" : "33";
            gfx.setTextColor(TFT_YELLOW, bgColor);
            gfx.drawString(destNum, p.x + 3, p.y + 14);
        } else if (i == 13 || i == 20 || i == 27 || i == 34) {
            // Papar nombor petak destinasi turun di bawah nombor petak
            const char* destNum = (i == 13) ? "4" :
                                  (i == 20) ? "9" :
                                  (i == 27) ? "16" : "22";
            gfx.setTextColor(TFT_YELLOW, bgColor);
            gfx.drawString(destNum, p.x + 3, p.y + 14);
        } else if (i == 35) {
            gfx.setTextColor(TFT_YELLOW, bgColor);
            gfx.drawString("ATH", p.x + 6, p.y + 16);
        }

        // Lukis Token Pemain (Maksimum 4 pemain)
        // P1: Cyan (Kiri atas)
        if (playerPos[0] == i) {
            gfx.fillCircle(p.x + 8, p.y + 17, 5, TFT_CYAN);
            gfx.drawCircle(p.x + 8, p.y + 17, 5, TFT_WHITE);
            gfx.setTextColor(TFT_BLACK, TFT_CYAN);
            gfx.drawString("1", p.x + 6, p.y + 14);
        }

        // P2 atau Bot: Kanan atas
        int effectivePlayers = (numPlayers == 1) ? 2 : numPlayers;
        if (effectivePlayers >= 2 && playerPos[1] == i) {
            uint16_t c = (numPlayers == 1) ? TFT_RED : TFT_YELLOW;
            gfx.fillCircle(p.x + 23, p.y + 17, 5, c);
            gfx.drawCircle(p.x + 23, p.y + 17, 5, TFT_WHITE);
            gfx.setTextColor(TFT_BLACK, c);
            gfx.drawString((numPlayers == 1) ? "B" : "2", p.x + 21, p.y + 14);
        }

        // P3: Kiri bawah (Hanya mod 3P dan 4P)
        if (numPlayers >= 3 && playerPos[2] == i) {
            gfx.fillCircle(p.x + 8, p.y + 30, 5, TFT_GREEN);
            gfx.drawCircle(p.x + 8, p.y + 30, 5, TFT_WHITE);
            gfx.setTextColor(TFT_BLACK, TFT_GREEN);
            gfx.drawString("3", p.x + 6, p.y + 27);
        }

        // P4: Kanan bawah (Hanya mod 4P)
        if (numPlayers >= 4 && playerPos[3] == i) {
            gfx.fillCircle(p.x + 23, p.y + 30, 5, TFT_MAGENTA);
            gfx.drawCircle(p.x + 23, p.y + 30, 5, TFT_WHITE);
            gfx.setTextColor(TFT_WHITE, TFT_MAGENTA);
            gfx.drawString("4", p.x + 21, p.y + 27);
        }
    }

    template<typename T>
    void drawFullBoard(T& gfx) {
        for (int i = 1; i <= TOTAL_TILES; i++) {
            drawSingleTile(gfx, i);
        }
        needBoardRedraw = false;
    }

    // Render Panel Kanan (Dadu, Butang Mod 1-4P, Skor) ke dalam Sprite (Width 78, Height 204)
    template<typename S>
    void renderControlPanel(S& sprite) {
        sprite.fillScreen(sprite.color565(15, 23, 42)); // Midnight Dark
        sprite.drawRect(0, 0, sprite.width(), sprite.height(), sprite.color565(56, 189, 248)); // Cyan border

        sprite.setTextSize(1);

        // Skor Kedudukan mengikut mod
        if (numPlayers == 1) {
            sprite.setTextColor(TFT_CYAN, sprite.color565(15, 23, 42));
            sprite.drawString("P1 :" + String(playerPos[0]) + "/35", 5, 5);
            sprite.setTextColor(TFT_RED, sprite.color565(15, 23, 42));
            sprite.drawString("BOT:" + String(playerPos[1]) + "/35", 5, 17);
        } else if (numPlayers == 2) {
            sprite.setTextColor(TFT_CYAN, sprite.color565(15, 23, 42));
            sprite.drawString("P1:" + String(playerPos[0]), 5, 5);
            sprite.setTextColor(TFT_YELLOW, sprite.color565(15, 23, 42));
            sprite.drawString("P2:" + String(playerPos[1]), 42, 5);
            sprite.setTextColor(TFT_SILVER, sprite.color565(15, 23, 42));
            sprite.drawString("Sasaran: 35", 5, 17);
        } else if (numPlayers == 3) {
            sprite.setTextColor(TFT_CYAN, sprite.color565(15, 23, 42));
            sprite.drawString("P1:" + String(playerPos[0]), 4, 4);
            sprite.setTextColor(TFT_YELLOW, sprite.color565(15, 23, 42));
            sprite.drawString("P2:" + String(playerPos[1]), 40, 4);
            sprite.setTextColor(TFT_GREEN, sprite.color565(15, 23, 42));
            sprite.drawString("P3:" + String(playerPos[2]), 4, 16);
            sprite.setTextColor(TFT_SILVER, sprite.color565(15, 23, 42));
            sprite.drawString("Goal:35", 40, 16);
        } else {
            sprite.setTextColor(TFT_CYAN, sprite.color565(15, 23, 42));
            sprite.drawString("P1:" + String(playerPos[0]), 4, 4);
            sprite.setTextColor(TFT_YELLOW, sprite.color565(15, 23, 42));
            sprite.drawString("P2:" + String(playerPos[1]), 40, 4);
            sprite.setTextColor(TFT_GREEN, sprite.color565(15, 23, 42));
            sprite.drawString("P3:" + String(playerPos[2]), 4, 16);
            sprite.setTextColor(TFT_MAGENTA, sprite.color565(15, 23, 42));
            sprite.drawString("P4:" + String(playerPos[3]), 40, 16);
        }

        sprite.drawFastHLine(4, 28, sprite.width() - 8, sprite.color565(51, 65, 85));

        // Indikator Giliran
        if (numPlayers == 1) {
            if (currentTurn == 0) {
                sprite.setTextColor(TFT_GREENYELLOW, sprite.color565(15, 23, 42));
                sprite.drawCenterString(">> GILIRAN P1 <<", sprite.width() / 2, 33);
            } else {
                sprite.setTextColor(TFT_ORANGE, sprite.color565(15, 23, 42));
                sprite.drawCenterString("Giliran Bot...", sprite.width() / 2, 33);
            }
        } else {
            uint16_t turnColor = (currentTurn == 0) ? TFT_CYAN : 
                                 (currentTurn == 1) ? TFT_YELLOW : 
                                 (currentTurn == 2) ? TFT_GREEN : TFT_MAGENTA;
            char tBuf[24];
            snprintf(tBuf, sizeof(tBuf), ">> GILIRAN P%d <<", currentTurn + 1);
            sprite.setTextColor(turnColor, sprite.color565(15, 23, 42));
            sprite.drawCenterString(tBuf, sprite.width() / 2, 33);
        }

        // Dadu 3D Interaktif (Hanya sentuh dadu untuk putar)
        int diceX = 16;
        int diceY = 46;
        int diceSize = 46;
        sprite.fillRoundRect(diceX, diceY, diceSize, diceSize, 6, TFT_WHITE);
        sprite.drawRoundRect(diceX, diceY, diceSize, diceSize, 6, TFT_BLACK);
        drawDicePips(sprite, diceX, diceY, diceSize, lastDice);

        // Label butang dadu
        sprite.setTextColor(TFT_YELLOW, sprite.color565(15, 23, 42));
        if (isRolling) {
            sprite.drawCenterString("MEMUTAR...", sprite.width() / 2, 95);
        } else {
            sprite.drawCenterString("TAP DADU 🎲", sprite.width() / 2, 95);
        }

        // Mesej Status
        sprite.fillRect(3, 107, sprite.width() - 6, 20, sprite.color565(24, 34, 58));
        sprite.setTextColor(TFT_GREENYELLOW, sprite.color565(24, 34, 58));
        sprite.drawCenterString(statusMsg, sprite.width() / 2, 112);

        // 4 Petak Pilihan Mod Pemain (1P - 4P)
        sprite.setTextColor(TFT_SILVER, sprite.color565(15, 23, 42));
        sprite.drawCenterString("- MOD PEMAIN -", sprite.width() / 2, 130);

        // Baris 1: [ 1P ] dan [ 2P ]
        drawModeButton(sprite, 4, 140, 33, 26, "1P", (numPlayers == 1));
        drawModeButton(sprite, 41, 140, 33, 26, "2P", (numPlayers == 2));

        // Baris 2: [ 3P ] dan [ 4P ]
        drawModeButton(sprite, 4, 170, 33, 26, "3P", (numPlayers == 3));
        drawModeButton(sprite, 41, 170, 33, 26, "4P", (numPlayers == 4));

        needPanelRedraw = false;
    }

    template<typename S>
    void drawModeButton(S& sprite, int x, int y, int w, int h, const char* label, bool active) {
        uint16_t bg = active ? sprite.color565(16, 185, 129) : sprite.color565(30, 41, 59);
        uint16_t border = active ? TFT_YELLOW : sprite.color565(71, 85, 105);
        uint16_t textC = active ? TFT_WHITE : sprite.color565(148, 163, 184);

        sprite.fillRoundRect(x, y, w, h, 4, bg);
        sprite.drawRoundRect(x, y, w, h, 4, border);
        sprite.setTextColor(textC, bg);
        sprite.drawCenterString(label, x + w / 2, y + 8);
    }

    template<typename T>
    void drawDicePips(T& gfx, int x, int y, int size, int val) {
        int r = 3;
        int c = x + size / 2;
        int m = y + size / 2;
        int l = x + 12;
        int rt = x + size - 12;
        int t = y + 12;
        int b = y + size - 12;

        if (val == 1) {
            gfx.fillCircle(c, m, r + 2, TFT_RED);
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
