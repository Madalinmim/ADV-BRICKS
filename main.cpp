#include <M5Cardputer.h>
#include <Preferences.h>

M5Canvas canvas(&M5Cardputer.Display);
Preferences preferences;

// Game States
enum GameState {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_WIN
};
GameState gameState = STATE_MENU;

bool soundEnabled = true;

// Paddle Setup
float paddleX = 100.0f;
const float paddleY = 122.0f;
const float paddleWidth = 44.0f;
const float paddleHeight = 5.0f;

// IMU Tilt smoothing variables (Increased speed multiplier & corrected direction)
float filtAccX = 0.0f;
const float filterAlpha = 0.25f;
const float paddleSensitivity = 20.0f; // Made faster!

// Ball Setup
float ballX = 120.0f;
float ballY = 115.0f;
float ballVelX = 2.0f;
float ballVelY = -2.5f;
const float ballRadius = 2.5f;
bool ballAttached = true;

// Bricks Grid Setup (5 rows x 10 cols max)
const int BRICK_ROWS = 5;
const int BRICK_COLS = 10;
const float BRICK_WIDTH = 21.0f;
const float BRICK_HEIGHT = 8.0f;
const float BRICK_GAP_X = 2.0f;
const float BRICK_GAP_Y = 2.0f;
const float BRICK_OFFSET_X = 6.0f;
const float BRICK_OFFSET_Y = 20.0f;

struct Brick {
    bool active;
    int hp;
    uint16_t color;
};
Brick bricks[BRICK_ROWS][BRICK_COLS];

// Game Progression & Stats
int score = 0;
int level = 1;
int highScore = 0;
int lives = 3;

// Outer Wall Bounds
struct Wall {
    int16_t x, y, w, h;
};
const Wall outerWalls[] = {
    {0, 12, 240, 2},   // Top
    {0, 12, 2, 123},   // Left
    {238, 12, 2, 123}  // Right
};

const uint16_t rowColors[5] = {RED, ORANGE, YELLOW, GREEN, CYAN};

// ---- SOUND EFFECTS (Quieter & Balanced Volumes) ----
void playSound(String effect) {
    if (!soundEnabled) return;

    if (effect == "paddle_hit") {
        M5Cardputer.Speaker.setVolume(100);
        M5Cardputer.Speaker.tone(600, 30);
    } else if (effect == "brick_hit") {
        M5Cardputer.Speaker.setVolume(120);
        M5Cardputer.Speaker.tone(1200, 40);
    } else if (effect == "wall_hit") {
        M5Cardputer.Speaker.setVolume(80);
        M5Cardputer.Speaker.tone(400, 25);
    } else if (effect == "die") {
        M5Cardputer.Speaker.setVolume(160); // Lowered from max to be less deafening
        M5Cardputer.Speaker.tone(350, 80); delay(90);
        M5Cardputer.Speaker.tone(200, 120);
    } else if (effect == "level_win") {
        M5Cardputer.Speaker.setVolume(130);
        M5Cardputer.Speaker.tone(523, 80); delay(90);
        M5Cardputer.Speaker.tone(659, 80); delay(90);
        M5Cardputer.Speaker.tone(784, 80); delay(90);
        M5Cardputer.Speaker.tone(1046, 180);
    } else if (effect == "game_over") {
        M5Cardputer.Speaker.setVolume(160);
        M5Cardputer.Speaker.tone(220, 120); delay(130);
        M5Cardputer.Speaker.tone(150, 250);
    }
}

