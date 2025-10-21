#define _CRT_SECURE_NO_WARNINGS
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>     
#include <time.h>
#include <math.h>
#include <limits.h>

#define WIDTH 800
#define HEIGHT 600
#define CELL_SIZE 20 
#define UPDATE_INTERVAL 0.00000000000015
#define WALL_THICKNESS 1
#define MAX_SNAKE_LENGTH 500
#define MAX_WALLS 200
#define SIMULATION_DEPTH 3
#define BONUS_LIFETIME 100

// Структура для хранения координат сегмента змейки

typedef struct {
    int x;
    int y;
} Segment;
// Структура для хранения данных бонуса

typedef struct {
    int x;
    int y;
    int type;
    int lifetime;
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

Bonus currentBonus = { -1, -1, -1, 0 };
int multiplierCount = 0;
int multiplierActive = 0;

enum GameState gameState = MENU;
int currentLevel = 1;

const int fieldMinX = WALL_THICKNESS;
const int fieldMaxX = (WIDTH / CELL_SIZE) - WALL_THICKNESS - 1;
const int fieldMinY = WALL_THICKNESS;
const int fieldMaxY = (HEIGHT / CELL_SIZE) - WALL_THICKNESS - 1;


int botEnabled = 1;
int panicMode = 0;


int hungerCounter = 0;
float currentHungerPenalty = 0.0f;

double start_time = -1.;
double gameover_time = -1.;
double update_count = 0.;
double avg_fps = 0.;
// Структура для хранения узла для поиска пути A*

typedef struct {
    int x, y;
    int f, g, h;
    int parentX, parentY;
} Node;

Node nodes[40][30];
int closedList[40][30];
int openList[40][30];
// Структура для хранения состояния симуляции бота

typedef struct {
    Segment snake[MAX_SNAKE_LENGTH];
    int snakeLength;
    int foodX, foodY;
    Bonus bonus;
    int score;
    int multiplierCount;
    int walls[MAX_WALLS][2];
    int wallsCount;
} SimulationState;
// Функция обновляет штраф за голод для змейки


void updateHungerPenalty() {
    if (snake[0].x == foodX && snake[0].y == foodY) {

        hungerCounter = 0;
        currentHungerPenalty = 0.0f;
    }
    else {

        hungerCounter++;

        currentHungerPenalty = hungerCounter * hungerCounter * 0.1f;


        if (currentHungerPenalty > 50.0f) {
            currentHungerPenalty = 50.0f;
        }
    }
}
// Функция эвристики для алгоритма A*


int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}
// Функция инициализации узлов для алгоритма A*

void initNodes() {
    for (int x = fieldMinX; x <= fieldMaxX; x++) {
        for (int y = fieldMinY; y <= fieldMaxY; y++) {
            nodes[x][y].x = x;
            nodes[x][y].y = y;
            nodes[x][y].f = INT_MAX;
            nodes[x][y].g = INT_MAX;
            nodes[x][y].h = 0;
            nodes[x][y].parentX = -1;
            nodes[x][y].parentY = -1;
            closedList[x][y] = 0;
            openList[x][y] = 0;
        }
    }
}
// Проверяет, находится ли узел в пределах игрового поля

int isValidNode(int x, int y) {
    return x >= fieldMinX && x <= fieldMaxX && y >= fieldMinY && y <= fieldMaxY;
}
// Находит путь от старта к цели с помощью алгоритма A*


