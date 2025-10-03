#define _CRT_SECURE_NO_WARNINGS
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>     
#include <time.h>

#define WIDTH 800
#define HEIGHT 600
#define CELL_SIZE 20 
#define UPDATE_INTERVAL 0.15
#define WALL_THICKNESS 1
#define MAX_SNAKE_LENGTH 500
#define MAX_WALLS 200

typedef struct {
    int x;
    int y;
} Segment;

typedef struct {
    int x;
    int y;
    int type;
    double spawnTime;
} Bonus;

enum Direction { UP, DOWN, LEFT, RIGHT, NONE };
enum GameState { MENU, PLAYING, INFO };

Segment snake[MAX_SNAKE_LENGTH];
int snakeLength = 1;
enum Direction dir = RIGHT;
enum Direction pendingDir = NONE;

int foodX, foodY;

int gameOver = 0;
int score = 0;
int walls[MAX_WALLS][2];
int wallsCount = 0;
int foodEaten = 0;

double lastBonusTime = -30.0;
Bonus currentBonus = { -1, -1, -1, 0.0 };
int multiplierActive = 0;
double multiplierStartTime = 0.0;

enum GameState gameState = MENU;
int currentLevel = 1;


const int fieldMinX = WALL_THICKNESS;
const int fieldMaxX = (WIDTH / CELL_SIZE) - WALL_THICKNESS - 1;
const int fieldMinY = WALL_THICKNESS;
const int fieldMaxY = (HEIGHT / CELL_SIZE) - WALL_THICKNESS - 1;

int isPositionValid(int x, int y) {
    if (x < fieldMinX || x > fieldMaxX || y < fieldMinY || y > fieldMaxY)
        return 0;
    for (int i = 0; i < wallsCount; i++)
        if (x == walls[i][0] && y == walls[i][1])
            return 0;
    for (int i = 0; i < snakeLength; i++)
        if (x == snake[i].x && y == snake[i].y)
            return 0;
    return 1;
}

void initGame(int level) {
    for (int i = 0; i < MAX_SNAKE_LENGTH; i++) {
        snake[i].x = 0;
        snake[i].y = 0;
    }
    wallsCount = 0;
    snake[0].x = 10;
    snake[0].y = 10;
    snakeLength = 1;
    dir = RIGHT;
    pendingDir = NONE;
    gameOver = 0;
    score = 0;
    foodEaten = 0;
    lastBonusTime = -30.0;
    currentBonus.type = -1;
    multiplierActive = 0;

    currentLevel = level;

    if (level == 2) {
        int startX = WIDTH / CELL_SIZE / 2 - 5;
        int startY = HEIGHT / CELL_SIZE / 2 - 5;
        for (int y = startY; y < startY + 11; y++) {
            walls[wallsCount][0] = startX;
            walls[wallsCount][1] = y;
            wallsCount++;
        }
        for (int y = startY; y < startY + 11; y++) {
            walls[wallsCount][0] = startX + 10;
            walls[wallsCount][1] = y;
            wallsCount++;
        }
        for (int x = startX; x <= startX + 10; x++) {
            walls[wallsCount][0] = x;
            walls[wallsCount][1] = startY + 10;
            wallsCount++;
        }
    }
    else if (level == 3) {
        int centerX = (fieldMaxX + fieldMinX) / 2;
        int centerY = (fieldMaxY + fieldMinY) / 2;
        int outerSize = 9;
        int innerSize = 5;

        for (int x = centerX - outerSize; x <= centerX + outerSize; x++) {
            if (x < fieldMinX || x > fieldMaxX) continue;
            walls[wallsCount][0] = x;
            walls[wallsCount][1] = centerY + outerSize;
            wallsCount++;
            if (abs(x - centerX) > 2) {
                walls[wallsCount][0] = x;
                walls[wallsCount][1] = centerY - outerSize;
                wallsCount++;
            }
        }
        for (int y = centerY - outerSize + 1; y < centerY + outerSize; y++) {
            if (y < fieldMinY || y > fieldMaxY) continue;
            walls[wallsCount][0] = centerX - outerSize;
            walls[wallsCount][1] = y;
            wallsCount++;
            walls[wallsCount][0] = centerX + outerSize;
            walls[wallsCount][1] = y;
            wallsCount++;
        }
        for (int x = centerX - innerSize; x <= centerX + innerSize; x++) {
            if (x < fieldMinX || x > fieldMaxX) continue;
            walls[wallsCount][0] = x;
            walls[wallsCount][1] = centerY - innerSize;
            wallsCount++;
            if (abs(x - centerX) > 1) {
                walls[wallsCount][0] = x;
                walls[wallsCount][1] = centerY + innerSize;
                wallsCount++;
            }
        }
        for (int y = centerY - innerSize + 1; y < centerY + innerSize; y++) {
            if (y < fieldMinY || y > fieldMaxY) continue;
            walls[wallsCount][0] = centerX - innerSize;
            walls[wallsCount][1] = y;
            wallsCount++;
            walls[wallsCount][0] = centerX + innerSize;
            walls[wallsCount][1] = y;
            wallsCount++;
        }
    }

    do {
        foodX = fieldMinX + rand() % (fieldMaxX - fieldMinX + 1);
        foodY = fieldMinY + rand() % (fieldMaxY - fieldMinY + 1);
    } while (!isPositionValid(foodX, foodY));
}


