#ifndef HIP_BOT_H
#define HIP_BOT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- *
|                  |
|   CPU STRUCTS    |
|                  |
* ----------------- */

// CPU версия структур для передачи данных
typedef struct {
    int x;
    int y;
} CPUSegment;

typedef struct {
    int x;
    int y;
    int type;
    int lifetime;
} CPUBonus;

typedef struct {
    CPUSegment snake[500];
    int snakeLength;
    int foodX;
    int foodY;
    CPUBonus bonus;
    int walls[200][2];
    int wallsCount;
    int score;
    int multiplierCount;
    int fieldMinX;
    int fieldMinY;
    int fieldMaxX;
    int fieldMaxY;
} CPUGameState;

typedef struct {
    int direction;      // Лучшее направление
    float score;        // Оценка хода
    int simulations;    // Количество симуляций
    float avg_time_ms;  // Среднее время симуляции
} BotDecision;

/* ---------------- *
|                  |
|   GPU INTERFACE  |
|                  |
* ----------------- */

// Инициализация GPU бота
int hip_bot_init(void);

// Освобождение ресурсов
void hip_bot_cleanup(void);

// Получить решение бота используя GPU
// depth: глубина симуляции (3-15)
// hunger_penalty: текущий штраф за голод
BotDecision hip_bot_get_decision(
    const CPUGameState* state,
    int depth,
    float hunger_penalty
);

// Получить информацию о GPU
void hip_bot_print_device_info(void);

// Проверить доступность GPU
int hip_bot_is_available(void);

// Установить уровень детализации логов
void hip_bot_set_verbose(int level);

// Бенчмарк GPU бота
typedef struct {
    float total_time_ms;
    float kernel_time_ms;
    float transfer_time_ms;
    int simulations_per_second;
    int gpu_memory_used_mb;
} GPUBenchmarkResult;

GPUBenchmarkResult hip_bot_benchmark(int num_iterations, int depth);

#ifdef __cplusplus
}
#endif

#endif // HIP_BOT_H