int findPath(int startX, int startY, int targetX, int targetY) {
    if (!isPositionValid(targetX, targetY)) return 0;

    initNodes();

    nodes[startX][startY].g = 0;
    nodes[startX][startY].h = heuristic(startX, startY, targetX, targetY);
    nodes[startX][startY].f = nodes[startX][startY].h;
    openList[startX][startY] = 1;

    int found = 0;

    while (1) {
        int minF = INT_MAX;
        int currentX = -1, currentY = -1;


        for (int x = fieldMinX; x <= fieldMaxX; x++) {
            for (int y = fieldMinY; y <= fieldMaxY; y++) {
                if (openList[x][y] && nodes[x][y].f < minF) {
                    minF = nodes[x][y].f;
                    currentX = x;
                    currentY = y;
                }
            }
        }

        if (currentX == -1 || currentY == -1) break;

        openList[currentX][currentY] = 0;
        closedList[currentX][currentY] = 1;

        if (currentX == targetX && currentY == targetY) {
            found = 1;
            break;
        }

        int dx[] = { 0, 1, 0, -1 };
        int dy[] = { -1, 0, 1, 0 };

        for (int i = 0; i < 4; i++) {
            int neighborX = currentX + dx[i];
            int neighborY = currentY + dy[i];

            if (!isValidNode(neighborX, neighborY)) continue;
            if (!isPositionValid(neighborX, neighborY)) continue;
            if (closedList[neighborX][neighborY]) continue;

            int tentativeG = nodes[currentX][currentY].g + 1;

            if (!openList[neighborX][neighborY] || tentativeG < nodes[neighborX][neighborY].g) {
                nodes[neighborX][neighborY].parentX = currentX;
                nodes[neighborX][neighborY].parentY = currentY;
                nodes[neighborX][neighborY].g = tentativeG;
                nodes[neighborX][neighborY].h = heuristic(neighborX, neighborY, targetX, targetY);
                nodes[neighborX][neighborY].f = nodes[neighborX][neighborY].g + nodes[neighborX][neighborY].h;

                if (!openList[neighborX][neighborY]) {
                    openList[neighborX][neighborY] = 1;
                }
            }
        }
    }

    return found;
}
// Вычисляет реальное расстояние с учётом стен и препятствий


int calculateActualDistance(int startX, int startY, int targetX, int targetY) {
    if (!findPath(startX, startY, targetX, targetY)) {
        return 1000;
    }

    return nodes[targetX][targetY].g;
}
// Проверяет, можно ли переместиться в указанную клетку


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
// Проверяет допустимость позиции в режиме симуляции

int isPositionValidInSimulation(SimulationState* state, int x, int y) {
    if (x < fieldMinX || x > fieldMaxX || y < fieldMinY || y > fieldMaxY)
        return 0;
    for (int i = 0; i < state->wallsCount; i++)
        if (x == state->walls[i][0] && y == state->walls[i][1])
            return 0;
    for (int i = 0; i < state->snakeLength; i++)
        if (x == state->snake[i].x && y == state->snake[i].y)
            return 0;
    return 1;
}
// Копирует текущее состояние игры в структуру симуляции

void copyGameStateToSimulation(SimulationState* sim) {

    for (int i = 0; i < snakeLength; i++) {
        sim->snake[i].x = snake[i].x;
        sim->snake[i].y = snake[i].y;
    }
    sim->snakeLength = snakeLength;


    sim->foodX = foodX;
    sim->foodY = foodY;
    sim->bonus.x = currentBonus.x;
    sim->bonus.y = currentBonus.y;
    sim->bonus.type = currentBonus.type;
    sim->bonus.lifetime = currentBonus.lifetime;


    for (int i = 0; i < wallsCount; i++) {
        sim->walls[i][0] = walls[i][0];
        sim->walls[i][1] = walls[i][1];
    }
    sim->wallsCount = wallsCount;


    sim->score = score;
    sim->multiplierCount = multiplierCount;
}
// Выполняет один ход в симуляции