int generateBonus() {
    int valid;
    int attempts = 0;
    const int max_attempts = 1000;

    do {
        valid = 1;
        currentBonus.x = fieldMinX + rand() % (fieldMaxX - fieldMinX + 1);
        currentBonus.y = fieldMinY + rand() % (fieldMaxY - fieldMinY + 1);
        currentBonus.type = rand() % 3;
        currentBonus.spawnTime = glfwGetTime();

        if (!isPositionValid(currentBonus.x, currentBonus.y)) valid = 0;
        if (currentBonus.x == foodX && currentBonus.y == foodY) valid = 0;
        for (int i = 0; i < snakeLength; i++) {
            if (currentBonus.x == snake[i].x && currentBonus.y == snake[i].y) {
                valid = 0;
                break;
            }
        }

        if (attempts++ > max_attempts) {
            currentBonus.type = -1;
            return 0;
        }
    } while (!valid);

    return 1;
}

void applyBonusEffect(int type) {
    switch (type) {
    case 0:
        multiplierActive = 1;
        multiplierStartTime = glfwGetTime();
        break;
    case 1:
    {
        int newLength = snakeLength > 3 ? snakeLength - 3 : 1;
        for (int i = newLength; i < snakeLength; i++) {
            snake[i].x = 0;
            snake[i].y = 0;
        }
        snakeLength = newLength;
    }
    break;
    case 2:
        score += 5;
        if (snakeLength + 2 <= MAX_SNAKE_LENGTH) {
            Segment last = snake[snakeLength - 1];
            snake[snakeLength] = last;
            snake[snakeLength + 1] = last;
            snakeLength += 2;
        }
        else {
            snakeLength = MAX_SNAKE_LENGTH;
        }
        break;
    }
}

void processInput(GLFWwindow* window) {
    static int iKeyPressed = 0;
    static int rKeyPressed = 0;
    static int eKeyPressed = 0;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);

    if (gameState == MENU) {
        for (int key = GLFW_KEY_1; key <= GLFW_KEY_3; key++) {
            if (glfwGetKey(window, key) == GLFW_PRESS) {
                initGame(key - GLFW_KEY_0);
                gameState = PLAYING;
            }
        }
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS && !iKeyPressed) {
            gameState = INFO;
            iKeyPressed = 1;
        }
        else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_RELEASE) {
            iKeyPressed = 0;
        }
    }
    else if (gameState == INFO) {
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS && !iKeyPressed) {
            gameState = MENU;
            iKeyPressed = 1;
        }
        else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_RELEASE) {
            iKeyPressed = 0;
        }
    }
    else if (gameState == PLAYING) {
        if (gameOver) {
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed) {
                initGame(currentLevel);
                rKeyPressed = 1;
            }
            else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) {
                rKeyPressed = 0;
            }
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !eKeyPressed) {
                gameState = MENU;
                eKeyPressed = 1;
            }
            else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE) {
                eKeyPressed = 0;
            }
            return;
        }

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && dir != DOWN)
            pendingDir = UP;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && dir != UP)
            pendingDir = DOWN;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && dir != RIGHT)
            pendingDir = LEFT;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && dir != LEFT)
            pendingDir = RIGHT;

        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS && !iKeyPressed) {
            gameState = MENU;
            iKeyPressed = 1;
        }
        else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_RELEASE) {
            iKeyPressed = 0;
        }
    }
}

