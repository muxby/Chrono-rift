/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  CHRONO RIFT — SFML Arbiter (Complete Game Logic + Visualizer UI)      ║
 * ║  Full game implementation with beautiful SFML visualization            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * This is the COMPLETE game arbiter with three modes:
 *   • Full Visualizer UI (Combat/Scheduler/Hybrid views)
 *   • Complete stamina-based scheduling logic
 *   • Deadlock detection and artifact management
 *   • Stun mechanics and Ultimate ability
 *   • Weapon inventory with space allocator
 *
 * Modes:
 *   ./sfml_arbiter                    - Interactive (prompts for input)
 *   ./sfml_arbiter <roll> <party>     - Direct launch (no prompts)
 */

#include "Visualizer.hpp"
#include "../common.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <pthread.h>
#include <unistd.h>

// ════════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ════════════════════════════════════════════════════════════════════════════

static SharedState* g = nullptr;
static volatile sig_atomic_t asp_paused = 0;
static volatile sig_atomic_t terminate_requested = 0;

// Command-line parameters (0 = use interactive prompts)
static int g_roll_arg = 0;
static int g_party_arg = 0;

static void sigalrm_handler(int) {
    asp_paused = 0;
    if (g && g->asp_pid > 0) kill(g->asp_pid, SIGCONT);
}

static void sigterm_handler(int) {
    terminate_requested = 1;
}

// ════════════════════════════════════════════════════════════════════════════
// RENDER THREAD — Uses the full Visualizer UI
// ════════════════════════════════════════════════════════════════════════════

void* render_thread(void*) {
    ChronoRift::Visualizer visualizer;
    if (!visualizer.initialize()) {
        std::cerr << "[ERROR] Failed to initialize SFML Visualizer!" << std::endl;
        return nullptr;
    }

    std::cout << "[OK] SFML Visualizer initialized. Opening window..." << std::endl;

    // The Visualizer::run method blocks until the game ends
    visualizer.run(g);

    // If visualizer exits (window closed), signal game to stop
    if (g && g->running) {
        g->running = 0;
    }

    visualizer.shutdown();
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════════════
// ORIGINAL GAME LOGIC — COMPLETELY UNCHANGED
// ════════════════════════════════════════════════════════════════════════════

void init_char(CharacterState& c, int id, TeamType t, int hp, int dmg, int speed, int max_stamina) {
    std::memset(&c, 0, sizeof(c));
    c.alive = 1;
    c.id = id;
    c.team = t;
    c.hp = hp;
    c.max_hp = hp;
    c.dmg = dmg;
    c.speed = speed;
    c.stamina = 0;
    c.max_stamina = max_stamina;
}

void init_state(SharedState* s) {
    std::memset(s, 0, sizeof(*s));

    pthread_mutexattr_t ma;
    pthread_condattr_t ca;
    pthread_mutexattr_init(&ma);
    pthread_condattr_init(&ca);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&s->mtx, &ma);
    pthread_cond_init(&s->turn_cv, &ca);
    pthread_cond_init(&s->action_cv, &ca);

    s->running = 1;
    s->pending.ready = 0;
    s->log_head = 0;
    s->kills = 0;

    int roll, party;

    // Use command-line arguments if provided, otherwise prompt interactively
    if (g_roll_arg > 0 && g_party_arg > 0) {
        roll = g_roll_arg;
        party = g_party_arg;
        std::cout << "Using roll number: " << roll << ", party size: " << party << std::endl;
    } else {
        std::cout << "Enter roll number seed: ";
        std::cin >> roll;
        std::cout << "Enter party size (1-4): ";
        std::cin >> party;
    }

    party = std::max(1, std::min(4, party));

    s->roll_seed = roll;
    s->num_players = party;

    std::mt19937 rng(roll);
    std::uniform_int_distribution<int> hpP(100, 1000), hpE(50, 200), nE(2, 9), spE(10, 30);

    int last = roll % 10;
    int second_last = (roll / 10) % 10;
    int last2 = roll % 100;

    for (int i = 0; i < s->num_players; ++i) {
        int hp = roll + hpP(rng);
        int dmg = last + 10;
        int speed = 100 / s->num_players;
        init_char(s->players[i], i, TEAM_PLAYER, hp, dmg, speed, 100);
    }

    s->num_npcs = nE(rng);
    for (int i = 0; i < s->num_npcs; ++i) {
        int hp = last2 + hpE(rng);
        int dmg = second_last + 10;
        int speed = spE(rng);
        init_char(s->npcs[i], i, TEAM_NPC, hp, dmg, speed, 150);
    }

    s->artifacts[0] = {W_SOLAR_CORE, 1, -1, -1, 0, {0}, {0}};
    s->artifacts[1] = {W_LUNAR_BLADE, 1, -1, -1, 0, {0}, {0}};
    s->artifacts[2] = {W_ECLIPSE_RELIC, 0, -1, -1, 0, {0}, {0}};

    s->active_team = TEAM_PLAYER;
    s->active_id = 0;
    s->turn_seq = 1;

    s->initialized = 1;
    pthread_cond_broadcast(&s->turn_cv);
}