int makeMoveInSimulation(SimulationState* state, enum Direction moveDir) {
    if (state->snakeLength <= 0) return 0;

    Segment newHead = state->snake[0];
    switch (moveDir) {
    case UP:    newHead.y--; break;
    case DOWN:  newHead.y++; break;
    case LEFT:  newHead.x--; break;
    case RIGHT: newHead.x++; break;
    case NONE: return 0;
    }


    if (!isPositionValidInSimulation(state, newHead.x, newHead.y))
        return 0;


    for (int i = state->snakeLength - 1; i > 0; i--) {
        state->snake[i] = state->snake[i - 1];
    }
    state->snake[0] = newHead;


    if (newHead.x == state->foodX && newHead.y == state->foodY) {
        if (state->snakeLength < MAX_SNAKE_LENGTH) {
            state->snakeLength++;
            state->snake[state->snakeLength - 1] = state->snake[state->snakeLength - 2];
        }


        do {
            state->foodX = fieldMinX + rand() % (fieldMaxX - fieldMinX + 1);
            state->foodY = fieldMinY + rand() % (fieldMaxY - fieldMinY + 1);
        } while (!isPositionValidInSimulation(state, state->foodX, state->foodY));
    }


    if (state->bonus.type != -1 && newHead.x == state->bonus.x && newHead.y == state->bonus.y) {
        state->bonus.type = -1;
    }

    return 1;
}
// Вычисляет количество доступных клеток в симуляции

int calculateFreeSpaceInSimulation(SimulationState* state) {
    int visited[40][30] = { 0 };
    int queue[1200][2];
    int front = 0, rear = 0;
    int count = 0;

    int startX = state->snake[0].x;
    int startY = state->snake[0].y;

    queue[rear][0] = startX;
    queue[rear][1] = startY;
    rear++;
    visited[startX][startY] = 1;
    count++;

    int dx[] = { 0, 1, 0, -1 };
    int dy[] = { -1, 0, 1, 0 };

    while (front < rear) {
        int x = queue[front][0];
        int y = queue[front][1];
        front++;

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx >= fieldMinX && nx <= fieldMaxX && ny >= fieldMinY && ny <= fieldMaxY &&
                !visited[nx][ny] && isPositionValidInSimulation(state, nx, ny)) {
                visited[nx][ny] = 1;
                queue[rear][0] = nx;
                queue[rear][1] = ny;
                rear++;
                count++;
            }
        }
    }

    return count;
}
// Оценивает состояние симуляции по нескольким критериям


float evaluateSimulationState(SimulationState* state) {
    if (state->snakeLength <= 0) return -10000.0f;

    float score = 0.0f;
    int headX = state->snake[0].x;
    int headY = state->snake[0].y;


    int foodDist = calculateActualDistance(headX, headY, state->foodX, state->foodY);
    int bonusDist = (state->bonus.type != -1) ?
        calculateActualDistance(headX, headY, state->bonus.x, state->bonus.y) : 1000;


    if (headX == state->foodX && headY == state->foodY) {
        score += 300.0f;
    }


    score -= foodDist * 2.0f;


    if (state->bonus.type != -1) {

        if (headX == state->bonus.x && headY == state->bonus.y) {
            score += 1000.0f;
        }


        score -= bonusDist * 1.5f;


        if (bonusDist < 5) {
            score += (5 - bonusDist) * 10.0f;
        }
    }


    int safeMoves = 0;
    int dx[] = { 0, 1, 0, -1 };
    int dy[] = { -1, 0, 1, 0 };

    for (int i = 0; i < 4; i++) {
        int nx = headX + dx[i];
        int ny = headY + dy[i];
        if (isPositionValidInSimulation(state, nx, ny)) {
            safeMoves++;
        }
    }


    float safetyMultiplier = 1.0f;
    if (safeMoves == 0) {
        score -= 1000.0f;
        safetyMultiplier = 0.1f;
    }
    else if (safeMoves == 1) {
        score -= 30.0f;
        safetyMultiplier = 0.5f;
    }
    else if (safeMoves == 2) {
        score -= 5.0f;
        safetyMultiplier = 0.8f;
    }
    else {
        score += safeMoves * 2.0f;
        safetyMultiplier = 1.0f;
    }


    float adjustedHungerPenalty = currentHungerPenalty * safetyMultiplier;
    score -= adjustedHungerPenalty;


    int freeSpace = calculateFreeSpaceInSimulation(state);
    score += freeSpace * 0.1f;


    score += state->snakeLength * 1.0f;

    return score;
}
// Рекурсивно симулирует игру для оценки возможных ходов


