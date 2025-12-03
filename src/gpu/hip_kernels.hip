#include "hip_types.h"
#include <hip/hip_runtime.h>

/* ============================================
   DEVICE HELPER FUNCTIONS
   ============================================ */

// Проверка валидности позиции в состоянии игры
__device__ bool is_position_valid_gpu(int x, int y, const GPUGameState* state) {
    // Проверка границ
    if (!gpu_in_bounds(x, y, state)) {
        return false;
    }
    
    // Проверка стен
    for (int i = 0; i < state->wallsCount; i++) {
        if (x == state->walls[i][0] && y == state->walls[i][1]) {
            return false;
        }
    }
    
    // Проверка тела змейки
    for (int i = 0; i < state->snakeLength; i++) {
        if (x == state->snake[i].x && y == state->snake[i].y) {
            return false;
        }
    }
    
    return true;
}

// Выполнить один ход в симуляции
__device__ bool make_move_gpu(GPUGameState* state, int direction) {
    if (state->snakeLength <= 0) return false;
    
    GPUSegment newHead = state->snake[0];
    
    switch (direction) {
        case GPU_UP:    newHead.y--; break;
        case GPU_DOWN:  newHead.y++; break;
        case GPU_LEFT:  newHead.x--; break;
        case GPU_RIGHT: newHead.x++; break;
        case GPU_NONE:  return false;
    }
    
    // Проверка валидности новой позиции
    if (!is_position_valid_gpu(newHead.x, newHead.y, state)) {
        return false;
    }
    
    // Двигаем змейку
    for (int i = state->snakeLength - 1; i > 0; i--) {
        state->snake[i] = state->snake[i - 1];
    }
    state->snake[0] = newHead;
    
    // Проверка на еду
    if (newHead.x == state->foodX && newHead.y == state->foodY) {
        if (state->snakeLength < GPU_MAX_SNAKE_LENGTH) {
            state->snakeLength++;
            state->snake[state->snakeLength - 1] = state->snake[state->snakeLength - 2];
        }
        
        // Генерация новой еды (упрощенная версия для GPU)
        // В реальности будет использоваться seed для каждого thread
        int attempts = 0;
        do {
            state->foodX = state->fieldMinX + (newHead.x * 17 + newHead.y * 31 + attempts) % 
                          (state->fieldMaxX - state->fieldMinX + 1);
            state->foodY = state->fieldMinY + (newHead.y * 13 + newHead.x * 29 + attempts) % 
                          (state->fieldMaxY - state->fieldMinY + 1);
            attempts++;
        } while (!is_position_valid_gpu(state->foodX, state->foodY, state) && attempts < 100);
    }
    
    // Проверка на бонус
    if (state->bonus.type != -1 && newHead.x == state->bonus.x && newHead.y == state->bonus.y) {
        state->bonus.type = -1;
    }
    
    return true;
}

/* ============================================
   KERNEL 1: PARALLEL BFS для подсчета свободного пространства
   ============================================ */

__global__ void parallel_bfs_kernel(
    const GPUGameState* states,
    int* free_space_counts,
    int num_states
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_states) return;
    
    const GPUGameState* state = &states[idx];
    
    // Используем shared memory для очереди BFS
    __shared__ int queue_x[256];
    __shared__ int queue_y[256];
    __shared__ unsigned char visited[GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1];
    
    // Инициализация visited
    if (threadIdx.x < (GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1)) {
        visited[threadIdx.x] = 0;
    }
    __syncthreads();
    
    int front = 0, rear = 0;
    int count = 0;
    
    int startX = state->snake[0].x;
    int startY = state->snake[0].y;
    
    queue_x[rear] = startX;
    queue_y[rear] = startY;
    rear++;
    
    // Отметить начальную позицию как посещенную
    int start_idx = startY * GPU_FIELD_WIDTH + startX;
    atomicOr((unsigned int*)&visited[start_idx / 8], 1 << (start_idx % 8));
    count++;
    
    int dx[] = {0, 1, 0, -1};
    int dy[] = {-1, 0, 1, 0};
    
    while (front < rear && rear < 256) {
        int x = queue_x[front];
        int y = queue_y[front];
        front++;
        
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (!gpu_in_bounds(nx, ny, state)) continue;
            
            int cell_idx = ny * GPU_FIELD_WIDTH + nx;
            unsigned char mask = 1 << (cell_idx % 8);
            
            if (visited[cell_idx / 8] & mask) continue;
            
            if (is_position_valid_gpu(nx, ny, state)) {
                atomicOr((unsigned int*)&visited[cell_idx / 8], mask);
                if (rear < 256) {
                    queue_x[rear] = nx;
                    queue_y[rear] = ny;
                    rear++;
                    count++;
                }
            }
        }
    }
    
    free_space_counts[idx] = count;
}