void* deadlock_monitor(void*) {
    while (true) {
        sleep(1);
        pthread_mutex_lock(&g->mtx);
        if (!g->running) {
            pthread_mutex_unlock(&g->mtx);
            break;
        }

        auto& a = g->artifacts[0];
        auto& b = g->artifacts[1];

        bool both_held = (a.holder_team != -1 && b.holder_team != -1);
        bool same_holder = (a.holder_team == b.holder_team && a.holder_id == b.holder_id);
        bool deadlock = both_held && !same_holder;

        if (deadlock) {
            b.holder_team = -1;
            b.holder_id = -1;
            add_log(g, "Deadlock detected: Arbiter forced Lunar Blade release");
        }

        pthread_mutex_unlock(&g->mtx);
    }
    return nullptr;
}

void apply_action(PendingAction& a) {
    CharacterState* actor = get_character(g, a.actor_team, a.actor_id);
    CharacterState* target = get_character(g, a.target_team, a.target_id);
    if (!actor || !actor->alive) return;

    if (a.action == ACT_QUIT) {
        g->running = 0;
        add_log(g, "Quit requested");
        return;
    }

    if (a.action == ACT_SKIP) {
        actor->stamina = actor->max_stamina / 2;
        add_log(g, "Skip action committed");
        return;
    }

    if (a.action == ACT_HEAL) {
        actor->hp = std::min(actor->max_hp, actor->hp + actor->max_hp / 10);
        actor->stamina = 0;
        add_log(g, "Heal action committed");
        return;
    }

    if (a.action == ACT_SWAP_IN) {
        if (actor->storage_count > 0) {
            WeaponId w = static_cast<WeaponId>(actor->storage[actor->storage_count - 1]);
            actor->storage_count--;
            if (!allocate_weapon(actor, w)) swap_out_minimal(actor, w);
        }
        actor->stamina = 0;
        add_log(g, "Swap In action committed");
        return;
    }

    if (a.action == ACT_ULTIMATE) {
        bool ok = has_weapon(actor, W_SOLAR_CORE) && has_weapon(actor, W_LUNAR_BLADE);
        if (ok && g->asp_pid > 0) {
            kill(g->asp_pid, SIGSTOP);
            asp_paused = 1;
            signal(SIGALRM, sigalrm_handler);
            alarm(10);
            add_log(g, "Ultimate triggered: ASP paused for 10s");
        }
        actor->stamina = 0;
        return;
    }

    if (!target || !target->alive) {
        actor->stamina = 0;
        return;
    }

    int dmg = actor->dmg;
    if (a.action == ACT_USE_WEAPON) dmg = weapon_def(a.weapon).damage;

    if (a.action == ACT_EXHAUST) {
        target->stamina = std::max(0, target->stamina - dmg);
    } else {
        target->hp -= dmg;
        if (target->hp <= 0) {
            target->hp = 0;
            target->alive = 0;
            if (target->team == TEAM_NPC) {
                g->kills++;
                if ((std::rand() % 100) < 50) {
                    WeaponId drop = static_cast<WeaponId>((std::rand() % 8) + 1);
                    int p = first_living_player(g);
                    if (p >= 0) {
                        CharacterState* pl = &g->players[p];
                        if (!allocate_weapon(pl, drop)) swap_out_minimal(pl, drop);
                    }
                }
            }
        }
    }

    actor->stamina = 0;

    if ((std::rand() % 100) < 20 && target->alive) {
        target->stunned_until_epoch = now_epoch() + 3;
        if (target->team == TEAM_PLAYER && g->hip_pid > 0) kill(g->hip_pid, SIGUSR1);
        if (target->team == TEAM_NPC && g->asp_pid > 0) kill(g->asp_pid, SIGUSR1);
        add_log(g, "Stun applied for 3s");
    }

    if (!g->eclipse_present && (std::rand() % 100) < 10) {
        g->artifacts[2].present = 1;
        g->eclipse_present = 1;
        add_log(g, "Eclipse Relic appeared");
    }
}

