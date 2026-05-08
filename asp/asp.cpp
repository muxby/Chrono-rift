/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  ASP — Automated Strategic Process (AI Opponent)                        ║
 * ║  OS Concepts: Threading, Shared Memory IPC, Signal Handling           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * ASP is the AI opponent manager. Each NPC gets one thread that:
 *   1. Waits on turn_cv until its turn is assigned
 *   2. Checks stun state (via SIGUSR1 signal handler or shared state)
 *   3. If not stunned, submits a random action (80% strike, 20% skip)
 *   4. Signs on action_cv to wake the arbiter
 *
 * OS Concepts Demonstrated:
 *   1. SHARED MEMORY IPC: Attaches to same SharedState region as arbiter/hip
 *   2. THREADING: One pthread per NPC, all coordinating via shared mutex/condvar
 *   3. SIGNAL HANDLING: SIGUSR1 for stun, SIGCONT for resume (from Ultimate)
 *   4. SYNCHRONIZATION: pthread_mutex + pthread_cond for turn coordination
 *
 * The ASP threads do NOT perform I/O (no stdin), so they don't need the
 * unlock-then-submit pattern that HIP uses. They simply build a PendingAction
 * and submit it atomically under the mutex.
 */

#include "../common.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// STUN SIGNAL HANDLER — Async signal delivery from arbiter on successful hit

static SharedState* g = nullptr;
static std::atomic<int> stunned_until{0};

void stun_handler(int) {
    // Async-signal-safe: atomic store of stun end time.
    // The NPC thread reads this to skip its turn for 3 seconds.
    stunned_until.store(now_epoch() + 3);
}

// ═══════════════════════════════════════════════════════════════════════════
// NPC ACTION SUBMISSION — Builds and submits a random AI action

// Submits an NPC action to shared memory.
// 80% chance: ACT_STRIKE (attack first living player)
// 20% chance: ACT_SKIP (do nothing)
//
// OS Concepts: Shared memory write under mutex, condition variable signal.
void submit_npc_action(int nid) {
    PendingAction a{};
    a.actor_team = TEAM_NPC;
    a.actor_id = nid;

    int target = first_living_player(g);
    if (target < 0) {
        a.action = ACT_SKIP;
        a.target_team = TEAM_PLAYER;
        a.target_id = 0;
    } else {
        a.action = (std::rand() % 100 < 80) ? ACT_STRIKE : ACT_SKIP;
        a.target_team = TEAM_PLAYER;
        a.target_id = target;
    }

    g->pending = a;
    g->pending.ready = 1;
    pthread_cond_signal(&g->action_cv);
}

// ═══════════════════════════════════════════════════════════════════════════
// NPC THREAD — Per-NPC thread waiting for turn assignment

void* npc_thread(void* arg) {
    int nid = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    // Wait for arbiter to initialize shared state
    pthread_mutex_lock(&g->mtx);
    while (!g->initialized) pthread_cond_wait(&g->turn_cv, &g->mtx);

    while (g->running) {
        // Wait until THIS NPC's turn (active_team==TEAM_NPC && active_id==nid)
        while (g->running && !(g->active_team == TEAM_NPC && g->active_id == nid)) {
            pthread_cond_wait(&g->turn_cv, &g->mtx);
        }
        if (!g->running) break;

        // Skip if dead
        if (!g->npcs[nid].alive) continue;

        // Check stun: either from signal (stunned_until atomic) or in-game stun
        int now = now_epoch();
        if (now < stunned_until.load() || now < g->npcs[nid].stunned_until_epoch) {
            PendingAction s{};
            s.action = ACT_SKIP;
            s.actor_team = TEAM_NPC;
            s.actor_id = nid;
            g->pending = s;
            g->pending.ready = 1;
            pthread_cond_signal(&g->action_cv);
            continue;
        }

        // Not stunned — submit AI action
        submit_npc_action(nid);
    }

    pthread_mutex_unlock(&g->mtx);
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN — Attach to shared memory, spawn NPC threads

int main() {
    // Register SIGUSR1 for stun (sent by arbiter on successful hit)
    signal(SIGUSR1, stun_handler);

    // Shared Memory IPC: Attach to existing shared memory region
    g = map_shared(false);
    if (!g) return 1;

    // Let arbiter know our PID (for SIGUSR1 delivery and Ultimate pause)
    g->asp_pid = getpid();

    // Threading: Spawn one thread per NPC (OS Concept: pthread_create)
    std::vector<pthread_t> threads(g->num_npcs);
    for (int i = 0; i < g->num_npcs; ++i) {
        int* id = new int(i);
        pthread_create(&threads[i], nullptr, npc_thread, id);
    }

    // Wait for all NPC threads to finish
    for (auto& t : threads) pthread_join(t, nullptr);

    // Cleanup: Unmap shared memory (OS Concept: munmap)
    munmap(g, sizeof(SharedState));
    return 0;
}