// ---- 10 UNIQUE LEVEL LAYOUTS ----
void loadLevelLayout(int lvl) {
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            bricks[r][c].active = false;
            bricks[r][c].hp = 1;
            bricks[r][c].color = rowColors[r % 5];
        }
    }

    switch (lvl) {
        case 1: // Standard full wall
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < BRICK_COLS; c++)
                    bricks[r][c].active = true;
            break;

        case 2: // Checkerboard pattern
            for (int r = 0; r < 5; r++)
                for (int c = 0; c < BRICK_COLS; c++)
                    if ((r + c) % 2 == 0) bricks[r][c].active = true;
            break;

        case 3: // Columns with gaps
            for (int r = 0; r < 5; r++)
                for (int c = 0; c < BRICK_COLS; c++)
                    if (c % 2 != 0) bricks[r][c].active = true;
            break;

        case 4: // Pyramid center
            for (int r = 0; r < 5; r++)
                for (int c = r; c < BRICK_COLS - r; c++)
                    bricks[r][c].active = true;
            break;

        case 5: // Armored Top Row (2 HP) + Full grid
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < BRICK_COLS; c++) {
                    bricks[r][c].active = true;
                    if (r == 0) bricks[r][c].hp = 2;
                }
            }
            break;

        case 6: // Diamond shape
            bricks[0][4].active = true; bricks[0][5].active = true;
            for (int c = 3; c <= 6; c++) bricks[1][c].active = true;
            for (int c = 2; c <= 7; c++) bricks[2][c].active = true;
            for (int c = 3; c <= 6; c++) bricks[3][c].active = true;
            bricks[4][4].active = true; bricks[4][5].active = true;
            break;

        case 7: // Two vertical fortresses
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 3; c++) bricks[r][c].active = true;
                for (int c = 7; c < 10; c++) bricks[r][c].active = true;
            }
            break;

        case 8: // Alternating armored stripes
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < BRICK_COLS; c++) {
                    bricks[r][c].active = true;
                    if (r % 2 == 0 && (c % 2 == 0)) bricks[r][c].hp = 2;
                }
            }
            break;

        case 9: // The Maze (Sparse tunnels)
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < BRICK_COLS; c++) {
                    if (c != 2 && c != 6 && r != 2) bricks[r][c].active = true;
                    if (r == 0 || r == 4) bricks[r][c].hp = 2;
                }
            }
            break;

        case 10: // Ultimate Boss Grid (Full Armor)
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < BRICK_COLS; c++) {
                    bricks[r][c].active = true;
                    bricks[r][c].hp = (r <= 1) ? 2 : 1;
                }
            }
            break;
    }
}

void resetBall() {
    ballAttached = true;
    ballX = paddleX + paddleWidth / 2.0f;
    ballY = paddleY - ballRadius - 1.0f;
    float baseSpeed = 2.2f + (level * 0.15f);
    ballVelX = (random(0, 2) == 0 ? 1 : -1) * baseSpeed * 0.7f;
    ballVelY = -baseSpeed;
}

void loadLevel(int lvl) {
    paddleX = (240.0f - paddleWidth) / 2.0f;
    loadLevelLayout(lvl);
    resetBall();
}

void resetGame() {
    score = 0;
    level = 1;
    lives = 3;
    loadLevel(level);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);
    M5Cardputer.Display.setRotation(0);

    canvas.createSprite(240, 135);
    M5.Imu.begin();

    preferences.begin("arkanoid-game", false);
    highScore = preferences.getInt("highScore", 0);
}