static time_t g_turn_start = 0;
static int g_turn_timeout = 3;

void schedule_next_turn() {
    int chosen_team = -1, chosen_id = -1;
    for (int i = 0; i < g->num_players && chosen_team == -1; ++i)
        if (g->players[i].alive && g->players[i].stamina >= g->players[i].max_stamina) {
            chosen_team = TEAM_PLAYER;
            chosen_id = i;
        }

    for (int i = 0; i < g->num_npcs && chosen_team == -1; ++i)
        if (g->npcs[i].alive && g->npcs[i].stamina >= g->npcs[i].max_stamina) {
            chosen_team = TEAM_NPC;
            chosen_id = i;
        }

    if (chosen_team != -1) {
        g->active_team = chosen_team;
        g->active_id = chosen_id;
        g->turn_seq++;
        g->pending.ready = 0;
        g_turn_start = now_epoch();
        pthread_cond_broadcast(&g->turn_cv);
        return;
    }

    // No entity ready — wait for turn_scheduler_thread to signal stamina updates.
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;

    while (chosen_team == -1 && g->running) {
        int rc = pthread_cond_timedwait(&g->turn_cv, &g->mtx, &ts);
        if (rc == ETIMEDOUT) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
        }

        for (int i = 0; i < g->num_players && chosen_team == -1; ++i)
            if (g->players[i].alive && g->players[i].stamina >= g->players[i].max_stamina) {
                chosen_team = TEAM_PLAYER;
                chosen_id = i;
            }
        for (int i = 0; i < g->num_npcs && chosen_team == -1; ++i)
            if (g->npcs[i].alive && g->npcs[i].stamina >= g->npcs[i].max_stamina) {
                chosen_team = TEAM_NPC;
                chosen_id = i;
            }
    }

    if (chosen_team != -1 && g->running) {
        g->active_team = chosen_team;
        g->active_id = chosen_id;
        g->turn_seq++;
        g->pending.ready = 0;
        g_turn_start = now_epoch();
        pthread_cond_broadcast(&g->turn_cv);
    }
}

