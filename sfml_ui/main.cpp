/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  CHRONO RIFT — SFML Process Visualizer                                  ║
 * ║  High-Performance 2D Visualization for OS Process Scheduling            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * This SFML application connects to the existing Chrono Rift shared memory
 * and provides a real-time visualization of:
 *   • Arbiter scheduling decisions
 *   • ASP (NPC) process states with color-coded blocks
 *   • HIP (Player) resource allocation
 *   • Live Gantt chart showing scheduling timeline
 *   • System metrics dashboard (AWT, Turnaround, Throughput)
 *   • Interactive controls for quantum time and priority
 *
 * Controls:
 *   [1] Combat View      [2] Scheduler View      [3] Hybrid View
 *   [Space] Spawn new process     [ESC] Exit
 *   Click anywhere for particle effects
 */

#include "Visualizer.hpp"
#include "../shared.hpp"
#include "../common.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <csignal>
#include <unistd.h>

static SharedState* g_state = nullptr;
static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CHRONO RIFT — SFML Process Visualizer                        ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Controls:                                                    ║\n";
    std::cout << "║    [1] Combat View    [2] Scheduler View    [3] Hybrid View   ║\n";
    std::cout << "║    [Space] Spawn Process    [ESC] Exit                        ║\n";
    std::cout << "║    Click for particle effects                                 ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    // Try to attach to existing shared memory (created by arbiter)
    SharedState* state = map_shared(false);
    if (!state) {
        std::cerr << "Failed to attach to shared memory." << std::endl;
        std::cerr << "Make sure the arbiter is running first!" << std::endl;
        std::cerr << "Retrying in 2 seconds..." << std::endl;
        sleep(2);

        state = map_shared(false);
        if (!state) {
            std::cerr << "Still cannot attach. Exiting." << std::endl;
            return 1;
        }
    }

    g_state = state;
    std::cout << "[OK] Attached to shared memory: " << SHM_NAME << std::endl;

    // Wait for initialization
    std::cout << "Waiting for arbiter to initialize..." << std::endl;
    int attempts = 0;
    while (!state->initialized && attempts < 50) {
        usleep(100000); // 100ms
        attempts++;
    }

    if (!state->initialized) {
        std::cerr << "Timeout waiting for arbiter initialization." << std::endl;
        munmap(state, sizeof(SharedState));
        return 1;
    }

    std::cout << "[OK] Arbiter initialized. Starting visualizer..." << std::endl;
    std::cout << "Players: " << state->num_players << ", NPCs: " << state->num_npcs << std::endl;

    // Create and run the visualizer
    ChronoRift::Visualizer visualizer;
    if (!visualizer.initialize()) {
        std::cerr << "Failed to initialize visualizer!" << std::endl;
        munmap(state, sizeof(SharedState));
        return 1;
    }

    std::cout << "[OK] Visualizer initialized. Opening window..." << std::endl;
    visualizer.run(state);

    // Cleanup
    visualizer.shutdown();
    munmap(state, sizeof(SharedState));

    std::cout << "Visualizer exited cleanly." << std::endl;
    return 0;
}
