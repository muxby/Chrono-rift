

#include "../common.hpp"

#include <csignal>
#include <iostream>

using namespace std;

static SharedState* g = nullptr;
static volatile sig_atomic_t stunned_until = 0;

// stun signal handler - arbiter sends SIGUSR1 when we get stunned
void stun_handler(int) {
    stunned_until = now_epoch() + 3;
}

// clear stdin buffer (No longer needed with getline, kept for compatibility if needed elsewhere)
void clear_input_buffer() {
    cin.clear();
    cin.ignore(10000, '\n');
}

// show current player's inventory
void show_inventory(int pid) {
    cout << "\n--- P" << pid << " Inventory ---\n";
    bool has_items = false;
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        int wid = g->players[pid].inventory[i];
        if (wid > 0 && wid <= W_ECLIPSE_RELIC) {
            auto w = weapon_def(static_cast<WeaponId>(wid));
            cout << "  Slot " << i << ": [" << wid << "] " << w.name
                      << " (DMG:" << w.damage << " Slots:" << w.slots << ")\n";
            has_items = true;
            // Skip remaining slots occupied by the same weapon
            int weapon_size = w.slots;
            if (weapon_size > 1) {
                i += (weapon_size - 1);
            }
        }
    }
    if (!has_items) {
        cout << "  (empty)\n";
    }

    // Show storage
    cout << "  Storage: ";
    if (g->players[pid].storage_count > 0) {
        for (int i = 0; i < g->players[pid].storage_count; ++i) {
            int wid = g->players[pid].storage[i];
            if (wid > 0 && wid <= W_ECLIPSE_RELIC) {
                auto w = weapon_def(static_cast<WeaponId>(wid));
                cout << "[" << wid << "]" << w.name << " ";
            }
        }
    } else {
        cout << "(empty)";
    }
    cout << "\n-------------------\n";
}

// show available enemies
void show_enemies() {
    cout << "\n--- Enemies ---\n";
    for (int i = 0; i < g->num_npcs; ++i) {
        if (g->npcs[i].alive) {
            cout << "  N" << i << ": HP=" << g->npcs[i].hp
                      << "/" << g->npcs[i].max_hp
                      << " ST=" << g->npcs[i].stamina
                      << "/" << g->npcs[i].max_stamina << "\n";
        }
    }
    cout << "-------------------\n";
}

// show available weapons for use
WeaponId choose_weapon(int pid) {
    cout << "\n--- Choose Weapon ---\n";
    WeaponId available[INVENTORY_SLOTS];
    int avail_count = 0;
    bool seen[W_ECLIPSE_RELIC + 1] = {false};

    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        int wid = g->players[pid].inventory[i];
        if (wid > 0 && wid <= W_ECLIPSE_RELIC && !seen[wid]) {
            seen[wid] = true;
            available[avail_count++] = static_cast<WeaponId>(wid);
            auto w = weapon_def(static_cast<WeaponId>(wid));
            cout << "  [" << wid << "] " << w.name
                      << " (DMG:" << w.damage << " Slots:" << w.slots << ")\n";
        }
    }
    cout << "---------------------\n";

    if (avail_count == 0) {
        cout << "No weapons available! Using default attack.\n";
        return W_NONE;
    }

    cout << "Enter weapon number: " << flush;
    int choice;
    string line;
    while (true) {
        if (!getline(cin, line)) return W_NONE;
        if (line.empty()) continue;
        try { choice = stoi(line); break; } catch(...) {}
        cout << "Invalid input. Enter weapon number: " << flush;
    }

    for (int i = 0; i < avail_count; ++i) {
        if (static_cast<int>(available[i]) == choice) {
            return available[i];
        }
    }
    cout << "Invalid weapon. Using default.\n";
    return W_NONE;
}

// choose target enemy
int choose_target() {
    cout << "Enter target enemy number (0-" << (g->num_npcs - 1) << "): " << flush;
    int target;
    string line;
    while (true) {
        if (!getline(cin, line)) return first_living_npc(g);
        if (line.empty()) continue;
        try { target = stoi(line); break; } catch(...) {}
        cout << "Invalid input. Enter target number: " << flush;
    }

    if (target >= 0 && target < g->num_npcs && g->npcs[target].alive) {
        return target;
    }
    // Fall back to first living
    int fallback = first_living_npc(g);
    cout << "Invalid target. Using N" << fallback << ".\n";
    return fallback >= 0 ? fallback : 0;
}

// choose weapon from storage for swap-in
WeaponId choose_storage_weapon(int pid) {
    cout << "\n--- Storage ---\n";
    if (g->players[pid].storage_count == 0) {
        cout << "  (empty)\n";
        return W_NONE;
    }
    for (int i = 0; i < g->players[pid].storage_count; ++i) {
        int wid = g->players[pid].storage[i];
        if (wid > 0 && wid <= W_ECLIPSE_RELIC) {
            auto w = weapon_def(static_cast<WeaponId>(wid));
            cout << "  [" << i << "] " << w.name
                      << " (DMG:" << w.damage << " Slots:" << w.slots << ")\n";
        }
    }
    cout << "---------------\n";
    cout << "Enter storage index: " << flush;
    int idx;
    string line;
    while (true) {
        if (!getline(cin, line)) return W_NONE;
        if (line.empty()) continue;
        try { idx = stoi(line); break; } catch(...) {}
        cout << "Invalid input. Enter storage index: " << flush;
    }

    if (idx >= 0 && idx < g->players[pid].storage_count) {
        return static_cast<WeaponId>(g->players[pid].storage[idx]);
    }
    cout << "Invalid index.\n";
    return W_NONE;
}