static void* turn_scheduler_thread(void*) {
    while (true) {
        sleep(1);
        pthread_mutex_lock(&g->mtx);
        if (!g->running) {
            pthread_mutex_unlock(&g->mtx);
            return nullptr;
        }

        int now = now_epoch();
        for (int i = 0; i < g->num_players; ++i) {
            if (!g->players[i].alive) continue;
            if (now < g->players[i].stunned_until_epoch) continue;
            g->players[i].stamina = std::min(g->players[i].max_stamina,
                                            g->players[i].stamina + g->players[i].speed);
        }
        for (int i = 0; i < g->num_npcs; ++i) {
            if (!g->npcs[i].alive) continue;
            if (now < g->npcs[i].stunned_until_epoch) continue;
            g->npcs[i].stamina = std::min(g->npcs[i].max_stamina,
                                          g->npcs[i].stamina + g->npcs[i].speed);
        }

        pthread_cond_broadcast(&g->turn_cv);
        pthread_mutex_unlock(&g->mtx);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    // Parse optional command-line arguments: ./sfml_arbiter <roll> <party>
    if (argc >= 3) {
        g_roll_arg = std::atoi(argv[1]);
        g_party_arg = std::atoi(argv[2]);
        if (g_party_arg < 1 || g_party_arg > 4) g_party_arg = 0; // Invalid, will prompt
    }

    signal(SIGALRM, sigalrm_handler);
    signal(SIGTERM, sigterm_handler);

    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CHRONO RIFT — SFML Arbiter (Visualizer UI + Full Logic)      ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Modes: [1]Combat [2]Scheduler [3]Hybrid | ESC: Exit         ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    g = map_shared(true);
    if (!g) return 1;

    init_state(g);
    g->arbiter_pid = getpid();
    g_turn_start = now_epoch();

    // Start deadlock monitor, render thread, and turn scheduler
    pthread_t mon, ren, sch;
    pthread_create(&mon, nullptr, deadlock_monitor, nullptr);
    pthread_create(&ren, nullptr, render_thread, nullptr);
    pthread_create(&sch, nullptr, turn_scheduler_thread, nullptr);

    pthread_mutex_lock(&g->mtx);
    add_log(g, "SFML Arbiter started (Visualizer UI)");

    // Main game loop
    while (g->running) {
        if (terminate_requested) {
            g->running = 0;
            g->win = 0;
            add_log(g, "Quit condition met (SIGTERM)");
            break;
        }

        int alive_players = 0;
        for (int i = 0; i < g->num_players; ++i) alive_players += g->players[i].alive;
        if (alive_players == 0) {
            g->running = 0;
            g->win = 0;
            add_log(g, "Lose condition met");
            break;
        }
        if (g->kills >= 10) {
            g->running = 0;
            g->win = 1;
            add_log(g, "Win condition met");
            break;
        }

        schedule_next_turn();

        if (g->active_team == TEAM_NPC) {
            timespec ts{};
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 3;
            int rc = 0;
            while (g->running && !g->pending.ready && rc != ETIMEDOUT) {
                rc = pthread_cond_timedwait(&g->action_cv, &g->mtx, &ts);
                if (terminate_requested) {
                    g->running = 0;
                    g->win = 0;
                    add_log(g, "Quit condition met (SIGTERM)");
                    break;
                }
            }
            if (!g->running) break;
            if (!g->pending.ready) {
                PendingAction skip{};
                skip.action = ACT_SKIP;
                skip.actor_team = TEAM_NPC;
                skip.actor_id = g->active_id;
                apply_action(skip);
            } else {
                apply_action(g->pending);
                g->pending.ready = 0;
            }
        } else {
            while (g->running && !g->pending.ready) {
                pthread_cond_wait(&g->action_cv, &g->mtx);
                if (terminate_requested) {
                    g->running = 0;
                    g->win = 0;
                    add_log(g, "Quit condition met (SIGTERM)");
                    break;
                }
            }
            if (!g->running) break;
            if (g->pending.ready) {
                apply_action(g->pending);
                g->pending.ready = 0;
            }
        }
    }

    // Signal all waiting threads to exit
    pthread_cond_broadcast(&g->turn_cv);
    pthread_cond_broadcast(&g->action_cv);
    pthread_mutex_unlock(&g->mtx);

    // Wait for all threads to finish
    pthread_join(mon, nullptr);
    pthread_join(ren, nullptr);
    pthread_join(sch, nullptr);

    munmap(g, sizeof(SharedState));
    shm_unlink(SHM_NAME);

    std::cout << "\n[OK] SFML Arbiter exited cleanly." << std::endl;
    return 0;
}