/* ============================================
   KERNEL 2: PARALLEL A* для поиска путей
   ============================================ */

__global__ void parallel_astar_kernel(
    const GPUGameState* states,
    const int* start_x,
    const int* start_y,
    const int* target_x,
    const int* target_y,
    int* distances,
    int num_paths
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_paths) return;
    
    const GPUGameState* state = &states[idx];
    int sx = start_x[idx];
    int sy = start_y[idx];
    int tx = target_x[idx];
    int ty = target_y[idx];
    
    // Проверка валидности цели
    if (!is_position_valid_gpu(tx, ty, state)) {
        distances[idx] = 10000;
        return;
    }
    
    // Используем локальные массивы для A*
    __shared__ GPUPathNode nodes[GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT];
    __shared__ unsigned char closed_list[GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1];
    __shared__ unsigned char open_list[GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1];
    
    // Инициализация (только один thread в блоке делает это)
    if (threadIdx.x == 0) {
        for (int i = 0; i < GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT; i++) {
            nodes[i].f = INT_MAX;
            nodes[i].g = INT_MAX;
            nodes[i].h = 0;
            nodes[i].parentX = -1;
            nodes[i].parentY = -1;
        }
        
        for (int i = 0; i < GPU_FIELD_WIDTH * GPU_FIELD_HEIGHT / 8 + 1; i++) {
            closed_list[i] = 0;
            open_list[i] = 0;
        }
    }
    __syncthreads();
    
    int start_idx = sy * GPU_FIELD_WIDTH + sx;
    nodes[start_idx].x = sx;
    nodes[start_idx].y = sy;
    nodes[start_idx].g = 0;
    nodes[start_idx].h = gpu_manhattan_distance(sx, sy, tx, ty);
    nodes[start_idx].f = nodes[start_idx].h;
    open_list[start_idx / 8] |= (1 << (start_idx % 8));
    
    bool found = false;
    int iterations = 0;
    const int MAX_ITERATIONS = 500;
    
    while (!found && iterations < MAX_ITERATIONS) {
        iterations++;
        
        // Найти узел с минимальным f в open list
        int minF = INT_MAX;
        int currentX = -1, currentY = -1;
        int current_idx = -1;
        
        for (int y = state->fieldMinY; y <= state->fieldMaxY; y++) {
            for (int x = state->fieldMinX; x <= state->fieldMaxX; x++) {
                int idx_node = y * GPU_FIELD_WIDTH + x;
                if ((open_list[idx_node / 8] & (1 << (idx_node % 8))) && nodes[idx_node].f < minF) {
                    minF = nodes[idx_node].f;
                    currentX = x;
                    currentY = y;
                    current_idx = idx_node;
                }
            }
        }
        
        if (current_idx == -1) break;
        
        // Переместить из open в closed
        open_list[current_idx / 8] &= ~(1 << (current_idx % 8));
        closed_list[current_idx / 8] |= (1 << (current_idx % 8));
        
        // Проверка достижения цели
        if (currentX == tx && currentY == ty) {
            found = true;
            distances[idx] = nodes[current_idx].g;
            break;
        }
        
        // Проверка соседей
        int dx[] = {0, 1, 0, -1};
        int dy[] = {-1, 0, 1, 0};
        
        for (int i = 0; i < 4; i++) {
            int nx = currentX + dx[i];
            int ny = currentY + dy[i];
            
            if (!gpu_in_bounds(nx, ny, state)) continue;
            if (!is_position_valid_gpu(nx, ny, state)) continue;
            
            int neighbor_idx = ny * GPU_FIELD_WIDTH + nx;
            
            if (closed_list[neighbor_idx / 8] & (1 << (neighbor_idx % 8))) continue;
            
            int tentative_g = nodes[current_idx].g + 1;
            
            bool is_in_open = open_list[neighbor_idx / 8] & (1 << (neighbor_idx % 8));
            
            if (!is_in_open || tentative_g < nodes[neighbor_idx].g) {
                nodes[neighbor_idx].parentX = currentX;
                nodes[neighbor_idx].parentY = currentY;
                nodes[neighbor_idx].g = tentative_g;
                nodes[neighbor_idx].h = gpu_manhattan_distance(nx, ny, tx, ty);
                nodes[neighbor_idx].f = nodes[neighbor_idx].g + nodes[neighbor_idx].h;
                
                if (!is_in_open) {
                    open_list[neighbor_idx / 8] |= (1 << (neighbor_idx % 8));
                }
            }
        }
    }
    
    if (!found) {
        distances[idx] = 10000;
    }
}