float simulateGame(SimulationState* state, enum Direction firstMove, int depth) {

    if (depth <= 0) {
        return evaluateSimulationState(state);
    }

    SimulationState currentState = *state;
    float moveScore = 0.0f;


    if (!makeMoveInSimulation(&currentState, firstMove)) {
        return -10000.0f;
    }



    if (currentState.snake[0].x == foodX &&
        currentState.snake[0].y == foodY) {
        moveScore += 500.0f;
    }


    if (currentState.bonus.type != -1 &&
        currentState.snake[0].x == currentState.bonus.x &&
        currentState.snake[0].y == currentState.bonus.y) {
        moveScore += 1500.0f;
    }


    if (depth <= 1) {
        return moveScore + evaluateSimulationState(&currentState);
    }


    float bestFutureScore = -10000.0f;
    enum Direction directions[] = { UP, RIGHT, DOWN, LEFT };
    int headX = currentState.snake[0].x;
    int headY = currentState.snake[0].y;

    for (int i = 0; i < 4; i++) {
        int nx = headX, ny = headY;
        switch (directions[i]) {
        case UP:    ny--; break;
        case DOWN:  ny++; break;
        case LEFT:  nx--; break;
        case RIGHT: nx++; break;
        case NONE: continue;
        }


        if (isPositionValidInSimulation(&currentState, nx, ny)) {

            float score = simulateGame(&currentState, directions[i], depth - 1);
            if (score > bestFutureScore) {
                bestFutureScore = score;
            }
        }
    }



    if (bestFutureScore <= -10000.0f) {
        return moveScore + evaluateSimulationState(&currentState);
    }


    return moveScore + bestFutureScore;
}
// Управляет ботом с использованием симуляции состояний


void botControlWithSimulation() {
    if (gameOver || !botEnabled) return;


    updateHungerPenalty();

    SimulationState baseState;
    copyGameStateToSimulation(&baseState);

    float bestScore = -10000.0f;
    enum Direction bestDir = NONE;
    enum Direction directions[] = { UP, RIGHT, DOWN, LEFT };


    if (currentBonus.type != -1) {
        for (int i = 0; i < 4; i++) {
            Segment testHead = baseState.snake[0];
            switch (directions[i]) {
            case UP:    testHead.y--; break;
            case DOWN:  testHead.y++; break;
            case LEFT:  testHead.x--; break;
            case RIGHT: testHead.x++; break;
            case NONE: continue;
            }


            if (testHead.x == currentBonus.x && testHead.y == currentBonus.y) {
                if (isPositionValidInSimulation(&baseState, testHead.x, testHead.y)) {
                    pendingDir = directions[i];
                    return;
                }
            }
        }
    }


    for (int i = 0; i < 4; i++) {
        Segment testHead = baseState.snake[0];
        switch (directions[i]) {
        case UP:    testHead.y--; break;
        case DOWN:  testHead.y++; break;
        case LEFT:  testHead.x--; break;
        case RIGHT: testHead.x++; break;
        case NONE: continue;
        }


        if (testHead.x == foodX && testHead.y == foodY) {
            if (isPositionValidInSimulation(&baseState, testHead.x, testHead.y)) {
                pendingDir = directions[i];
                return;
            }
        }
    }


    int simulationDepth = SIMULATION_DEPTH;

    for (int i = 0; i < 4; i++) {
        Segment testHead = baseState.snake[0];
        switch (directions[i]) {
        case UP:    testHead.y--; break;
        case DOWN:  testHead.y++; break;
        case LEFT:  testHead.x--; break;
        case RIGHT: testHead.x++; break;
        case NONE: continue;
        }

        if (!isPositionValidInSimulation(&baseState, testHead.x, testHead.y)) {
            continue;
        }

        float score = simulateGame(&baseState, directions[i], simulationDepth);


        if (directions[i] == dir) {
            score += 3.0f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDir = directions[i];
        }
    }

    if (bestDir != NONE) {
        pendingDir = bestDir;
        return;
    }


    int headX = snake[0].x;
    int headY = snake[0].y;
    enum Direction safeDirs[] = { UP, RIGHT, DOWN, LEFT };

    for (int i = 0; i < 4; i++) {
        int nx = headX, ny = headY;
        switch (safeDirs[i]) {
        case UP:    ny--; break;
        case DOWN:  ny++; break;
        case LEFT:  nx--; break;
        case RIGHT: nx++; break;
        case NONE: continue;
        }

        if (isPositionValid(nx, ny)) {
            pendingDir = safeDirs[i];
            return;
        }
    }
}
// Вызывает функцию управления ботом


