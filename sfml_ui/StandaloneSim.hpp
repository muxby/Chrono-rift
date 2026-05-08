#pragma once

#include "../shared.hpp"
#include <random>
#include <thread>
#include <atomic>

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
// STANDALONE SIMULATION — Generates mock data for UI testing without
// requiring the original arbiter/hip/asp processes.
// ═══════════════════════════════════════════════════════════════════════════

class StandaloneSimulation {
public:
    StandaloneSimulation();
    ~StandaloneSimulation();

    // Initialize mock shared state
    void init(SharedState* state);

    // Start the simulation thread
    void start();

    // Stop the simulation
    void stop();

    // Check if running
    bool isRunning() const { return running.load(); }

private:
    SharedState* state;
    std::atomic<bool> running;
    std::thread simThread;
    std::mt19937 rng;

    void simulationLoop();
    void simulateTurn();
    void simulateNPCAction(int npcId);
    void simulatePlayerAction(int playerId);
    void updateStamina();
    void checkWinConditions();
    void spawnArtifacts();
};

} // namespace ChronoRift