/* ============================================
   KERNEL 3: Оценка состояний игры
   ============================================ */

__global__ void evaluate_states_kernel(
    const GPUGameState* states,
    const int* distances_to_food,
    const int* distances_to_bonus,
    const int* free_spaces,
    float* scores,
    float hunger_penalty,
    int num_states
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_states) return;
    
    const GPUGameState* state = &states[idx];
    
    if (state->snakeLength <= 0) {
        scores[idx] = -10000.0f;
        return;
    }
    
    float score = 0.0f;
    int headX = state->snake[0].x;
    int headY = state->snake[0].y;
    
    int foodDist = distances_to_food[idx];
    int bonusDist = (state->bonus.type != -1) ? distances_to_bonus[idx] : 1000;
    
    // Награда за достижение еды
    if (headX == state->foodX && headY == state->foodY) {
        score += 300.0f;
    }
    
    // Штраф за расстояние до еды
    score -= foodDist * 2.0f;
    
    // Обработка бонусов
    if (state->bonus.type != -1) {
        if (headX == state->bonus.x && headY == state->bonus.y) {
            score += 1000.0f;
        }
        score -= bonusDist * 1.5f;
        
        if (bonusDist < 5) {
            score += (5 - bonusDist) * 10.0f;
        }
    }
    
    // Подсчет безопасных ходов
    int safeMoves = 0;
    int dx[] = {0, 1, 0, -1};
    int dy[] = {-1, 0, 1, 0};
    
    for (int i = 0; i < 4; i++) {
        int nx = headX + dx[i];
        int ny = headY + dy[i];
        if (is_position_valid_gpu(nx, ny, state)) {
            safeMoves++;
        }
    }
    
    // Штрафы и бонусы за безопасность
    float safetyMultiplier = 1.0f;
    if (safeMoves == 0) {
        score -= 1000.0f;
        safetyMultiplier = 0.1f;
    } else if (safeMoves == 1) {
        score -= 30.0f;
        safetyMultiplier = 0.5f;
    } else if (safeMoves == 2) {
        score -= 5.0f;
        safetyMultiplier = 0.8f;
    } else {
        score += safeMoves * 2.0f;
        safetyMultiplier = 1.0f;
    }
    
    // Штраф за голод
    float adjustedHungerPenalty = hunger_penalty * safetyMultiplier;
    score -= adjustedHungerPenalty;
    
    // Бонус за свободное пространство
    int freeSpace = free_spaces[idx];
    score += freeSpace * 0.1f;
    
    // Бонус за длину змейки
    score += state->snakeLength * 1.0f;
    
    scores[idx] = score;
}

/* ============================================
   KERNEL 4: Массовая симуляция игровых состояний
   ============================================ */

__global__ void massive_simulation_kernel(
    const GPUGameState* base_state,
    GPUSimulationResult* results,
    int depth,
    int num_simulations
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_simulations) return;
    
    // Каждый thread симулирует одну последовательность ходов
    GPUGameState local_state;
    copy_game_state(&local_state, base_state);
    
    // Декодируем sequence of moves из idx
    // Для depth=5: 4^5 = 1024 возможных последовательности
    int move_sequence[GPU_MAX_DEPTH];
    int temp_idx = idx;
    
    for (int d = 0; d < depth; d++) {
        move_sequence[d] = temp_idx % 4;
        temp_idx /= 4;
    }
    
    // Выполняем симуляцию
    float total_score = 0.0f;
    bool valid = true;
    
    for (int d = 0; d < depth && valid; d++) {
        int direction = move_sequence[d];
        
        if (!make_move_gpu(&local_state, direction)) {
            valid = false;
            total_score = -10000.0f;
            break;
        }
        
        // Награда за достижение еды
        if (local_state.snake[0].x == local_state.foodX &&
            local_state.snake[0].y == local_state.foodY) {
            total_score += 500.0f;
        }
        
        // Награда за бонус
        if (local_state.bonus.type != -1 &&
            local_state.snake[0].x == local_state.bonus.x &&
            local_state.snake[0].y == local_state.bonus.y) {
            total_score += 1500.0f;
        }
    }
    
    // Записываем результат
    results[idx].score = total_score;
    results[idx].direction = move_sequence[0];  // Первый ход в последовательности
    results[idx].valid = valid ? 1 : 0;
}


