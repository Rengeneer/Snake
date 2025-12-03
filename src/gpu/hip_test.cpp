#include "hip_bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ============================================
   GPU TEST & BENCHMARK UTILITY
   ============================================ */

void print_separator() {
    printf("\n================================================\n");
}

void test_gpu_availability() {
    print_separator();
    printf("TEST 1: GPU Availability\n");
    print_separator();
    
    if (hip_bot_is_available()) {
        printf("✓ GPU is available\n");
        hip_bot_print_device_info();
    } else {
        printf("✗ GPU is NOT available\n");
        printf("  Check ROCm installation and GPU drivers\n");
    }
}

void test_initialization() {
    print_separator();
    printf("TEST 2: GPU Bot Initialization\n");
    print_separator();
    
    hip_bot_set_verbose(1);
    
    int result = hip_bot_init();
    if (result == 0) {
        printf("✓ GPU Bot initialized successfully\n");
        hip_bot_cleanup();
        printf("✓ GPU Bot cleanup successful\n");
    } else {
        printf("✗ GPU Bot initialization failed\n");
    }
}

void test_simple_decision() {
    print_separator();
    printf("TEST 3: Simple Decision Making\n");
    print_separator();
    
    if (hip_bot_init() != 0) {
        printf("✗ Failed to initialize\n");
        return;
    }
    
    // Создаем простое игровое состояние
    CPUGameState state;
    state.snakeLength = 5;
    state.foodX = 20;
    state.foodY = 15;
    state.fieldMinX = 1;
    state.fieldMinY = 1;
    state.fieldMaxX = 38;
    state.fieldMaxY = 28;
    state.wallsCount = 0;
    state.score = 0;
    state.multiplierCount = 0;
    state.bonus.type = -1;
    
    // Прямая линия змейки
    for (int i = 0; i < 5; i++) {
        state.snake[i].x = 10 + i;
        state.snake[i].y = 10;
    }
    
    printf("Testing decision with depth 5...\n");
    BotDecision decision = hip_bot_get_decision(&state, 5, 0.0f);
    
    const char* dir_names[] = {"UP", "DOWN", "LEFT", "RIGHT"};
    printf("✓ Decision: %s (score: %.2f)\n", 
           dir_names[decision.direction], decision.score);
    printf("  Simulations: %d\n", decision.simulations);
    printf("  Time: %.3f ms\n", decision.avg_time_ms);
    
    hip_bot_cleanup();
}

void test_depth_scaling() {
    print_separator();
    printf("TEST 4: Depth Scaling Performance\n");
    print_separator();
    
    if (hip_bot_init() != 0) {
        printf("✗ Failed to initialize\n");
        return;
    }
    
    CPUGameState state;
    state.snakeLength = 10;
    state.foodX = 25;
    state.foodY = 20;
    state.fieldMinX = 1;
    state.fieldMinY = 1;
    state.fieldMaxX = 38;
    state.fieldMaxY = 28;
    state.wallsCount = 0;
    state.score = 0;
    state.multiplierCount = 0;
    state.bonus.type = -1;
    
    for (int i = 0; i < 10; i++) {
        state.snake[i].x = 15 + i;
        state.snake[i].y = 15;
    }
    
    printf("Depth | Simulations | Time (ms) | Sims/sec\n");
    printf("------|-------------|-----------|----------\n");
    
    for (int depth = 3; depth <= 10; depth++) {
        BotDecision decision = hip_bot_get_decision(&state, depth, 0.0f);
        int sims_per_sec = (int)((decision.simulations * 1000.0f) / decision.avg_time_ms);
        printf("  %2d  | %10d  | %9.3f | %8d\n", 
               depth, decision.simulations, decision.avg_time_ms, sims_per_sec);
    }
    
    hip_bot_cleanup();
}

