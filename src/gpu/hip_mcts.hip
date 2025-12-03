#include "hip_types.h"
#include <hip/hip_runtime.h>

/* ============================================
   MONTE CARLO TREE SEARCH (MCTS) for GPU
   ============================================ */

#define MCTS_MAX_ROLLOUT_DEPTH 50
#define MCTS_EXPLORATION_CONSTANT 1.414f  // sqrt(2)

// Узел дерева MCTS
typedef struct {
    int visits;
    float total_score;
    int parent;
    int children[4];  // 4 возможных действия
    int state_id;
    int action;       // Действие, которое привело к этому узлу
    bool expanded;
} MCTSNode;

/* ============================================
   DEVICE HELPER FUNCTIONS
   ============================================ */

// UCB1 формула для выбора узла
__device__ float calculate_ucb1(const MCTSNode* node, const MCTSNode* parent) {
    if (node->visits == 0) {
        return INFINITY;
    }
    
    float exploitation = node->total_score / node->visits;
    float exploration = MCTS_EXPLORATION_CONSTANT * 
                       sqrtf(logf((float)parent->visits) / node->visits);
    
    return exploitation + exploration;
}

// Случайный rollout до конца игры или максимальной глубины
__device__ float perform_rollout(GPUGameState* state, int thread_seed) {
    float score = 0.0f;
    int depth = 0;
    
    int dx[] = {0, 1, 0, -1};
    int dy[] = {-1, 0, 1, 0};
    
    while (depth < MCTS_MAX_ROLLOUT_DEPTH) {
        // Псевдо-случайный выбор действия
        int action = (thread_seed * 1103515245 + 12345 + depth) % 4;
        thread_seed = action;
        
        int headX = state->snake[0].x;
        int headY = state->snake[0].y;
        int newX = headX + dx[action];
        int newY = headY + dy[action];
        
        // Проверяем валидность хода
        bool valid = true;
        if (!gpu_in_bounds(newX, newY, state)) {
            valid = false;
        }
        
        // Проверка стен
        if (valid) {
            for (int i = 0; i < state->wallsCount; i++) {
                if (newX == state->walls[i][0] && newY == state->walls[i][1]) {
                    valid = false;
                    break;
                }
            }
        }
        
        // Проверка тела змейки
        if (valid) {
            for (int i = 0; i < state->snakeLength; i++) {
                if (newX == state->snake[i].x && newY == state->snake[i].y) {
                    valid = false;
                    break;
                }
            }
        }
        
        if (!valid) {
            score -= 100.0f;  // Штраф за невалидный ход
            break;
        }
        
        // Выполняем ход
        for (int i = state->snakeLength - 1; i > 0; i--) {
            state->snake[i] = state->snake[i - 1];
        }
        state->snake[0].x = newX;
        state->snake[0].y = newY;
        
        // Награды
        if (newX == state->foodX && newY == state->foodY) {
            score += 50.0f;
            
            // Упрощенная генерация новой еды
            state->foodX = state->fieldMinX + (newX * 17 + newY * 31 + depth) % 
                          (state->fieldMaxX - state->fieldMinX + 1);
            state->foodY = state->fieldMinY + (newY * 13 + newX * 29 + depth) % 
                          (state->fieldMaxY - state->fieldMinY + 1);
            
            if (state->snakeLength < GPU_MAX_SNAKE_LENGTH) {
                state->snakeLength++;
            }
        }
        
        // Штраф за расстояние до еды
        int dist = gpu_manhattan_distance(newX, newY, state->foodX, state->foodY);
        score -= dist * 0.1f;
        
        // Небольшая награда за выживание
        score += 1.0f;
        
        depth++;
    }
    
    return score;
}

/* ============================================
   KERNEL: Параллельный MCTS
   ============================================ */

