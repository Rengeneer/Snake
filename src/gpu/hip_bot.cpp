#include "hip_bot.h"
#include "hip_types.h"
#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------------- *
|                  |
|   GLOBAL STATE   |
|                  |
* ----------------- */

static int g_verbose = 0;
static int g_initialized = 0;

// Device pointers
static GPUGameState* d_base_state = nullptr;
static GPUGameState* d_sim_states = nullptr;
static GPUSimulationResult* d_results = nullptr;
static int* d_distances_food = nullptr;
static int* d_distances_bonus = nullptr;
static int* d_free_spaces = nullptr;
static float* d_scores = nullptr;

// Host pointers for results
static GPUSimulationResult* h_results = nullptr;

static const int MAX_SIMULATIONS = GPU_MAX_SIMULATIONS;

/* ---------------- *
|                  |
|   ERROR HANDLING |
|                  |
* ----------------- */

#define HIP_CHECK(cmd) \
    do { \
        hipError_t error = cmd; \
        if (error != hipSuccess) { \
            fprintf(stderr, "HIP Error: '%s'(%d) at %s:%d\n", \
                    hipGetErrorString(error), error, __FILE__, __LINE__); \
            hip_bot_cleanup(); \
            return -1; \
        } \
    } while(0)

#define HIP_CHECK_VOID(cmd) \
    do { \
        hipError_t error = cmd; \
        if (error != hipSuccess) { \
            fprintf(stderr, "HIP Error: '%s'(%d) at %s:%d\n", \
                    hipGetErrorString(error), error, __FILE__, __LINE__); \
        } \
    } while(0)

/* ---------------- *
|                  |
|   KERNEL DECLARATIONS |
|                  |
* ----------------- */

extern "C" {
    __global__ void massive_simulation_kernel(
        const GPUGameState* base_state,
        GPUSimulationResult* results,
        int depth,
        int num_simulations
    );
    
    __global__ void parallel_bfs_kernel(
        const GPUGameState* states,
        int* free_space_counts,
        int num_states
    );
    
    __global__ void parallel_astar_kernel(
        const GPUGameState* states,
        const int* start_x,
        const int* start_y,
        const int* target_x,
        const int* target_y,
        int* distances,
        int num_paths
    );
    
    __global__ void evaluate_states_kernel(
        const GPUGameState* states,
        const int* distances_to_food,
        const int* distances_to_bonus,
        const int* free_spaces,
        float* scores,
        float hunger_penalty,
        int num_states
    );
}

/* ---------------- *
|                  |
|   INITIALIZATION |
|                  |
* ----------------- */

int hip_bot_init(void) {
    if (g_initialized) {
        return 0;  // Already initialized
    }
    
    // Проверка доступности GPU
    int deviceCount = 0;
    HIP_CHECK(hipGetDeviceCount(&deviceCount));
    
    if (deviceCount == 0) {
        fprintf(stderr, "No HIP-capable devices found!\n");
        return -1;
    }
    
    if (g_verbose) {
        printf("Found %d HIP device(s)\n", deviceCount);
        hip_bot_print_device_info();
    }
    
    // Выделение памяти на GPU
    HIP_CHECK(hipMalloc(&d_base_state, sizeof(GPUGameState)));
    HIP_CHECK(hipMalloc(&d_sim_states, MAX_SIMULATIONS * sizeof(GPUGameState)));
    HIP_CHECK(hipMalloc(&d_results, MAX_SIMULATIONS * sizeof(GPUSimulationResult)));
    HIP_CHECK(hipMalloc(&d_distances_food, MAX_SIMULATIONS * sizeof(int)));
    HIP_CHECK(hipMalloc(&d_distances_bonus, MAX_SIMULATIONS * sizeof(int)));
    HIP_CHECK(hipMalloc(&d_free_spaces, MAX_SIMULATIONS * sizeof(int)));
    HIP_CHECK(hipMalloc(&d_scores, MAX_SIMULATIONS * sizeof(float)));
    
    // Выделение памяти на CPU для результатов
    h_results = (GPUSimulationResult*)malloc(MAX_SIMULATIONS * sizeof(GPUSimulationResult));
    if (!h_results) {
        fprintf(stderr, "Failed to allocate host memory\n");
        hip_bot_cleanup();
        return -1;
    }
    
    g_initialized = 1;
    
    if (g_verbose) {
        printf("GPU Bot initialized successfully\n");
        printf("Allocated %.2f MB on GPU\n", 
               (MAX_SIMULATIONS * (sizeof(GPUGameState) + sizeof(GPUSimulationResult) + 
                sizeof(int) * 3 + sizeof(float)) + sizeof(GPUGameState)) / (1024.0 * 1024.0));
    }
    
    return 0;
}

