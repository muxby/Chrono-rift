// main.cpp - Passive SFML visualizer that attaches to an existing arbiter's shared memory.
// Just displays the game state, doesn't control anything.

#include "Visualizer.hpp"
#include "../shared.hpp"
#include "../common.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <csignal>
#include <unistd.h>

using namespace std;
using namespace ChronoRift;

static SharedState* g_state = nullptr;
static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    cout << "=== CHRONO RIFT - SFML Visualizer ===\n";
    cout << "Controls: [1]Combat [2]Scheduler [3]Hybrid [ESC]Exit\n\n";

    // Try to attach to existing shared memory (created by arbiter)
    SharedState* state = map_shared(false);
    if (!state) {
        cerr << "Failed to attach to shared memory." << endl;
        cerr << "Make sure the arbiter is running first!" << endl;
        cerr << "Retrying in 2 seconds..." << endl;
        sleep(2);

        state = map_shared(false);
        if (!state) {
            cerr << "Still cannot attach. Exiting." << endl;
            return 1;
        }
    }

    g_state = state;
    cout << "[OK] Attached to shared memory: " << SHM_NAME << endl;

    // Wait for initialization
    cout << "Waiting for arbiter to initialize..." << endl;
    int attempts = 0;
    while (!state->initialized && attempts < 10) {
        sleep(1);
        attempts++;
    }

    if (!state->initialized) {
        cerr << "Timeout waiting for arbiter initialization." << endl;
        cleanup_shared(state, false);
        return 1;
    }

    cout << "[OK] Arbiter initialized. Starting visualizer..." << endl;
    cout << "Players: " << state->num_players << ", NPCs: " << state->num_npcs << endl;

    // Create and run the visualizer
    Visualizer visualizer;
    if (!visualizer.initialize()) {
        cerr << "Failed to initialize visualizer!" << endl;
        cleanup_shared(state, false);
        return 1;
    }

    cout << "[OK] Visualizer initialized. Opening window..." << endl;
    visualizer.run(state);

    // Cleanup
    visualizer.shutdown();
    cleanup_shared(state, false);

    cout << "Visualizer exited cleanly." << endl;
    return 0;
}