// read player's action choice from stdin
int read_action_choice(int pid) {
    show_inventory(pid);
    show_enemies();

    cout << "\n===== P" << pid << " TURN =====\n";
    cout << "HP: " << g->players[pid].hp << "/" << g->players[pid].max_hp
              << " | Stamina: " << g->players[pid].stamina << "/" << g->players[pid].max_stamina
              << " | DMG: " << g->players[pid].dmg << "\n";
    cout << "Choose action:\n";
    cout << "  1) Attack (Strike)\n";
    cout << "  2) Exhaust (reduce enemy stamina)\n";
    cout << "  3) Use Weapon\n";
    cout << "  4) Swap In (weapon from storage)\n";
    cout << "  5) Heal (+10% HP)\n";
    cout << "  6) Skip (restore 50% stamina)\n";
    cout << "  7) Ultimate (requires both artifacts)\n";
    cout << "  8) Quit\n";
    cout << "Enter choice (1-8): " << flush;

    int ch = 6; // default to skip
    string line;
    while (true) {
        if (!getline(cin, line)) return 6; // skip on EOF
        if (line.empty()) continue;
        try {
            ch = stoi(line);
            if (ch >= 1 && ch <= 8) break;
        } catch(...) {}
        cout << "Invalid choice. Enter a number 1-8: " << flush;
    }
    return ch;
}

// build a PendingAction from the player's choice
PendingAction build_action(int pid, int choice, int npc) {
    PendingAction a{};
    a.actor_team = TEAM_PLAYER;
    a.actor_id = pid;
    a.target_team = TEAM_NPC;
    a.target_id = npc;

    switch (choice) {
        case 1: a.action = ACT_STRIKE; break;
        case 2: a.action = ACT_EXHAUST; break;
        case 3: {
            a.action = ACT_USE_WEAPON;
            a.weapon = choose_weapon(pid);
            break;
        }
        case 4: {
            a.action = ACT_SWAP_IN;
            a.weapon = choose_storage_weapon(pid);
            break;
        }
        case 5: a.action = ACT_HEAL; break;
        case 6: a.action = ACT_SKIP; break;
        case 7: a.action = ACT_ULTIMATE; break;
        case 8: a.action = ACT_QUIT; break;
        default: a.action = ACT_SKIP; break;
    }
    return a;
}

// read input first (no lock), then lock and write to shared memory
void submit_action_player(int pid) {
    int npc = first_living_npc(g);
    if (npc < 0) npc = 0;

    // read input without holding the lock
    int choice = read_action_choice(pid);

    // For strike/exhaust/weapon, let player choose target
    if (choice == 1 || choice == 2 || choice == 3) {
        cout << "Choose target:\n";
        npc = choose_target();
    }

    PendingAction a = build_action(pid, choice, npc);

    if (a.action == ACT_QUIT && g->arbiter_pid > 0) {
        kill(g->arbiter_pid, SIGTERM);
    }

    // submit to shared memory - must hold mutex
    pthread_mutex_lock(&g->mtx);
    g->pending = a;
    g->pending.ready = 1;
    pthread_cond_signal(&g->action_cv);
    pthread_mutex_unlock(&g->mtx);

    sem_post(&g->action_sem);
}

// each player runs in its own thread, sleeps until its turn
void* player_thread(void* arg) {
    int pid = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    pthread_mutex_lock(&g->mtx);
    while (!g->initialized) pthread_cond_wait(&g->turn_cv, &g->mtx);

    while (g->running) {
        // wait for our turn — arbiter broadcasts turn_cv when active_team/id changes
        while (g->running && !(g->active_team == TEAM_PLAYER && g->active_id == pid)) {
            pthread_cond_wait(&g->turn_cv, &g->mtx);
        }
        if (!g->running) break;

        if (!g->players[pid].alive) continue;

        // Check stun state
        int now = now_epoch();
        if (now < stunned_until || now < g->players[pid].stunned_until_epoch) {
            PendingAction s{};
            s.action = ACT_SKIP;
            s.actor_team = TEAM_PLAYER;
            s.actor_id = pid;
            g->pending = s;
            g->pending.ready = 1;
            pthread_cond_signal(&g->action_cv);
            pthread_mutex_unlock(&g->mtx);
            sem_post(&g->action_sem);
            pthread_mutex_lock(&g->mtx);
            continue;
        }

        // not stunned - get player input (released mutex first)
        pthread_mutex_unlock(&g->mtx);
        submit_action_player(pid);
        pthread_mutex_lock(&g->mtx);
    }

    pthread_mutex_unlock(&g->mtx);
    return nullptr;
}

// dedicated thread just for catching SIGUSR1 stun signals
void* signal_thread(void* arg) {
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (g->running) {
        sigsuspend(&oldmask);
        // stun_handler already ran and set stunned_until
    }
    return nullptr;
}

int main() {
    signal(SIGUSR1, stun_handler);

    g = map_shared(false);
    if (!g) return 1;

    g->hip_pid = getpid();

    // one thread per player character
    pthread_t threads[MAX_PLAYERS];
    for (int i = 0; i < g->num_players; ++i) {
        int* id = new int(i);
        pthread_create(&threads[i], nullptr, player_thread, id);
    }

    // spawn a dedicated signal-handler thread (detached)
    pthread_t sig_thr;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&sig_thr, &attr, signal_thread, nullptr);
    pthread_attr_destroy(&attr);

    for (int i = 0; i < g->num_players; ++i) pthread_join(threads[i], nullptr);

    cleanup_shared(g, false);
    return 0;
}