void hip_bot_cleanup(void) {
    if (!g_initialized) return;
    
    if (d_base_state) hipFree(d_base_state);
    if (d_sim_states) hipFree(d_sim_states);
    if (d_results) hipFree(d_results);
    if (d_distances_food) hipFree(d_distances_food);
    if (d_distances_bonus) hipFree(d_distances_bonus);
    if (d_free_spaces) hipFree(d_free_spaces);
    if (d_scores) hipFree(d_scores);
    
    if (h_results) free(h_results);
    
    d_base_state = nullptr;
    d_sim_states = nullptr;
    d_results = nullptr;
    d_distances_food = nullptr;
    d_distances_bonus = nullptr;
    d_free_spaces = nullptr;
    d_scores = nullptr;
    h_results = nullptr;
    
    g_initialized = 0;
    
    if (g_verbose) {
        printf("GPU Bot cleanup complete\n");
    }
}

/* ---------------- *
|                  |
|   CONVERSION     |
|                  |
* ----------------- */

static void cpu_to_gpu_state(GPUGameState* gpu_state, const CPUGameState* cpu_state) {
    // Копируем змейку
    for (int i = 0; i < cpu_state->snakeLength && i < GPU_MAX_SNAKE_LENGTH; i++) {
        gpu_state->snake[i].x = cpu_state->snake[i].x;
        gpu_state->snake[i].y = cpu_state->snake[i].y;
    }
    gpu_state->snakeLength = cpu_state->snakeLength;
    
    // Копируем еду и бонус
    gpu_state->foodX = cpu_state->foodX;
    gpu_state->foodY = cpu_state->foodY;
    gpu_state->bonus.x = cpu_state->bonus.x;
    gpu_state->bonus.y = cpu_state->bonus.y;
    gpu_state->bonus.type = cpu_state->bonus.type;
    gpu_state->bonus.lifetime = cpu_state->bonus.lifetime;
    
    // Копируем стены
    for (int i = 0; i < cpu_state->wallsCount && i < GPU_MAX_WALLS; i++) {
        gpu_state->walls[i][0] = (short)cpu_state->walls[i][0];
        gpu_state->walls[i][1] = (short)cpu_state->walls[i][1];
    }
    gpu_state->wallsCount = (short)cpu_state->wallsCount;
    
    // Копируем игровую информацию
    gpu_state->score = cpu_state->score;
    gpu_state->multiplierCount = cpu_state->multiplierCount;
    
    // Копируем границы
    gpu_state->fieldMinX = (short)cpu_state->fieldMinX;
    gpu_state->fieldMinY = (short)cpu_state->fieldMinY;
    gpu_state->fieldMaxX = (short)cpu_state->fieldMaxX;
    gpu_state->fieldMaxY = (short)cpu_state->fieldMaxY;
}

/* ---------------- *
|                  |
|   MAIN FUNCTION  |
|                  |
* ----------------- */

