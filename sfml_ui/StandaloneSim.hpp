#pragma once

#include "../shared.hpp"
#include <pthread.h>
#include <cstdlib>
#include <ctime>

namespace ChronoRift {

class StandaloneSimulation {
public:
    StandaloneSimulation();
    ~StandaloneSimulation();

    void init(SharedState* state);
    void start();
    void stop();

    bool isRunning() {
        pthread_mutex_lock(&running_mtx);
        bool r = running;
        pthread_mutex_unlock(&running_mtx);
        return r;
    }

private:
    SharedState* state;
    bool running;
    pthread_mutex_t running_mtx;
    pthread_t simThread;
    bool threadStarted;

    static void* threadEntry(void* arg);
    void simulationLoop();
    void simulateTurn();
    void simulateNPCAction(int npcId);
    void simulatePlayerAction(int playerId);
    void updateStamina();
    void checkWinConditions();
    void spawnArtifacts();
};

} // namespace ChronoRift