void botControl() {
    botControlWithSimulation();
}
// Вычисляет количество свободных клеток на поле

int calculateFreeSpace() {
    int visited[40][30] = { 0 };
    int queue[1200][2];
    int front = 0, rear = 0;
    int count = 0;

    queue[rear][0] = snake[0].x;
    queue[rear][1] = snake[0].y;
    rear++;
    visited[snake[0].x][snake[0].y] = 1;
    count++;

    int dx[] = { 0, 1, 0, -1 };
    int dy[] = { -1, 0, 1, 0 };

    while (front < rear) {
        int x = queue[front][0];
        int y = queue[front][1];
        front++;

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (isValidNode(nx, ny) && !visited[nx][ny] && isPositionValid(nx, ny)) {
                visited[nx][ny] = 1;
                queue[rear][0] = nx;
                queue[rear][1] = ny;
                rear++;
                count++;
            }
        }
    }

    return count;
}
// Инициализирует игру и загружает уровень

void initGame(int level) {
    start_time = glfwGetTime();
    avg_fps = 0;

    update_count = 0;

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
    currentBonus.type = -1;
    multiplierCount = 0;
    panicMode = 0;


    hungerCounter = 0;
    currentHungerPenalty = 0.0f;

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
// Создаёт новый бонус на поле

int generateBonus() {
    int valid;
    int attempts = 0;
    const int max_attempts = 1000;

    do {
        valid = 1;
        currentBonus.x = fieldMinX + rand() % (fieldMaxX - fieldMinX + 1);
        currentBonus.y = fieldMinY + rand() % (fieldMaxY - fieldMinY + 1);
        currentBonus.type = rand() % 3;
        currentBonus.lifetime = BONUS_LIFETIME;

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
// Устанавливает состояние Game Over и вычисляет FPS

void set_gameover() {
    gameOver = 1;
    gameover_time = glfwGetTime();
    if (gameover_time > start_time) {
        avg_fps = update_count / (gameover_time - start_time);
    }
    else {
        avg_fps = 0;
    }
    return;
}
// Применяет эффект выбранного бонуса

void applyBonusEffect(int type) {
    switch (type) {
    case 0:
        multiplierCount = 5;
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
// Обрабатывает ввод с клавиатуры

void processInput(GLFWwindow* window) {
    static int iKeyPressed = 0;
    static int rKeyPressed = 0;
    static int eKeyPressed = 0;
    static int bKeyPressed = 0;

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

        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bKeyPressed) {
            botEnabled = !botEnabled;
            bKeyPressed = 1;
        }
        else if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) {
            bKeyPressed = 0;
        }

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


        if (!botEnabled) {
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && dir != DOWN)
                pendingDir = UP;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && dir != UP)
                pendingDir = DOWN;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && dir != RIGHT)
                pendingDir = LEFT;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && dir != LEFT)
                pendingDir = RIGHT;
        }

        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS && !iKeyPressed) {
            gameState = MENU;
            iKeyPressed = 1;
        }
        else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_RELEASE) {
            iKeyPressed = 0;
        }
    }
}
// Обновляет состояние игры на каждом шаге