BotDecision hip_bot_get_decision(
    const CPUGameState* state,
    int depth,
    float hunger_penalty
) {
    BotDecision decision;
    decision.direction = 3;  // RIGHT by default
    decision.score = -10000.0f;
    decision.simulations = 0;
    decision.avg_time_ms = 0.0f;
    
    if (!g_initialized) {
        if (hip_bot_init() != 0) {
            fprintf(stderr, "Failed to initialize GPU bot\n");
            return decision;
        }
    }
    
    // Ограничение глубины
    if (depth < 1) depth = 1;
    if (depth > GPU_MAX_DEPTH) depth = GPU_MAX_DEPTH;
    
    // Вычисляем количество симуляций для данной глубины
    int num_simulations = 1;
    for (int i = 0; i < depth; i++) {
        num_simulations *= 4;
        if (num_simulations > MAX_SIMULATIONS) {
            num_simulations = MAX_SIMULATIONS;
            break;
        }
    }
    
    decision.simulations = num_simulations;
    
    // Создание событий для замера времени
    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);
    hipEventRecord(start, 0);
    
    // Конвертация состояния CPU -> GPU
    GPUGameState h_base_state;
    cpu_to_gpu_state(&h_base_state, state);
    
    // Копирование на GPU
    HIP_CHECK_VOID(hipMemcpy(d_base_state, &h_base_state, sizeof(GPUGameState), hipMemcpyHostToDevice));
    
    // Настройка размеров блоков и сетки
    int threadsPerBlock = 256;
    int blocksPerGrid = (num_simulations + threadsPerBlock - 1) / threadsPerBlock;
    
    if (g_verbose) {
        printf("Launching kernel with %d blocks, %d threads per block\n", blocksPerGrid, threadsPerBlock);
        printf("Total simulations: %d, Depth: %d\n", num_simulations, depth);
    }
    
    // Запуск kernel массовой симуляции
    hipLaunchKernelGGL(massive_simulation_kernel,
                       dim3(blocksPerGrid),
                       dim3(threadsPerBlock),
                       0, 0,  // shared memory and stream
                       d_base_state,
                       d_results,
                       depth,
                       num_simulations);
    
    // Проверка ошибок запуска kernel
    HIP_CHECK_VOID(hipGetLastError());
    
    // Копирование результатов обратно на CPU
    HIP_CHECK_VOID(hipMemcpy(h_results, d_results, 
                              num_simulations * sizeof(GPUSimulationResult), 
                              hipMemcpyDeviceToHost));
    
    // Ожидание завершения
    HIP_CHECK_VOID(hipDeviceSynchronize());
    
    // Замер времени
    hipEventRecord(stop, 0);
    hipEventSynchronize(stop);
    float elapsed_ms = 0;
    hipEventElapsedTime(&elapsed_ms, start, stop);
    decision.avg_time_ms = elapsed_ms;
    
    // Уничтожение событий
    hipEventDestroy(start);
    hipEventDestroy(stop);
    
    // Анализ результатов: группируем по первому ходу и находим лучший
    float direction_scores[4] = {-10000.0f, -10000.0f, -10000.0f, -10000.0f};
    int direction_counts[4] = {0, 0, 0, 0};
    
    for (int i = 0; i < num_simulations; i++) {
        if (h_results[i].valid) {
            int dir = h_results[i].direction;
            if (dir >= 0 && dir < 4) {
                direction_scores[dir] += h_results[i].score;
                direction_counts[dir]++;
            }
        }
    }
    
    // Нормализация и выбор лучшего направления
    int best_dir = 0;
    float best_score = -10000.0f;
    
    for (int dir = 0; dir < 4; dir++) {
        if (direction_counts[dir] > 0) {
            float avg_score = direction_scores[dir] / direction_counts[dir];
            
            if (g_verbose) {
                printf("Direction %d: avg_score=%.2f, count=%d\n", 
                       dir, avg_score, direction_counts[dir]);
            }
            
            if (avg_score > best_score) {
                best_score = avg_score;
                best_dir = dir;
            }
        }
    }
    
    decision.direction = best_dir;
    decision.score = best_score;
    
    if (g_verbose) {
        printf("Selected direction: %d, score: %.2f, time: %.3f ms\n", 
               best_dir, best_score, elapsed_ms);
    }
    
    return decision;
}

/* ---------------- *
|                  |
|   UTILITY        |
|                  |
* ----------------- */