void updateGame() {
    if (gameOver || gameState != PLAYING) return;

    if (pendingDir != NONE) {
        dir = pendingDir;
        pendingDir = NONE;
    }

    Segment newHead = snake[0];
    switch (dir) {
    case UP:    newHead.y--; break;
    case DOWN:  newHead.y++; break;
    case LEFT:  newHead.x--; break;
    case RIGHT: newHead.x++; break;
    }

    if (!isPositionValid(newHead.x, newHead.y)) {
        gameOver = 1;
        return;
    }
    for (int i = 0; i < wallsCount; i++) {
        if (newHead.x == walls[i][0] && newHead.y == walls[i][1]) {
            gameOver = 1;
            return;
        }
    }
    for (int i = 0; i < snakeLength; i++) {
        if (newHead.x == snake[i].x && newHead.y == snake[i].y) {
            gameOver = 1;
            return;
        }
    }

    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = newHead;

    if (newHead.x == foodX && newHead.y == foodY) {
        if (multiplierActive) score += 2;
        else score += 1;

        if (snakeLength < MAX_SNAKE_LENGTH) {
            snakeLength++;
            snake[snakeLength - 1] = snake[snakeLength - 2];
        }

        foodEaten++;

        if (foodEaten >= 3 && glfwGetTime() - lastBonusTime >= 30.0) {
            if (generateBonus()) {
                foodEaten = 0;
                lastBonusTime = glfwGetTime();
            }
            else {
                foodEaten = 3;
            }
        }

        int valid;
        do {
            valid = 1;
            foodX = fieldMinX + rand() % (fieldMaxX - fieldMinX + 1);
            foodY = fieldMinY + rand() % (fieldMaxY - fieldMinY + 1);

            if (!isPositionValid(foodX, foodY)) {
                valid = 0;
                continue;
            }
        } while (!valid);
    }

    if (currentBonus.type != -1 && snake[0].x == currentBonus.x && snake[0].y == currentBonus.y) {
        applyBonusEffect(currentBonus.type);
        currentBonus.type = -1;
    }

    if (currentBonus.type != -1 && glfwGetTime() - currentBonus.spawnTime >= 10.0) {
        currentBonus.type = -1;
    }

    if (multiplierActive && glfwGetTime() - multiplierStartTime >= 20.0) {
        multiplierActive = 0;
    }
}

void drawSquare(int x, int y) {
    glBegin(GL_QUADS);
    glVertex2f(x * CELL_SIZE, y * CELL_SIZE);
    glVertex2f((x + 1) * CELL_SIZE, y * CELL_SIZE);
    glVertex2f((x + 1) * CELL_SIZE, (y + 1) * CELL_SIZE);
    glVertex2f(x * CELL_SIZE, (y + 1) * CELL_SIZE);
    glEnd();
}

void drawWalls() {
    glColor3f(0.7f, 0.7f, 0.7f);
    for (int x = 0; x < WALL_THICKNESS; x++) {
        for (int y = 0; y < HEIGHT / CELL_SIZE; y++) {
            drawSquare(x, y);
            drawSquare((WIDTH / CELL_SIZE) - 1 - x, y);
        }
    }
    for (int y = 0; y < WALL_THICKNESS; y++) {
        for (int x = 0; x < WIDTH / CELL_SIZE; x++) {
            drawSquare(x, y);
            drawSquare(x, (HEIGHT / CELL_SIZE) - 1 - y);
        }
    }
    for (int i = 0; i < wallsCount; i++)
        drawSquare(walls[i][0], walls[i][1]);
}