void updateGame() {
    if (gameOver || gameState != PLAYING) return;


    if (botEnabled) {
        botControl();
    }

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
        set_gameover();
        return;
    }
    for (int i = 0; i < wallsCount; i++) {
        if (newHead.x == walls[i][0] && newHead.y == walls[i][1]) {
            set_gameover();
            return;
        }
    }
    for (int i = 0; i < snakeLength; i++) {
        if (newHead.x == snake[i].x && newHead.y == snake[i].y) {
            set_gameover();
            return;
        }
    }

    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = newHead;

    if (newHead.x == foodX && newHead.y == foodY) {

        if (multiplierCount > 0) {
            score += 2;
            multiplierCount--;
        }
        else {
            score += 1;
        }

        if (snakeLength < MAX_SNAKE_LENGTH) {
            snakeLength++;
            snake[snakeLength - 1] = snake[snakeLength - 2];
        }

        foodEaten++;


        if (foodEaten >= 5 && currentBonus.type == -1) {
            if (generateBonus()) {
                foodEaten = 0;
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


    if (currentBonus.type != -1) {
        currentBonus.lifetime--;
        if (currentBonus.lifetime <= 0) {
            currentBonus.type = -1;
        }
    }


    updateHungerPenalty();
}
// Рисует квадратную ячейку на экране

void drawSquare(int x, int y) {
    glBegin(GL_QUADS);
    glVertex2f(x * CELL_SIZE, y * CELL_SIZE);
    glVertex2f((x + 1) * CELL_SIZE, y * CELL_SIZE);
    glVertex2f((x + 1) * CELL_SIZE, (y + 1) * CELL_SIZE);
    glVertex2f(x * CELL_SIZE, (y + 1) * CELL_SIZE);
    glEnd();
}
// Рисует стены игрового поля

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
// Выводит текст на экран

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
// Отображает главное меню игры

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
// Отображает экран с информацией о бонусах

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
        {{1.0f, 1.0f, 0.0f}, "* Yellow - x2 points for 5 apples", 150},
        {{1.0f, 0.5f, 0.7f}, "* Pink - remove 3 tail segments", 250},
        {{0.0f, 0.0f, 1.0f}, "* Blue - +5 points +2 segments", 350},
        {{1.0f, 1.0f, 1.0f}, " Bonuses appear after 5 points", 450},
        {{1.0f, 1.0f, 1.0f}, " disappears after 25 moves", 500}
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
// Основная функция отрисовки текущего состояния

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


            char bonusText[32];
            sprintf(bonusText, "%d", currentBonus.lifetime);
            renderText(bonusText, currentBonus.x * CELL_SIZE + 5, currentBonus.y * CELL_SIZE + 5, 2.0f);
        }

        if (!gameOver) {
            char scoreText[32];
            sprintf(scoreText, "Score: %d", score);
            renderText(scoreText, 25, 30, 4.0f);


            if (multiplierCount > 0) {
                char multiplierText[32];
                sprintf(multiplierText, "x2: %d", multiplierCount);
                renderText(multiplierText, 25, 100, 3.0f);
            }


            if (botEnabled) {
                renderText("Bot: ON", WIDTH - 150, 30, 3.0f);
                if (panicMode) {
                    renderText("PANIC MODE", WIDTH - 200, 60, 3.0f);
                }
            }
            else {
                renderText("Bot: OFF", WIDTH - 150, 30, 3.0f);
            }
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
            sprintf(gameOverText, "Game Over! Score: %d, FPS: %f", score, avg_fps);
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
// Основная функция программы

int main() {
    srand((unsigned int)time(NULL));

    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Snake Game with Improved Bot", NULL, NULL);
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
            update_count += 1;
            lastUpdateTime = currentTime;
        }

        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}