void test_complex_scenario() {
    print_separator();
    printf("TEST 5: Complex Scenario (with walls)\n");
    print_separator();
    
    if (hip_bot_init() != 0) {
        printf("✗ Failed to initialize\n");
        return;
    }
    
    CPUGameState state;
    state.snakeLength = 15;
    state.foodX = 30;
    state.foodY = 20;
    state.fieldMinX = 1;
    state.fieldMinY = 1;
    state.fieldMaxX = 38;
    state.fieldMaxY = 28;
    state.score = 14;
    state.multiplierCount = 0;
    state.bonus.type = -1;
    
    // Длинная змейка
    for (int i = 0; i < 15; i++) {
        state.snake[i].x = 10 + (i % 5);
        state.snake[i].y = 10 + (i / 5);
    }
    
    // Добавляем стены (центральный квадрат)
    state.wallsCount = 0;
    for (int x = 18; x <= 22; x++) {
        state.walls[state.wallsCount][0] = x;
        state.walls[state.wallsCount][1] = 18;
        state.wallsCount++;
        state.walls[state.wallsCount][0] = x;
        state.walls[state.wallsCount][1] = 22;
        state.wallsCount++;
    }
    for (int y = 19; y <= 21; y++) {
        state.walls[state.wallsCount][0] = 18;
        state.walls[state.wallsCount][1] = y;
        state.wallsCount++;
        state.walls[state.wallsCount][0] = 22;
        state.walls[state.wallsCount][1] = y;
        state.wallsCount++;
    }
    
    printf("Testing complex scenario:\n");
    printf("  Snake length: %d\n", state.snakeLength);
    printf("  Walls: %d\n", state.wallsCount);
    printf("  Food at: (%d, %d)\n", state.foodX, state.foodY);
    
    BotDecision decision = hip_bot_get_decision(&state, 8, 5.0f);
    
    const char* dir_names[] = {"UP", "DOWN", "LEFT", "RIGHT"};
    printf("✓ Decision: %s (score: %.2f)\n", 
           dir_names[decision.direction], decision.score);
    printf("  Time: %.3f ms\n", decision.avg_time_ms);
    
    hip_bot_cleanup();
}

void run_benchmark() {
    print_separator();
    printf("TEST 6: Full Benchmark\n");
    print_separator();
    
    int iterations = 100;
    int depth = 8;
    
    printf("Running benchmark: %d iterations, depth %d\n", iterations, depth);
    printf("This may take a minute...\n\n");
    
    GPUBenchmarkResult result = hip_bot_benchmark(iterations, depth);
    
    printf("\n✓ Benchmark complete!\n");
}

void test_stress() {
    print_separator();
    printf("TEST 7: Stress Test (Rapid Decisions)\n");
    print_separator();
    
    if (hip_bot_init() != 0) {
        printf("✗ Failed to initialize\n");
        return;
    }
    
    CPUGameState state;
    state.snakeLength = 8;
    state.foodX = 20;
    state.foodY = 15;
    state.fieldMinX = 1;
    state.fieldMinY = 1;
    state.fieldMaxX = 38;
    state.fieldMaxY = 28;
    state.wallsCount = 0;
    state.score = 0;
    state.multiplierCount = 0;
    state.bonus.type = -1;
    
    for (int i = 0; i < 8; i++) {
        state.snake[i].x = 15 + i;
        state.snake[i].y = 15;
    }
    
    int num_decisions = 1000;
    printf("Making %d rapid decisions...\n", num_decisions);
    
    clock_t start = clock();
    
    for (int i = 0; i < num_decisions; i++) {
        BotDecision decision = hip_bot_get_decision(&state, 6, 0.0f);
        
        // Симулируем движение змейки
        state.snake[0].x += (decision.direction == 3 ? 1 : (decision.direction == 2 ? -1 : 0));
        state.snake[0].y += (decision.direction == 1 ? 1 : (decision.direction == 0 ? -1 : 0));
        
        if ((i + 1) % 100 == 0) {
            printf("  %d/%d decisions...\n", i + 1, num_decisions);
        }
    }
    
    clock_t end = clock();
    double total_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double avg_time = total_time / num_decisions;
    
    printf("✓ Stress test complete!\n");
    printf("  Total time: %.2f seconds\n", total_time);
    printf("  Average time per decision: %.3f ms\n", avg_time * 1000);
    printf("  Decisions per second: %.0f\n", 1.0 / avg_time);
    
    hip_bot_cleanup();
}

int main(int argc, char* argv[]) {
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║   Snake GPU Bot - Test & Benchmark Suite     ║\n");
    printf("║   AMD HIP Implementation                      ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    int run_all = 1;
    int test_num = 0;
    
    if (argc > 1) {
        test_num = atoi(argv[1]);
        if (test_num >= 1 && test_num <= 7) {
            run_all = 0;
        }
    }
    
    if (run_all || test_num == 1) test_gpu_availability();
    if (run_all || test_num == 2) test_initialization();
    if (run_all || test_num == 3) test_simple_decision();
    if (run_all || test_num == 4) test_depth_scaling();
    if (run_all || test_num == 5) test_complex_scenario();
    if (run_all || test_num == 6) run_benchmark();
    if (run_all || test_num == 7) test_stress();
    
    print_separator();
    printf("All tests completed!\n");
    print_separator();
    
    printf("\nUsage: %s [test_number]\n", argv[0]);
    printf("  Run specific test (1-7) or all tests (no argument)\n\n");
    
    return 0;
}