void renderText(const char* text, float x, float y, float scale) {
    static GLfloat vertices[4096];
    unsigned char color[] = { 255, 255, 255, 255 };
    int num_quads = stb_easy_font_print(x, y, text, NULL, vertices, sizeof(vertices));
    for (int i = 0; i < num_quads * 4; i++) {
        vertices[i * 4 + 0] = x + (vertices[i * 4 + 0] - x) * scale;
        vertices[i * 4 + 1] = y + (vertices[i * 4 + 1] - y) * scale;
    }
    glColor3ub(color[0], color[1], color[2]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, vertices);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void drawMenu() {
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(WIDTH, 0);
    glVertex2f(WIDTH, HEIGHT);
    glVertex2f(0, HEIGHT);
    glEnd();

    const char* levels[] = { "1", "2", "3" };
    const float squareSize = 80.0f;
    const float spacing = 40.0f;
    const float totalWidth = 3 * squareSize + 2 * spacing;
    float startX = (WIDTH - totalWidth) / 2;
    float y = HEIGHT / 2 - squareSize / 2;

    for (int i = 0; i < 3; i++) {
        float x = startX + i * (squareSize + spacing);
        glColor3f(0.4f, 0.4f, 0.4f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + squareSize, y);
        glVertex2f(x + squareSize, y + squareSize);
        glVertex2f(x, y + squareSize);
        glEnd();

        float textWidth = stb_easy_font_width(levels[i]) * 3.0f;
        float textX = x + (squareSize - textWidth) / 2;
        float textY = y + squareSize / 2 - 15;
        renderText(levels[i], textX, textY, 3.0f);
    }

    renderText("Press I for info", 20, HEIGHT - 40, 2.0f);
}

void drawInfo() {
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(WIDTH, 0);
    glVertex2f(WIDTH, HEIGHT);
    glVertex2f(0, HEIGHT);
    glEnd();

    renderText("Bonus Info", WIDTH / 2 - 80, 50, 3.0f);

    struct BonusInfo {
        float color[3];
        const char* text;
        int ypos;
    } bonuses[] = {
        {{1.0f, 1.0f, 0.0f}, "* Yellow - x2 points for 10sec", 150},
        {{1.0f, 0.5f, 0.7f}, "* Pink - remove 3 tail segments", 250},
        {{0.0f, 0.0f, 1.0f}, "* Blue - +5 points +2 segments", 350},
        {{1.0f, 1.0f, 1.0f}, " Bonuses appear after 3 points", 450},
        {{1.0f, 1.0f, 1.0f}, " appears 30sec, disappears 10sec", 500}
    };

    for (int i = 0; i < 5; i++) {
        if (i < 3) {
            glColor3fv(bonuses[i].color);
            drawSquare(WIDTH / 2 - 150, bonuses[i].ypos - 15);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        renderText(bonuses[i].text, WIDTH / 2 - 140, bonuses[i].ypos, 2.0f);
    }

    renderText("Press I to return", WIDTH - 220, HEIGHT - 40, 2.0f);
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (gameState == PLAYING) {
        drawWalls();
        glColor3f(0.0f, 1.0f, 0.0f);
        for (int i = 0; i < snakeLength; i++) {
            drawSquare(snake[i].x, snake[i].y);
        }
        glColor3f(1.0f, 0.0f, 0.0f);
        drawSquare(foodX, foodY);

        if (currentBonus.type != -1) {
            switch (currentBonus.type) {
            case 0: glColor3f(1.0f, 1.0f, 0.0f); break;
            case 1: glColor3f(1.0f, 0.41f, 0.71f); break;
            case 2: glColor3f(0.0f, 0.0f, 1.0f); break;
            }
            drawSquare(currentBonus.x, currentBonus.y);
        }

        if (!gameOver) {
            char scoreText[32];
            sprintf(scoreText, "Score: %d", score);
            renderText(scoreText, 25, 30, 4.0f);
        }

        if (gameOver) {
            glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
            glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(WIDTH, 0);
            glVertex2f(WIDTH, HEIGHT);
            glVertex2f(0, HEIGHT);
            glEnd();

            char gameOverText[64];
            sprintf(gameOverText, "Game Over! Score: %d", score);
            float textWidth = stb_easy_font_width(gameOverText) * 4.0f;
            float textHeight = 50.0f;
            renderText(gameOverText, (WIDTH - textWidth) / 2, (HEIGHT - textHeight) / 2 - 60, 4.0f);

            const char* restartText = "Press R to restart";
            float restartWidth = stb_easy_font_width(restartText) * 3.0f;
            renderText(restartText, (WIDTH - restartWidth) / 2, (HEIGHT - textHeight) / 2 + 40, 3.0f);

            const char* menuText = "Press E to exit to menu";
            float menuWidth = stb_easy_font_width(menuText) * 3.0f;
            renderText(menuText, (WIDTH - menuWidth) / 2, (HEIGHT - textHeight) / 2 + 100, 3.0f);
        }
    }
    else if (gameState == MENU) {
        drawMenu();
    }
    else if (gameState == INFO) {
        drawInfo();
    }
}

int main() {
    srand((unsigned int)time(NULL));  //    

    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Snake Game", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    gameState = MENU;
    double lastUpdateTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        processInput(window);

        if (currentTime - lastUpdateTime >= UPDATE_INTERVAL && gameState == PLAYING) {
            updateGame();
            lastUpdateTime = currentTime;
        }

        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
