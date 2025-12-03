#ifndef HIP_TYPES_H
#define HIP_TYPES_H

#include <hip/hip_runtime.h>

/* ---------------- *
|                  |
|   GPU CONSTANTS  |
|                  |
* ----------------- */

#define GPU_MAX_SNAKE_LENGTH 500
#define GPU_MAX_WALLS 200
#define GPU_FIELD_WIDTH 40
#define GPU_FIELD_HEIGHT 30
#define GPU_MAX_SIMULATIONS 4096  // Максимальное количество симуляций параллельно
#define GPU_MAX_DEPTH 15          // Максимальная глубина симуляции

/* ---------------- *
|                  |
|   GPU STRUCTS    |
|                  |
* ----------------- */

// SOA (Structure of Arrays) для эффективного доступа на GPU
typedef struct {
    int x;
    int y;
} __attribute__((aligned(8))) GPUSegment;

typedef struct {
    int x;
    int y;
    int type;
    int lifetime;
} __attribute__((aligned(16))) GPUBonus;

// Компактное представление игрового состояния для GPU
typedef struct {
    // Змейка
    GPUSegment snake[GPU_MAX_SNAKE_LENGTH];
    int snakeLength;
    
    // Еда и бонус
    int foodX;
    int foodY;
    GPUBonus bonus;
    
    // Стены
    short walls[GPU_MAX_WALLS][2];  // short вместо int для экономии памяти
    short wallsCount;
    
    // Игровая информация
    int score;
    int multiplierCount;
    
    // Границы поля
    short fieldMinX;
    short fieldMinY;
    short fieldMaxX;
    short fieldMaxY;
    
} __attribute__((aligned(16))) GPUGameState;

// Структура для результатов симуляции
typedef struct {
    float score;
    int direction;  // 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT
    int valid;      // 1 если симуляция успешна, 0 если игра закончилась
} __attribute__((aligned(16))) GPUSimulationResult;

// Структура для A* pathfinding
typedef struct {
    int x;
    int y;
    int f;  // f = g + h
    int g;  // стоимость от старта
    int h;  // эвристика до цели
    int parentX;
    int parentY;
} __attribute__((aligned(32))) GPUPathNode;

// Компактное игровое поле (битовая маска для оптимизации)
typedef struct {
    unsigned char cells[GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1];  // Битовая маска
} GPUFieldMask;

/* ---------------- *
|                  |
|   ENUMS          |
|                  |
* ----------------- */

enum GPUDirection {
    GPU_UP = 0,
    GPU_DOWN = 1,
    GPU_LEFT = 2,
    GPU_RIGHT = 3,
    GPU_NONE = 4
};

/* ---------------- *
|                  |
|   DEVICE FUNCS   |
|                  |
* ----------------- */

// Inline функции для работы на GPU
__device__ __forceinline__ int gpu_abs(int x) {
    return x < 0 ? -x : x;
}

__device__ __forceinline__ int gpu_min(int a, int b) {
    return a < b ? a : b;
}

__device__ __forceinline__ int gpu_max(int a, int b) {
    return a > b ? a : b;
}

__device__ __forceinline__ float gpu_minf(float a, float b) {
    return a < b ? a : b;
}

__device__ __forceinline__ float gpu_maxf(float a, float b) {
    return a > b ? a : b;
}

// Манхэттенское расстояние
__device__ __forceinline__ int gpu_manhattan_distance(int x1, int y1, int x2, int y2) {
    return gpu_abs(x1 - x2) + gpu_abs(y1 - y2);
}

// Проверка границ
__device__ __forceinline__ bool gpu_in_bounds(int x, int y, const GPUGameState* state) {
    return x >= state->fieldMinX && x <= state->fieldMaxX &&
           y >= state->fieldMinY && y <= state->fieldMaxY;
}

// Битовая маска для быстрой проверки занятости клетки
__device__ __forceinline__ void set_cell_occupied(GPUFieldMask* mask, int x, int y) {
    int idx = y * GPU_FIELD_WIDTH + x;
    mask->cells[idx / 8] |= (1 << (idx % 8));
}

__device__ __forceinline__ bool is_cell_occupied(const GPUFieldMask* mask, int x, int y) {
    int idx = y * GPU_FIELD_WIDTH + x;
    return (mask->cells[idx / 8] & (1 << (idx % 8))) != 0;
}

__device__ __forceinline__ void clear_field_mask(GPUFieldMask* mask) {
    for (int i = 0; i < GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1; i++) {
        mask->cells[i] = 0;
    }
}

/* ============================================
   SHARED DEVICE FUNCTIONS (inline for multiple files)
   ============================================ */

// Копирование состояния игры
__device__ __forceinline__ void copy_game_state(GPUGameState* dst, const GPUGameState* src) {
    // Копируем змейку
    for (int i = 0; i < src->snakeLength; i++) {
        dst->snake[i] = src->snake[i];
    }
    dst->snakeLength = src->snakeLength;
    
    // Копируем еду и бонус
    dst->foodX = src->foodX;
    dst->foodY = src->foodY;
    dst->bonus = src->bonus;
    
    // Копируем стены
    for (int i = 0; i < src->wallsCount; i++) {
        dst->walls[i][0] = src->walls[i][0];
        dst->walls[i][1] = src->walls[i][1];
    }
    dst->wallsCount = src->wallsCount;
    
    // Копируем игровую информацию
    dst->score = src->score;
    dst->multiplierCount = src->multiplierCount;
    
    // Копируем границы
    dst->fieldMinX = src->fieldMinX;
    dst->fieldMinY = src->fieldMinY;
    dst->fieldMaxX = src->fieldMaxX;
    dst->fieldMaxY = src->fieldMaxY;
}

#endif // HIP_TYPES_H