__global__ void parallel_mcts_kernel(
    const GPUGameState* base_state,
    int* best_actions,
    float* best_scores,
    int num_threads,
    int iterations_per_thread
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_threads) return;
    
    // Каждый thread выполняет MCTS независимо
    __shared__ MCTSNode tree[256];  // Локальное дерево для блока
    
    // Инициализация корневого узла
    if (threadIdx.x == 0) {
        tree[0].visits = 0;
        tree[0].total_score = 0.0f;
        tree[0].parent = -1;
        tree[0].expanded = false;
        tree[0].action = -1;
        
        for (int i = 0; i < 4; i++) {
            tree[0].children[i] = -1;
        }
    }
    __syncthreads();
    
    int next_node_id = 1;
    
    // MCTS iterations
    for (int iter = 0; iter < iterations_per_thread; iter++) {
        // 1. Selection - выбираем лучший узел по UCB1
        int current_node = 0;
        GPUGameState sim_state;
        copy_game_state(&sim_state, base_state);
        
        while (tree[current_node].expanded && next_node_id < 256) {
            // Находим лучшего потомка по UCB1
            int best_child = -1;
            float best_ucb = -INFINITY;
            
            for (int i = 0; i < 4; i++) {
                int child_id = tree[current_node].children[i];
                if (child_id != -1 && child_id < next_node_id) {
                    float ucb = calculate_ucb1(&tree[child_id], &tree[current_node]);
                    if (ucb > best_ucb) {
                        best_ucb = ucb;
                        best_child = child_id;
                    }
                }
            }
            
            if (best_child == -1) break;
            
            current_node = best_child;
        }
        
        // 2. Expansion - расширяем узел
        if (!tree[current_node].expanded && next_node_id < 250) {
            tree[current_node].expanded = true;
            
            // Создаем дочерние узлы для всех действий
            for (int action = 0; action < 4; action++) {
                if (next_node_id >= 256) break;
                
                int child_id = atomicAdd(&next_node_id, 1);
                if (child_id < 256) {
                    tree[child_id].visits = 0;
                    tree[child_id].total_score = 0.0f;
                    tree[child_id].parent = current_node;
                    tree[child_id].action = action;
                    tree[child_id].expanded = false;
                    
                    for (int i = 0; i < 4; i++) {
                        tree[child_id].children[i] = -1;
                    }
                    
                    tree[current_node].children[action] = child_id;
                }
            }
        }
        
        // 3. Simulation (rollout)
        int seed = idx * 1000 + iter;
        float score = perform_rollout(&sim_state, seed);
        
        // 4. Backpropagation
        int node = current_node;
        while (node != -1) {
            atomicAdd(&tree[node].visits, 1);
            atomicAdd(&tree[node].total_score, score);
            node = tree[node].parent;
        }
    }
    
    __syncthreads();
    
    // Выбираем лучшее действие из корня
    if (threadIdx.x == 0) {
        int best_action = 0;
        float best_avg_score = -INFINITY;
        
        for (int i = 0; i < 4; i++) {
            int child_id = tree[0].children[i];
            if (child_id != -1 && child_id < next_node_id && tree[child_id].visits > 0) {
                float avg_score = tree[child_id].total_score / tree[child_id].visits;
                if (avg_score > best_avg_score) {
                    best_avg_score = avg_score;
                    best_action = i;
                }
            }
        }
        
        best_actions[blockIdx.x] = best_action;
        best_scores[blockIdx.x] = best_avg_score;
    }
}

/* ============================================
   KERNEL: Гибридный MCTS + Deep Simulation
   ============================================ */

__global__ void hybrid_mcts_simulation_kernel(
    const GPUGameState* base_state,
    GPUSimulationResult* results,
    int num_simulations,
    int mcts_iterations
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_simulations) return;
    
    GPUGameState local_state;
    copy_game_state(&local_state, base_state);
    
    // Декодируем первое действие
    int first_action = idx % 4;
    
    // Выполняем первое действие
    int dx[] = {0, 1, 0, -1};
    int dy[] = {-1, 0, 1, 0};
    
    int newX = local_state.snake[0].x + dx[first_action];
    int newY = local_state.snake[0].y + dy[first_action];
    
    // Проверка валидности
    bool valid = gpu_in_bounds(newX, newY, &local_state);
    
    if (valid) {
        // Проверка стен и тела
        for (int i = 0; i < local_state.wallsCount && valid; i++) {
            if (newX == local_state.walls[i][0] && newY == local_state.walls[i][1]) {
                valid = false;
            }
        }
        
        for (int i = 0; i < local_state.snakeLength && valid; i++) {
            if (newX == local_state.snake[i].x && newY == local_state.snake[i].y) {
                valid = false;
            }
        }
    }
    
    if (!valid) {
        results[idx].score = -10000.0f;
        results[idx].direction = first_action;
        results[idx].valid = 0;
        return;
    }
    
    // Выполняем ход
    for (int i = local_state.snakeLength - 1; i > 0; i--) {
        local_state.snake[i] = local_state.snake[i - 1];
    }
    local_state.snake[0].x = newX;
    local_state.snake[0].y = newY;
    
    // Теперь выполняем MCTS rollout
    int seed = idx * 12345 + threadIdx.x;
    float score = perform_rollout(&local_state, seed);
    
    results[idx].score = score;
    results[idx].direction = first_action;
    results[idx].valid = 1;
}


