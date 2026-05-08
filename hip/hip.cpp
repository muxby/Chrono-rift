/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  HIP — Human Interfacing Process                                         ║
 * ║  OS Concepts: Process Management, Threading, Shared Memory IPC           ║
 * ║  Signal Handling, Condition Variables                                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * HIP represents the human player(s) in the Chrono Rift game.
 *
 * OS Concepts Demonstrated:
 *   1. PROCESS MANAGEMENT: Each player runs in a dedicated thread.
 *      The arbiter assigns turns via shared memory + condition variables.
 *   2. SHARED MEMORY IPC: Players attach to the same SharedState region
 *      mapped via shm_open/mmap (see common.hpp: map_shared).
 *   3. THREADING: One pthread per player, all reading from stdin in parallel.
 *      A dedicated stun_handler processes SIGUSR1 signals.
 *   4. SYNCHRONIZATION: pthread_mutex + pthread_cond for turn coordination.
 *      Players wait on turn_cv until their turn is assigned (active_id match).
 *   5. SIGNAL-BASED STUN: SIGUSR1 delivered asynchronously to stun_handler.
 *      Sets the stunned_until atomic so the player thread skips its turn.
 *
 * IPC Pattern:
 *   Arbiter (writer) --> SharedState --> HIP (reader) --> action_cv signal
 *
 * Thread Safety:
 *   - All shared state access is protected by g->mtx
 *   - Stun state uses an atomic<int> for signal handler safety
 *   - std::cin read is performed outside the mutex to avoid deadlock
 */

#include "../common.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// STUN SIGNAL HANDLER — Asynchronous interrupt delivery (OS Concept: Signals)
//
// SIGUSR1 is sent by the arbiter when an attack applies a stun effect.
// This handler runs in the signal context — it must be async-signal-safe.
// We atomically store the stun end time, which player threads read to skip turns.
//
// OS Concepts: Asynchronous signal delivery, signal handlers, atomic operations.
// The use of std::atomic ensures the signal handler can safely communicate
// with the player thread without data races (C++11 memory model guarantees).

static SharedState* g = nullptr;
static std::atomic<int> stunned_until{0};

void stun_handler(int) {
    stunned_until.store(now_epoch() + 3);
}

// ═══════════════════════════════════════════════════════════════════════════
// ACTION INPUT — Reads player choice from stdin, submits atomically to shared state

// Reads the player's action choice from stdin.
// This read is performed OUTSIDE the mutex to avoid blocking other threads.
// If stdin fails (e.g., EOF from Docker pipe), we clear the error and retry
// to prevent an infinite loop of prompts flooding the terminal.
int read_action_choice() {
    int ch = 6; // default to Skip if input fails
    while (true) {
        std::cout << "Choose action: 1)Strike 2)Exhaust 3)Weapon 4)SwapIn "
                  << "5)Heal 6)Skip 7)Ultimate 8)Quit: " << std::flush;
        if (std::cin >> ch) {
            break; // valid input received
        }
        // stdin failed (EOF, bad state, non-integer input)
        if (std::cin.eof()) {
            // No more input available — default to Skip
            return 6;
        }
        // Clear error state and discard bad input, then retry
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cerr << "[HIP] Invalid input, please enter a number 1-8.\n";
    }
    return ch;
}

// Builds a PendingAction from the player's choice.
// This is a pure function with no shared state access.
PendingAction build_action(int pid, int choice, int npc) {
    PendingAction a{};
    a.actor_team = TEAM_PLAYER;
    a.actor_id = pid;
    a.target_team = TEAM_NPC;
    a.target_id = npc;

    switch (choice) {
        case 1: a.action = ACT_STRIKE; break;
        case 2: a.action = ACT_EXHAUST; break;
        case 3: a.action = ACT_USE_WEAPON; a.weapon = W_IRON_HALBERD; break;
        case 4: a.action = ACT_SWAP_IN; break;
        case 5: a.action = ACT_HEAL; break;
        case 6: a.action = ACT_SKIP; break;
        case 7: a.action = ACT_ULTIMATE; break;
        case 8: a.action = ACT_QUIT; break;
        default: a.action = ACT_SKIP; break;
    }
    return a;
}

// Reads player's action choice from stdin, then atomically submits to shared
// memory. The stdin read is performed while NOT holding the mutex (the lock is
// released during I/O), but the actual submit is done inside a critical section.
//
// This avoids the deadlock risk of holding a mutex during blocking I/O, while
// still ensuring atomicity of the shared state update via the mutex + signal.
//
// OS Concepts: Synchronization (mutex/condvar), I/O in critical sections,
// producer-consumer pattern via action_cv.
void submit_action_player(int pid) {
    int npc = first_living_npc(g);
    if (npc < 0) npc = 0;

    // Read input WITHOUT holding the mutex (avoids blocking other threads on I/O)
    int choice = read_action_choice();

    PendingAction a = build_action(pid, choice, npc);

    if (a.action == ACT_QUIT && g->arbiter_pid > 0) {
        kill(g->arbiter_pid, SIGTERM);
    }

    // Submit atomically inside critical section
    pthread_mutex_lock(&g->mtx);
    g->pending = a;
    g->pending.ready = 1;
    pthread_cond_signal(&g->action_cv);
    pthread_mutex_unlock(&g->mtx);
}

// ═══════════════════════════════════════════════════════════════════════════
// PLAYER THREAD — Per-player thread that waits for turn, handles stun

// Each player gets one thread. Threads synchronize via the shared turn_cv:
//   - Wait until this player's turn (active_team==TEAM_PLAYER && active_id==pid)
//   - Check stun state (via atomic or shared state)
//   - If stunned: submit ACT_SKIP immediately (inside critical section)
//   - If ready: call submit_action_player() which reads stdin and submits
//     the action atomically.
//
// OS Concepts: Thread management, condition variable wait, signal safety,
// atomic state, I/O in concurrent programs.

void* player_thread(void* arg) {
    int pid = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    pthread_mutex_lock(&g->mtx);
    while (!g->initialized) pthread_cond_wait(&g->turn_cv, &g->mtx);

    while (g->running) {
        // Wait for our turn
        while (g->running && !(g->active_team == TEAM_PLAYER && g->active_id == pid)) {
            pthread_cond_wait(&g->turn_cv, &g->mtx);
        }
        if (!g->running) break;

        if (!g->players[pid].alive) continue;

        // Check stun state (signal-delivered or in-game stun)
        int now = now_epoch();
        if (now < stunned_until.load() || now < g->players[pid].stunned_until_epoch) {
            PendingAction s{};
            s.action = ACT_SKIP;
            s.actor_team = TEAM_PLAYER;
            s.actor_id = pid;
            g->pending = s;
            g->pending.ready = 1;
            pthread_cond_signal(&g->action_cv);
            continue;
        }

        // Our turn: read action (outside mutex) then submit (inside mutex)
        pthread_mutex_unlock(&g->mtx);
        submit_action_player(pid);
        // Re-acquire mutex before looping back (pthread_cond_wait requires it)
        pthread_mutex_lock(&g->mtx);
    }

    pthread_mutex_unlock(&g->mtx);
    return nullptr;
}

int main() {
    signal(SIGUSR1, stun_handler);

    g = map_shared(false);
    if (!g) return 1;

    g->hip_pid = getpid();

    std::vector<pthread_t> threads(g->num_players);
    for (int i = 0; i < g->num_players; ++i) {
        int* id = new int(i);
        pthread_create(&threads[i], nullptr, player_thread, id);
    }

    for (auto& t : threads) pthread_join(t, nullptr);

    munmap(g, sizeof(SharedState));
    return 0;
}