void loop() {
    M5Cardputer.update();

    // ---- Keyboard Controls (Menu, Pause, Launch) ----
    if (M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

        for (auto c : status.word) {
            if (c == 's' || c == 'S' || c == ' ') {
                if (gameState == STATE_MENU || gameState == STATE_GAMEOVER || gameState == STATE_WIN) {
                    resetGame();
                    gameState = STATE_PLAYING;
                } else if (gameState == STATE_PLAYING && ballAttached) {
                    ballAttached = false;
                }
            }
            if (c == 'm' || c == 'M') {
                soundEnabled = !soundEnabled;
                delay(150);
            }
            if (c == 'p' || c == 'P') {
                if (gameState == STATE_PLAYING) gameState = STATE_PAUSED;
                else if (gameState == STATE_PAUSED) gameState = STATE_PLAYING;
                delay(150);
            }
            if (c == 'q' || c == 'Q') {
                if (gameState == STATE_PAUSED || gameState == STATE_GAMEOVER || gameState == STATE_WIN) {
                    gameState = STATE_MENU;
                    delay(150);
                }
            }
        }
        if (status.enter) {
            if (gameState == STATE_MENU || gameState == STATE_GAMEOVER || gameState == STATE_WIN) {
                resetGame();
                gameState = STATE_PLAYING;
                delay(150);
            } else if (gameState == STATE_PLAYING && ballAttached) {
                ballAttached = false;
            }
        }
    }

    // ---- IMU Tilt Controls for Paddle (Corrected & Faster) ----
    float rawX = 0, rawY = 0, rawZ = 0;
    M5.Imu.getAccel(&rawX, &rawY, &rawZ);
    filtAccX = filtAccX * (1.0f - filterAlpha) + rawX * filterAlpha;

    // Flipped sign (+) and increased sensitivity factor
    paddleX -= filtAccX * paddleSensitivity;

    // Clamp paddle within side walls
    if (paddleX < 4.0f) paddleX = 4.0f;
    if (paddleX > 236.0f - paddleWidth) paddleX = 236.0f - paddleWidth;

    // ---- STATE: START MENU ----
    if (gameState == STATE_MENU) {
        canvas.fillScreen(BLACK);
        canvas.setCursor(60, 10);
        canvas.setTextColor(RED);
        canvas.setTextSize(2);
        canvas.print("ADV BRICKS");

        canvas.setCursor(65, 38);
        canvas.setTextColor(YELLOW);
        canvas.setTextSize(1);
        canvas.printf("High Score: %d", highScore);

        canvas.setCursor(15, 60);
        canvas.setTextColor(WHITE);
        canvas.print("[SPACE] / ENTER : Launch Ball");

        canvas.setCursor(15, 80);
        canvas.setTextColor(LIGHTGREY);
        canvas.print("Tilt Device     : Move Paddle");

        canvas.setCursor(15, 100);
        canvas.setTextColor(GREEN);
        canvas.printf("[M] Sound [%s]  [P] Pause", soundEnabled ? "ON" : "OFF");

        canvas.pushSprite(0, 0);
        delay(30);
        return;
    }

    // ---- STATE: PAUSE ----
    if (gameState == STATE_PAUSED) {
        canvas.fillScreen(BLACK);
        canvas.setCursor(85, 35);
        canvas.setTextColor(YELLOW);
        canvas.setTextSize(2);
        canvas.print("PAUSED");

        canvas.setCursor(45, 75);
        canvas.setTextColor(WHITE);
        canvas.setTextSize(1);
        canvas.print("[P] Continue  [Q] Quit");

        canvas.pushSprite(0, 0);
        delay(30);
        return;
    }

    // ---- STATE: GAME OVER ----
    if (gameState == STATE_GAMEOVER) {
        canvas.fillScreen(BLACK);
        canvas.setCursor(60, 20);
        canvas.setTextColor(RED);
        canvas.setTextSize(2);
        canvas.print("GAME OVER");

        canvas.setCursor(70, 55);
        canvas.setTextColor(YELLOW);
        canvas.setTextSize(1);
        canvas.printf("Final Score: %d", score);

        canvas.setCursor(40, 85);
        canvas.setTextColor(WHITE);
        canvas.print("[SPACE] Restart  [Q] Menu");

        canvas.pushSprite(0, 0);
        delay(30);
        return;
    }

    // ---- STATE: WIN SCREEN ----
    if (gameState == STATE_WIN) {
        canvas.fillScreen(BLACK);
        canvas.setCursor(40, 20);
        canvas.setTextColor(GREEN);
        canvas.setTextSize(2);
        canvas.print("VICTORY!");

        canvas.setCursor(55, 55);
        canvas.setTextColor(YELLOW);
        canvas.setTextSize(1);
        canvas.printf("Final Score: %d", score);

        canvas.setCursor(35, 85);
        canvas.setTextColor(WHITE);
        canvas.print("[SPACE] Play Again [Q] Menu");

        canvas.pushSprite(0, 0);
        delay(30);
        return;
    }

    // ---- STATE: PLAYING ----
    if (ballAttached) {
        ballX = paddleX + paddleWidth / 2.0f;
        ballY = paddleY - ballRadius - 1.0f;
    } else {
        ballX += ballVelX;
        ballY += ballVelY;

        // Bounce off left/right walls
        if (ballX - ballRadius <= 4.0f) {
            ballX = 4.0f + ballRadius;
            ballVelX = -ballVelX;
            playSound("wall_hit");
        } else if (ballX + ballRadius >= 236.0f) {
            ballX = 236.0f - ballRadius;
            ballVelX = -ballVelX;
            playSound("wall_hit");
        }

        // Bounce off top wall
        if (ballY - ballRadius <= 14.0f) {
            ballY = 14.0f + ballRadius;
            ballVelY = -ballVelY;
            playSound("wall_hit");
        }

        // Bounce off paddle with dynamic angling
        if (ballY + ballRadius >= paddleY && ballY - ballRadius <= paddleY + paddleHeight &&
            ballX >= paddleX && ballX <= paddleX + paddleWidth && ballVelY > 0) {
            
            ballY = paddleY - ballRadius;
            ballVelY = -ballVelY;
            
            float hitOffset = (ballX - (paddleX + paddleWidth / 2.0f)) / (paddleWidth / 2.0f);
            ballVelX = hitOffset * 3.8f; 
            playSound("paddle_hit");
        }

        // Check Brick Collisions
        bool allCleared = true;
        for (int r = 0; r < BRICK_ROWS; r++) {
            for (int c = 0; c < BRICK_COLS; c++) {
                if (!bricks[r][c].active) continue;

                allCleared = false;
                float bx = BRICK_OFFSET_X + c * (BRICK_WIDTH + BRICK_GAP_X);
                float by = BRICK_OFFSET_Y + r * (BRICK_HEIGHT + BRICK_GAP_Y);

                if (ballX + ballRadius > bx && ballX - ballRadius < bx + BRICK_WIDTH &&
                    ballY + ballRadius > by && ballY - ballRadius < by + BRICK_HEIGHT) {
                    
                    bricks[r][c].hp--;
                    if (bricks[r][c].hp <= 0) {
                        bricks[r][c].active = false;
                        score += (BRICK_ROWS - r) * 10;
                    }
                    
                    ballVelY = -ballVelY;
                    playSound("brick_hit");

                    if (score > highScore) {
                        highScore = score;
                        preferences.putInt("highScore", highScore);
                    }
                    break;
                }
            }
        }

        // Check Level Clear (10 levels total)
        if (allCleared) {
            playSound("level_win");
            level++;
            if (level > 10) {
                gameState = STATE_WIN;
            } else {
                loadLevel(level);
            }
        }

        // Ball falls past bottom boundary -> Lose Life
        if (ballY - ballRadius > 135.0f) {
            playSound("die");
            lives--;
            if (lives <= 0) {
                playSound("game_over");
                gameState = STATE_GAMEOVER;
            } else {
                resetBall();
            }
        }
    }

    // ---- Render Playing Screen ----
    canvas.fillScreen(BLACK);

    // Top HUD
    canvas.setCursor(4, 2);
    canvas.setTextColor(CYAN);
    canvas.setTextSize(1);
    canvas.printf("L:%d SCR:%d HI:%d LIFE:%d", level, score, highScore, lives);

    // Draw Outer Boundary Walls
    for (int i = 0; i < 3; i++) {
        const Wall& w = outerWalls[i];
        canvas.fillRect(w.x, w.y, w.w, w.h, YELLOW);
    }

    // Draw Bricks
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c].active) continue;
            float bx = BRICK_OFFSET_X + c * (BRICK_WIDTH + BRICK_GAP_X);
            float by = BRICK_OFFSET_Y + r * (BRICK_HEIGHT + BRICK_GAP_Y);
            
            uint16_t col = (bricks[r][c].hp > 1) ? WHITE : bricks[r][c].color;
            canvas.fillRect((int)bx, (int)by, (int)BRICK_WIDTH, (int)BRICK_HEIGHT, col);
            canvas.drawRect((int)bx, (int)by, (int)BRICK_WIDTH, (int)BRICK_HEIGHT, BLACK);
        }
    }

    // Draw Paddle & Ball
    canvas.fillRect((int)paddleX, (int)paddleY, (int)paddleWidth, (int)paddleHeight, BLUE);
    canvas.fillCircle((int)ballX, (int)ballY, (int)ballRadius, WHITE);

    canvas.pushSprite(0, 0);
    delay(16);
}