void hip_bot_print_device_info(void) {
    int deviceCount = 0;
    hipGetDeviceCount(&deviceCount);
    
    for (int dev = 0; dev < deviceCount; dev++) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, dev);
        
        printf("\n=== Device %d: %s ===\n", dev, prop.name);
        printf("  Compute Capability: %d.%d\n", prop.major, prop.minor);
        printf("  Total Global Memory: %.2f GB\n", prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
        printf("  Shared Memory per Block: %.2f KB\n", prop.sharedMemPerBlock / 1024.0);
        printf("  Registers per Block: %d\n", prop.regsPerBlock);
        printf("  Warp Size: %d\n", prop.warpSize);
        printf("  Max Threads per Block: %d\n", prop.maxThreadsPerBlock);
        printf("  Max Threads Dim: (%d, %d, %d)\n", 
               prop.maxThreadsDim[0], prop.maxThreadsDim[1], prop.maxThreadsDim[2]);
        printf("  Max Grid Size: (%d, %d, %d)\n", 
               prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2]);
        printf("  Clock Rate: %.2f GHz\n", prop.clockRate / 1000000.0);
        printf("  Memory Clock Rate: %.2f GHz\n", prop.memoryClockRate / 1000000.0);
        printf("  Memory Bus Width: %d-bit\n", prop.memoryBusWidth);
        printf("  L2 Cache Size: %.2f MB\n", prop.l2CacheSize / (1024.0 * 1024.0));
        printf("  Multi-Processor Count: %d\n", prop.multiProcessorCount);
    }
}

int hip_bot_is_available(void) {
    int deviceCount = 0;
    hipError_t error = hipGetDeviceCount(&deviceCount);
    return (error == hipSuccess && deviceCount > 0) ? 1 : 0;
}

void hip_bot_set_verbose(int level) {
    g_verbose = level;
}

GPUBenchmarkResult hip_bot_benchmark(int num_iterations, int depth) {
    GPUBenchmarkResult result;
    result.total_time_ms = 0.0f;
    result.kernel_time_ms = 0.0f;
    result.transfer_time_ms = 0.0f;
    result.simulations_per_second = 0;
    result.gpu_memory_used_mb = 0;
    
    if (!g_initialized) {
        if (hip_bot_init() != 0) {
            return result;
        }
    }
    
    // Создаем фиктивное состояние для теста
    CPUGameState test_state;
    memset(&test_state, 0, sizeof(test_state));
    test_state.snakeLength = 10;
    test_state.foodX = 20;
    test_state.foodY = 15;
    test_state.fieldMinX = 1;
    test_state.fieldMinY = 1;
    test_state.fieldMaxX = 38;
    test_state.fieldMaxY = 28;
    
    for (int i = 0; i < 10; i++) {
        test_state.snake[i].x = 10 + i;
        test_state.snake[i].y = 10;
    }
    
    printf("Running GPU benchmark: %d iterations, depth %d\n", num_iterations, depth);
    
    float total_time = 0.0f;
    
    for (int i = 0; i < num_iterations; i++) {
        BotDecision decision = hip_bot_get_decision(&test_state, depth, 0.0f);
        total_time += decision.avg_time_ms;
        
        if ((i + 1) % 10 == 0) {
            printf("  Completed %d/%d iterations\n", i + 1, num_iterations);
        }
    }
    
    result.total_time_ms = total_time;
    result.kernel_time_ms = total_time;  // Упрощенно
    result.simulations_per_second = (int)((num_iterations * 1000.0f) / total_time);
    
    // Подсчет использованной памяти
    size_t free_mem, total_mem;
    hipMemGetInfo(&free_mem, &total_mem);
    result.gpu_memory_used_mb = (int)((total_mem - free_mem) / (1024 * 1024));
    
    printf("\n=== Benchmark Results ===\n");
    printf("Total time: %.2f ms\n", result.total_time_ms);
    printf("Average time per decision: %.2f ms\n", result.total_time_ms / num_iterations);
    printf("Decisions per second: %d\n", result.simulations_per_second);
    printf("GPU memory used: %d MB\n", result.gpu_memory_used_mb);
    
    return result;
}


