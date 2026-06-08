// sfml_arbiter.cpp - main game process, handles scheduling and applies actions
// runs the render thread, deadlock monitor, and stamina ticker as background threads

#include "Visualizer.hpp"
#include "ThreadPool.hpp"
#include "os_helpers.hpp"
#include "../common.hpp"
#include "../memory_demo.hpp"

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <pthread.h>
#include <unistd.h>

using namespace std;
using namespace ChronoRift;

#define CRMAX(a,b) ((a) > (b) ? (a) : (b))
#define CRMIN(a,b) ((a) < (b) ? (a) : (b))

// globals
static SharedState* g = nullptr;
static volatile sig_atomic_t asp_paused = 0;
static volatile sig_atomic_t terminate_requested = 0;
static string g_roll_arg_str = "";
static int g_party_arg = 0;

// Memory arena for demonstrating malloc/free + fragmentation
static MemArena g_mem_arena;

// Thread pool for demonstrating thread pool + detached threads pattern
static ThreadPool* g_thread_pool = nullptr;

// SIGALRM handler: resumes ASP after ultimate ability (10s pause)
static void sigalrm_handler(int) {
    asp_paused = 0;
    if (g && g->asp_pid > 0) kill(g->asp_pid, SIGCONT);
}

static void sigterm_handler(int) {
    terminate_requested = 1;
}

// render thread - runs SFML visualizer in its own thread
void* render_thread(void*) {
    Visualizer visualizer;
    if (!visualizer.initialize()) {
        cerr << "[ERROR] Failed to initialize SFML Visualizer!" << endl;
        return nullptr;
    }

    cout << "[OK] SFML Visualizer initialized. Opening window..." << endl;
    visualizer.run(g);

    // window was closed, stop the game
    if (g && g->running) {
        g->running = 0;
    }

    visualizer.shutdown();
    return nullptr;
}

// initialize a character with given stats
void init_char(CharacterState& c, int id, TeamType t, int hp, int dmg, int speed, int max_stamina) {
    memset(&c, 0, sizeof(c));
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

// force the character to drop whatever artifact they're holding
static void force_release_artifact(CharacterState* c) {
    for (int a = 0; a < 3; ++a) {
        auto& art = g->artifacts[a];
        if (art.present && art.holder_team == c->team && art.holder_id == c->id) {
            art.holder_team = -1;
            art.holder_id = -1;
            add_log(g, "Deadlock prevention: force-released artifact");
            break;
        }
    }
}

// stun whoever has lower speed so the other can grab the artifact
static void preempt_lower_priority(const ArtifactState& a, const ArtifactState& b) {
    CharacterState* low = nullptr;

    // b = lunar blade, a = solar core
    if (b.holder_team != -1) {
        CharacterState* holder = get_character(g, b.holder_team, b.holder_id);
        if (holder) {
            CharacterState* other = get_character(g, a.holder_team, a.holder_id);
            if (other) {
                if (holder->speed < other->speed) {
                    low = holder;
                } else {
                    low = other;
                }
                if (low) {
                    low->stunned_until_epoch = now_epoch() + 3;
                    add_log(g, "Preempt: lower-priority holder stunned, artifact freed");
                }
            }
        }
    }
}

// submitted to thread pool every tick to check for circular wait
static void* deadlock_checker_task(void* arg) {
    (void)arg;
    pthread_mutex_lock(&g->mtx);
    if (!g->running) {
        pthread_mutex_unlock(&g->mtx);
        return nullptr;
    }

    auto& a = g->artifacts[0]; // solar core
    auto& b = g->artifacts[1]; // lunar blade

    bool both_held = (a.holder_team != -1 && b.holder_team != -1);
    bool same_holder = (a.holder_team == b.holder_team && a.holder_id == b.holder_id);
    bool deadlock = both_held && !same_holder;

    if (deadlock) {
        switch (g->deadlock_strategy) {
            case DETECT_ONLY:
                // Original behavior: force release Lunar Blade
                b.holder_team = -1;
                b.holder_id = -1;
                add_log(g, "Deadlock detected [DETECT]: Arbiter forced Lunar Blade release");
                break;
            case NO_HOLD_WAIT:
                // Force both to release their artifacts (break hold-and-wait)
                if (a.holder_team != -1) {
                    CharacterState* ah = get_character(g, a.holder_team, a.holder_id);
                    if (ah) force_release_artifact(ah);
                }
                if (b.holder_team != -1) {
                    CharacterState* bh = get_character(g, b.holder_team, b.holder_id);
                    if (bh) force_release_artifact(bh);
                }
                add_log(g, "Deadlock prevention [NO_HOLD_WAIT]: force-released both artifacts");
                break;
            case PREEMPT:
                preempt_lower_priority(a, b);
                add_log(g, "Deadlock prevention [PREEMPT]: preempted lower-priority holder");
                break;
        }
    }

    pthread_mutex_unlock(&g->mtx);
    return nullptr;
}


static void log_arena_fragmentation() {
    if (g_mem_arena.count == 0) return;
    float frag = arena_fragmentation(&g_mem_arena);
    char buf[128];
    snprintf(buf, sizeof(buf), "Mem arena: %.1f%% fragmentation, %d/%d blocks",
             (double)frag, g_mem_arena.count, ARENA_MAX_BLOCKS);
    add_log(g, buf);
}

static int last_digit(int n) {
    return n % 10;
}

static int last_two_digits(int n) {
    return n % 100;
}

// make sure the roll number is actually a positive int (not letters or negatives)
static bool is_valid_positive_integer(const string& input, int& out_value) {
    if (input.empty()) return false;
    for (char c : input) {
        if (c < '0' || c > '9') return false;
    }
    // Check for leading zeros that would make it look like not a simple integer
    try {
        // Use long long to check for overflow, then validate range
        long long val = stoll(input);
        if (val <= 0 || val > 2147483647LL) return false;
        out_value = static_cast<int>(val);
        return true;
    } catch (...) {
        return false;
    }
}

// set up initial game state: players, npcs, artifacts, semaphores
void init_state(SharedState* s) {
    memset(s, 0, sizeof(*s));

    // process-shared mutex and condvars so hip/asp can use them
    pthread_mutexattr_t ma;
    pthread_condattr_t ca;
    pthread_mutexattr_init(&ma);
    pthread_condattr_init(&ca);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&s->mtx, &ma);
    pthread_cond_init(&s->turn_cv, &ca);
    pthread_cond_init(&s->action_cv, &ca);

    // semaphores need pshared=1 so hip/asp can use them across processes
    sem_init(&s->action_sem, 1, 0);  // starts locked, posted when action is ready
    sem_init(&s->turn_sem,   1, 0);  // tracks how many entities have stamina filled
    sem_init(&s->log_sem,    1, 1);  // protects log writes

    // defaults
    s->scheduler_mode = SCHED_STAMINA;
    s->quantum_ms = 2000;  // 2 seconds default
    s->deadlock_strategy = DETECT_ONLY;

    s->running = 1;
    s->pending.ready = 0;
    s->log_head = 0;
    s->kills = 0;

    int roll = 0;
    // Player count and enemy count are always fixed at 4
    int party = 4;
    string roll_input;

    // if launched via standalone launcher, roll arg is already passed in
    if (!g_roll_arg_str.empty()) {
        int parsed_roll = 0;
        if (!is_valid_positive_integer(g_roll_arg_str, parsed_roll)) {
            cerr << "[ERROR] Roll number must be a positive integer (1 to 2147483647).\n";
            cerr << "        Got: " << g_roll_arg_str << "\n";
            cerr << "        Example: ./sfml_arbiter 12345\n";
            exit(1);
        }
        roll_input = g_roll_arg_str;
        roll = parsed_roll;
        cout << "Using roll input: " << roll_input << ", party size: " << party << " (fixed)" << endl;
    } else {
        // Interactive prompt for roll number only
        while (true) {
            cout << "Enter roll number (positive integer only): ";
            cin >> ws;
            getline(cin, roll_input);
            if (is_valid_positive_integer(roll_input, roll)) {
                break;
            }
            cout << "[INVALID] Roll number must be a positive integer (1, 2, 123, etc.).\n";
            cout << "          No letters, no negative numbers, no zero, no decimals.\n\n";
        }
        cout << "Party size: " << party << " (fixed)\n";
    }

    s->roll_seed = roll;
    s->num_players = party;

    srand(roll);

    // stats derived from roll number as per project spec
    int roll_last_digit = last_digit(roll);
    int roll_second_last = (roll / 10) % 10;
    int roll_last_two = last_two_digits(roll);

    // Enemy count: always fixed at 4
    s->num_npcs = 4;

    // Player stats
    for (int i = 0; i < s->num_players; ++i) {
        int hp = roll + (rand() % 901) + 100;  // roll + 100 to roll + 1000
        int dmg = roll_last_digit + 10;
        int speed = 100 / s->num_players;  // integer division per spec
        init_char(s->players[i], i, TEAM_PLAYER, hp, dmg, speed, 100);
    }

    // NPC stats — dmg multiplied by 3 so enemy hits are noticeable
    for (int i = 0; i < s->num_npcs; ++i) {
        int hp = roll_last_two + (rand() % 151) + 50;  // last2digits + 50 to last2digits + 200
        int dmg = (roll_second_last + 10) * 3;  // 3x multiplier for impactful hits
        int speed = (rand() % 21) + 10;  // 10 to 30
        init_char(s->npcs[i], i, TEAM_NPC, hp, dmg, speed, 150);
    }

    // Give each player a starting weapon (Splinter Stick - 2 slots)
    for (int i = 0; i < s->num_players; ++i) {
        allocate_weapon(&s->players[i], W_SPLINTER_STICK);
    }

    // artifacts
    s->artifacts[0] = {W_SOLAR_CORE, 1, -1, -1, 0, {0}, {0}};
    s->artifacts[1] = {W_LUNAR_BLADE, 1, -1, -1, 0, {0}, {0}};
    s->artifacts[2] = {W_ECLIPSE_RELIC, 0, -1, -1, 0, {0}, {0}};

    s->active_team = TEAM_PLAYER;
    s->active_id = 0;
    s->turn_seq = 1;
    s->turn_order = 0; // player goes first
    s->selected_player_id = 0; // default target for enemy attacks

    s->initialized = 1;
    pthread_cond_broadcast(&s->turn_cv);
}

// process the submitted action
void apply_action(PendingAction& a) {
    CharacterState* actor = get_character(g, a.actor_team, a.actor_id);
    CharacterState* target = get_character(g, a.target_team, a.target_id);
    if (!actor || !actor->alive) return;

    char buf[256];
    char actor_label = (a.actor_team == TEAM_PLAYER) ? 'P' : 'N';
    int actor_num = a.actor_id;

    if (a.action == ACT_QUIT) {
        g->running = 0;
        add_log(g, "Quit requested");
        return;
    }

    if (a.action == ACT_SKIP) {
        actor->stamina = actor->max_stamina / 2;
        actor->ready_epoch = 0;
        snprintf(buf, 256, "%c%d skips: stamina restored to %d",
                 actor_label, actor_num, actor->stamina);
        add_log(g, buf);
        return;
    }

    if (a.action == ACT_HEAL) {
        int old_hp = actor->hp;
        int heal_amt = actor->max_hp / 10;
        actor->hp = CRMIN(actor->max_hp, actor->hp + heal_amt);
        actor->stamina = 0;
        actor->ready_epoch = 0;
        snprintf(buf, 256, "%c%d heals: %d HP -> %d HP (+%d)",
                 actor_label, actor_num, old_hp, actor->hp, actor->hp - old_hp);
        add_log(g, buf);
        // flip turn order
        g->turn_order = (a.actor_team == TEAM_PLAYER) ? 1 : 0;
        return;
    }

    if (a.action == ACT_SWAP_IN) {
        if (actor->storage_count > 0 && a.weapon != W_NONE) {
            WeaponId w = a.weapon;
            // Find and remove the requested weapon from storage
            int found_idx = -1;
            for (int i = 0; i < actor->storage_count; ++i) {
                if (actor->storage[i] == static_cast<int>(w)) {
                    found_idx = i;
                    break;
                }
            }
            if (found_idx >= 0) {
                // Remove from storage by shifting
                for (int i = found_idx; i < actor->storage_count - 1; ++i) {
                    actor->storage[i] = actor->storage[i + 1];
                }
                actor->storage_count--;
                if (!allocate_weapon(actor, w)) swap_out_minimal(actor, w);
                snprintf(buf, 256, "%c%d swaps in %s from storage",
                         actor_label, actor_num, weapon_def(w).name);
                add_log(g, buf);
            } else {
                snprintf(buf, 256, "%c%d swap failed: weapon not in storage",
                         actor_label, actor_num);
                add_log(g, buf);
            }
        } else if (actor->storage_count > 0) {
            // Default: swap in most recent storage item
            WeaponId w = static_cast<WeaponId>(actor->storage[actor->storage_count - 1]);
            actor->storage_count--;
            if (!allocate_weapon(actor, w)) swap_out_minimal(actor, w);
            snprintf(buf, 256, "%c%d swaps in %s from storage",
                     actor_label, actor_num, weapon_def(w).name);
            add_log(g, buf);
        } else {
            snprintf(buf, 256, "%c%d swap failed: storage is empty",
                     actor_label, actor_num);
            add_log(g, buf);
        }
        actor->stamina = 0;
        // flip turn order for SWAP_IN action
        g->turn_order = (a.actor_team == TEAM_PLAYER) ? 1 : 0;
        return;
    }

    // ultimate ability: requires both artifacts, pauses ASP for 10s
    if (a.action == ACT_ULTIMATE) {
        bool ok = has_weapon(actor, W_SOLAR_CORE) && has_weapon(actor, W_LUNAR_BLADE);
        if (ok && g->asp_pid > 0) {
            kill(g->asp_pid, SIGSTOP); // pause asp process
            asp_paused = 1;
            signal(SIGALRM, sigalrm_handler);
            alarm(10); // resume after 10 seconds via SIGALRM
            snprintf(buf, 256, "%c%d activates ULTIMATE: ASP paused 10s",
                     actor_label, actor_num);
            add_log(g, buf);
        } else if (!ok) {
            snprintf(buf, 256, "%c%d ULTIMATE FAILED: missing artifacts",
                     actor_label, actor_num);
            add_log(g, buf);
        }
        actor->stamina = 0;
        // flip turn order for ULTIMATE action
        g->turn_order = (a.actor_team == TEAM_PLAYER) ? 1 : 0;
        return;
    }

    if (!target || !target->alive) {
        actor->stamina = 0;
        return;
    }

    char target_label = (a.target_team == TEAM_PLAYER) ? 'P' : 'N';
    int target_num = a.target_id;

    int base_dmg = actor->dmg;
    // NPC attacks deal extra damage to make enemy turns impactful
    // Player attacks use regular roll; NPC attacks use larger roll range
    bool is_npc_attacker = (a.actor_team == TEAM_NPC);
    int roll_val = is_npc_attacker ? (rand() % 51) : (rand() % 16);  // NPC: 0-50, Player: 0-15

    if (a.action == ACT_USE_WEAPON) {
        base_dmg = weapon_def(a.weapon).damage;
        int total_dmg = base_dmg + roll_val;
        int old_hp = target->hp;
        target->hp -= total_dmg;
        snprintf(buf, 256, "%c%d uses %s on %c%d: roll(%d) + wpn_dmg(%d) = %d dmg",
                 actor_label, actor_num, weapon_def(a.weapon).name,
                 target_label, target_num, roll_val, base_dmg, total_dmg);
        add_log(g, buf);
        snprintf(buf, 256, "%c%d: %d HP -> %d HP",
                 target_label, target_num, old_hp, target->hp);
        add_log(g, buf);
    } else if (a.action == ACT_EXHAUST) {
        int old_stam = target->stamina;
        int total_dmg = base_dmg + roll_val;
        target->stamina = CRMAX(0, target->stamina - total_dmg);
        snprintf(buf, 256, "%c%d exhausts %c%d: roll(%d) + base(%d) = -%d stamina (%d -> %d)",
                 actor_label, actor_num, target_label, target_num,
                 roll_val, base_dmg, total_dmg, old_stam, target->stamina);
        add_log(g, buf);
    } else {
        // basic strike — NPC strikes hit 50% harder for extra impact
        int strike_dmg = is_npc_attacker ? (base_dmg * 3 / 2) : base_dmg;
        int total_dmg = strike_dmg + roll_val;
        int old_hp = target->hp;
        target->hp -= total_dmg;
        snprintf(buf, 256, "%c%d attacks %c%d: roll(%d) + base_dmg(%d) = %d dmg",
                 actor_label, actor_num, target_label, target_num,
                 roll_val, strike_dmg, total_dmg);
        add_log(g, buf);
        snprintf(buf, 256, "%c%d: %d HP -> %d HP",
                 target_label, target_num, old_hp, target->hp);
        add_log(g, buf);
    }

    // check if target died
    if (target->hp <= 0) {
        target->hp = 0;
        target->alive = 0;
        if (target->team == TEAM_NPC) {
            g->kills++;
            snprintf(buf, 256, "KILL: %c%d eliminated %c%d!",
                     actor_label, actor_num, target_label, target_num);
            add_log(g, buf);
            // Weapon drop: 50% chance
            if ((rand() % 100) < 50) {
                WeaponId drop = static_cast<WeaponId>((rand() % 8) + 1);
                // Find a living player to offer the drop
                int p = first_living_player(g);
                if (p >= 0) {
                    CharacterState* pl = &g->players[p];
                    snprintf(buf, 256, "Weapon drop: %s offered to P%d",
                             weapon_def(drop).name, p);
                    add_log(g, buf);
                    // For simplicity, first living player auto-picks it up
                    // In a full implementation, the player would choose
                    if (!allocate_weapon(pl, drop)) {
                        swap_out_minimal(pl, drop);
                    }
                    snprintf(buf, 256, "P%d picked up %s",
                             p, weapon_def(drop).name);
                    add_log(g, buf);
                } else {
                    // No living players, enemy picks it up
                    int n = first_living_npc(g);
                    if (n >= 0) {
                        CharacterState* ne = &g->npcs[n];
                        if (!allocate_weapon(ne, drop)) {
                            swap_out_minimal(ne, drop);
                        }
                        snprintf(buf, 256, "Weapon drop: %s picked up by N%d",
                                 weapon_def(drop).name, n);
                        add_log(g, buf);
                    }
                }
            }
        }
    }

    actor->stamina = 0;
    actor->ready_epoch = 0;

    // Flip turn order after action: player acted → NPC's turn; NPC acted → player's turn
    if (a.actor_team == TEAM_PLAYER)
        g->turn_order = 1;  // now enemy's turn
    else
        g->turn_order = 0;  // now player's turn

    // 20% chance to stun target (delivered via signal)
    if ((rand() % 100) < 20 && target->alive) {
        target->stunned_until_epoch = now_epoch() + 3;
        if (target->team == TEAM_PLAYER && g->hip_pid > 0) kill(g->hip_pid, SIGUSR1);
        if (target->team == TEAM_NPC && g->asp_pid > 0) kill(g->asp_pid, SIGUSR1);
        snprintf(buf, 256, "%c%d stunned %c%d for 3s [SIGUSR1 sent]",
                 actor_label, actor_num, target_label, target_num);
        add_log(g, buf);
    }

    // random chance to spawn eclipse relic
    if (!g->eclipse_present && (rand() % 100) < 10) {
        g->artifacts[2].present = 1;
        g->eclipse_present = 1;
        add_log(g, "Eclipse Relic appeared");
    }
}

static time_t g_turn_start = 0;

void schedule_next_turn() {
    SchedulerMode mode = g->scheduler_mode;

    // alive, full stamina, not stunned
    auto ready = [](CharacterState& c, int now) -> bool {
        return c.alive && c.stamina >= c.max_stamina && now >= c.stunned_until_epoch;
    };

    bool found = false;
    int chosen_team = -1, chosen_id = -1;
    int now = now_epoch();

    switch (mode) {
        case SCHED_STAMINA: {
            // True FCFS: pick whichever entity reached full stamina earliest
            int best_epoch = 0x7fffffff;
            for (int i = 0; i < g->num_players; ++i) {
                if (ready(g->players[i], now) && g->players[i].ready_epoch < best_epoch) {
                    best_epoch = g->players[i].ready_epoch;
                    chosen_team = TEAM_PLAYER; chosen_id = i; found = true;
                }
            }
            for (int i = 0; i < g->num_npcs; ++i) {
                if (ready(g->npcs[i], now) && g->npcs[i].ready_epoch < best_epoch) {
                    best_epoch = g->npcs[i].ready_epoch;
                    chosen_team = TEAM_NPC; chosen_id = i; found = true;
                }
            }
            break;
        }

        case SCHED_MODE_RR: {
            // Round-robin: alternate player/NPC by turn_seq; skip dead/stunned
            TeamType rr_team = (g->turn_seq % 2 == 0) ? TEAM_PLAYER : TEAM_NPC;
            if (rr_team == TEAM_PLAYER) {
                for (int i = 0; i < g->num_players; ++i)
                    if (ready(g->players[i], now)) { chosen_team = TEAM_PLAYER; chosen_id = i; found = true; break; }
                if (!found)
                    for (int i = 0; i < g->num_npcs; ++i)
                        if (ready(g->npcs[i], now)) { chosen_team = TEAM_NPC; chosen_id = i; found = true; break; }
            } else {
                for (int i = 0; i < g->num_npcs; ++i)
                    if (ready(g->npcs[i], now)) { chosen_team = TEAM_NPC; chosen_id = i; found = true; break; }
                if (!found)
                    for (int i = 0; i < g->num_players; ++i)
                        if (ready(g->players[i], now)) { chosen_team = TEAM_PLAYER; chosen_id = i; found = true; break; }
            }
            break;
        }

        case SCHED_MODE_FIFO: {
            // Strict arrival order: lowest-index alive player first, then NPC
            for (int i = 0; i < g->num_players; ++i)
                if (ready(g->players[i], now)) { chosen_team = TEAM_PLAYER; chosen_id = i; found = true; break; }
            if (!found)
                for (int i = 0; i < g->num_npcs; ++i)
                    if (ready(g->npcs[i], now)) { chosen_team = TEAM_NPC; chosen_id = i; found = true; break; }
            break;
        }

        case SCHED_PRIORITY: {
            // Highest speed stat acts first; ties broken by team (players first)
            int best_speed = -1;
            TeamType best_team = TEAM_PLAYER;
            int best_id = -1;
            for (int i = 0; i < g->num_players; ++i) {
                if (ready(g->players[i], now) && g->players[i].speed > best_speed) {
                    best_speed = g->players[i].speed;
                    best_team = TEAM_PLAYER;
                    best_id = i;
                }
            }
            for (int i = 0; i < g->num_npcs; ++i) {
                if (ready(g->npcs[i], now) && g->npcs[i].speed > best_speed) {
                    best_speed = g->npcs[i].speed;
                    best_team = TEAM_NPC;
                    best_id = i;
                }
            }
            if (best_speed >= 0) {
                chosen_team = best_team;
                chosen_id = best_id;
                found = true;
            }
            break;
        }
    }

    if (found) {
        g->active_team = chosen_team;
        g->active_id = chosen_id;
        g->turn_seq++;
        g->pending.ready = 0;
        g_turn_start = now_epoch();
        sem_post(&g->turn_sem); // wake any thread waiting on turn_sem
        pthread_cond_broadcast(&g->turn_cv);
        return;
    }

    // nobody ready yet, wait for stamina to fill
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;

    while (!found && g->running) {
        int rc = pthread_cond_timedwait(&g->turn_cv, &g->mtx, &ts);
        if (rc == ETIMEDOUT) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
        }

        now = now_epoch();
        switch (mode) {
            case SCHED_STAMINA:
            case SCHED_MODE_FIFO:
                for (int i = 0; i < g->num_players && !found; ++i)
                    if (ready(g->players[i], now)) { chosen_team = TEAM_PLAYER; chosen_id = i; found = true; }
                for (int i = 0; i < g->num_npcs && !found; ++i)
                    if (ready(g->npcs[i], now)) { chosen_team = TEAM_NPC; chosen_id = i; found = true; }
                break;
            case SCHED_MODE_RR:
            case SCHED_PRIORITY:
                for (int i = 0; i < g->num_players && !found; ++i)
                    if (ready(g->players[i], now)) { chosen_team = TEAM_PLAYER; chosen_id = i; found = true; }
                for (int i = 0; i < g->num_npcs && !found; ++i)
                    if (ready(g->npcs[i], now)) { chosen_team = TEAM_NPC; chosen_id = i; found = true; }
                break;
        }
    }

    if (found && g->running) {
        g->active_team = chosen_team;
        g->active_id = chosen_id;
        g->turn_seq++;
        g->pending.ready = 0;
        g_turn_start = now_epoch();
        pthread_cond_broadcast(&g->turn_cv);
    }
}

// background thread: adds speed to stamina every second
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
            auto& p = g->players[i];
            if (!p.alive) continue;
            if (now < p.stunned_until_epoch) continue;
            bool was_full = (p.stamina >= p.max_stamina);
            p.stamina = CRMIN(p.max_stamina, p.stamina + p.speed);
            if (!was_full && p.stamina >= p.max_stamina)
                p.ready_epoch = now;  // record when stamina first filled
        }
        for (int i = 0; i < g->num_npcs; ++i) {
            auto& n = g->npcs[i];
            if (!n.alive) continue;
            if (now < n.stunned_until_epoch) continue;
            bool was_full = (n.stamina >= n.max_stamina);
            n.stamina = CRMIN(n.max_stamina, n.stamina + n.speed);
            if (!was_full && n.stamina >= n.max_stamina)
                n.ready_epoch = now;
        }

        pthread_cond_broadcast(&g->turn_cv);
        pthread_mutex_unlock(&g->mtx);

        // Submit deadlock check to thread pool every tick
        if (g_thread_pool) {
            g_thread_pool->submit(deadlock_checker_task, nullptr);
        }
    }
}

void pause_all_child_processes() {
    if (g->hip_pid > 0) kill(g->hip_pid, SIGSTOP);
    if (g->asp_pid > 0) kill(g->asp_pid, SIGSTOP);
    add_log(g, "[SIGSTOP] HIP and ASP paused");
}

void resume_all_child_processes() {
    if (g->hip_pid > 0) kill(g->hip_pid, SIGCONT);
    if (g->asp_pid > 0) kill(g->asp_pid, SIGCONT);
    add_log(g, "[SIGCONT] HIP and ASP resumed");
}

// deadlock monitor inline function for the background thread
static void* deadlock_monitor_inline(void*) {
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
            switch (g->deadlock_strategy) {
                case DETECT_ONLY:
                    b.holder_team = -1; b.holder_id = -1;
                    add_log(g, "Deadlock [DETECT]: forced Lunar Blade release");
                    break;
                case NO_HOLD_WAIT: {
                    if (a.holder_team != -1) {
                        CharacterState* ah = get_character(g, a.holder_team, a.holder_id);
                        if (ah) force_release_artifact(ah);
                    }
                    if (b.holder_team != -1) {
                        CharacterState* bh = get_character(g, b.holder_team, b.holder_id);
                        if (bh) force_release_artifact(bh);
                    }
                    add_log(g, "Deadlock [NO_HOLD_WAIT]: artifacts force-released");
                    break;
                }
                case PREEMPT:
                    preempt_lower_priority(a, b);
                    add_log(g, "Deadlock [PREEMPT]: lower-priority holder preempted");
                    break;
            }
        }
        pthread_mutex_unlock(&g->mtx);
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc >= 3) {
        g_roll_arg_str = argv[1];
        g_party_arg = atoi(argv[2]);
        if (g_party_arg < 1 || g_party_arg > 4) g_party_arg = 0;
    }

    signal(SIGALRM, sigalrm_handler);
    signal(SIGTERM, sigterm_handler);

    cout << "=== CHRONO RIFT - SFML Arbiter ===\n";
    cout << "Modes: [1]Combat [2]Scheduler [3]Hybrid | ESC: Exit\n";
    cout << "Scheduling: STAMINA | RR | FIFO | PRIORITY (switch in UI)\n";
    cout << "Deadlock: DETECT | NO_HOLD_WAIT | PREEMPT (switch in UI)\n\n";

    // Initialize memory arena for fragmentation tracking
    arena_init(&g_mem_arena, 65536);  // 64KB arena

    g = map_shared(true);
    if (!g) return 1;

    init_state(g);
    g->arbiter_pid = getpid();
    g_turn_start = now_epoch();

    // Initialize thread pool with 4 detached worker threads
    g_thread_pool = new ThreadPool();
    if (!g_thread_pool->init()) {
        cerr << "[WARN] Thread pool init failed - using inline deadlock monitor\n";
        delete g_thread_pool;
        g_thread_pool = nullptr;
    } else {
        cout << "[OK] Thread pool started: " << POOL_SIZE << " detached workers\n";
    }

    // start background threads
    pthread_t mon, ren, sch;
    pthread_create(&mon, nullptr, deadlock_monitor_inline, nullptr);
    pthread_create(&ren, nullptr, render_thread, nullptr);
    pthread_create(&sch, nullptr, turn_scheduler_thread, nullptr);

    pthread_mutex_lock(&g->mtx);
    add_log(g, "SFML Arbiter started");
    char mode_buf[128];
    snprintf(mode_buf, sizeof(mode_buf), "Scheduler: STAMINA | Quantum: %dms",
             g->quantum_ms);
    add_log(g, mode_buf);

    // Log initial stats
    char stats_buf[256];
    snprintf(stats_buf, sizeof(stats_buf),
             "Roll: %d | Players: %d (SPD=%d, DMG=%d) | NPCs: %d (DMG=%d)",
             g->roll_seed, g->num_players,
             g->players[0].speed, g->players[0].dmg,
             g->num_npcs, g->npcs[0].dmg);
    add_log(g, stats_buf);

    // main game loop — keyboard-driven: wait for player/enemy keyboard action
    while (g->running) {
        if (terminate_requested) {
            g->running = 0;
            g->win = 0;
            add_log(g, "Quit condition met (SIGTERM)");
            break;
        }

        // check win/lose
        int alive_players = 0;
        for (int i = 0; i < g->num_players; ++i) alive_players += g->players[i].alive;
        if (alive_players == 0) {
            g->running = 0;
            g->win = 0;
            add_log(g, "Lose condition met");
            break;
        }
        if (g->kills >= g->num_npcs) {
            g->running = 0;
            g->win = 1;
            add_log(g, "Win condition met");
            break;
        }

        // Set active team based on turn_order (keyboard controls both sides)
        if (g->turn_order == 0) {
            // Player's turn: pick the first living player as default active
            g->active_team = TEAM_PLAYER;
            for (int i = 0; i < g->num_players; ++i) {
                if (g->players[i].alive) { g->active_id = i; break; }
            }
        } else {
            // Enemy's turn: pick first living NPC as default active
            g->active_team = TEAM_NPC;
            for (int i = 0; i < g->num_npcs; ++i) {
                if (g->npcs[i].alive) { g->active_id = i; break; }
            }
        }
        g->pending.ready = 0;

        // Wait for the keyboard action (Q/E for player, U/O for enemy)
        // Player turn: wait indefinitely
        // Enemy turn: wait up to 30s then auto-attack
        if (g->turn_order == 0) {
            // Player's turn — wait indefinitely for Q/E
            while (g->running && !g->pending.ready) {
                pthread_cond_wait(&g->action_cv, &g->mtx);
                if (terminate_requested) {
                    g->running = 0; g->win = 0;
                    add_log(g, "Quit condition met (SIGTERM)");
                    break;
                }
            }
            if (!g->running) break;
            if (g->pending.ready) {
                apply_action(g->pending);
                g->pending.ready = 0;
            }
        } else {
            // Enemy's turn — wait up to 30s for U/O, then auto-attack
            timespec ts{};
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 30;
            int rc = 0;
            while (g->running && !g->pending.ready && rc != ETIMEDOUT) {
                rc = pthread_cond_timedwait(&g->action_cv, &g->mtx, &ts);
            }
            if (terminate_requested) {
                g->running = 0; g->win = 0;
                add_log(g, "Quit condition met (SIGTERM)");
                break;
            }
            if (!g->running) break;
            if (!g->pending.ready) {
                // 30s timeout: auto-attack the currently selected player in the UI
                // Fall back to first living player if selected target is dead
                int target_player = g->selected_player_id;
                if (target_player < 0 || target_player >= g->num_players
                    || !g->players[target_player].alive) {
                    target_player = -1;
                    for (int pi = 0; pi < g->num_players; ++pi) {
                        if (g->players[pi].alive) { target_player = pi; break; }
                    }
                }
                if (target_player >= 0) {
                    PendingAction npc_atk{};
                    npc_atk.action      = ACT_STRIKE;
                    npc_atk.actor_team  = TEAM_NPC;
                    npc_atk.actor_id    = g->active_id;
                    npc_atk.target_team = TEAM_PLAYER;
                    npc_atk.target_id   = target_player;
                    npc_atk.ready       = 1;
                    apply_action(npc_atk);
                    add_log(g, "Enemy auto-attacked (30s timeout)!");
                } else {
                    g->turn_order = 0; // no living players — flip back
                }
            } else {
                apply_action(g->pending);
                g->pending.ready = 0;
            }
        }

        // Periodically track memory fragmentation (every 5 turns)
        static int turn_counter = 0;
        turn_counter++;
        if (turn_counter % 5 == 0) {
            void* track = arena_alloc(&g_mem_arena, sizeof(MemArena));
            if (track) arena_free(&g_mem_arena, track);
            log_arena_fragmentation();
        }
    }

    // wake up any waiting threads so they can exit
    pthread_cond_broadcast(&g->turn_cv);
    pthread_cond_broadcast(&g->action_cv);
    sem_post(&g->action_sem); // unblock any sem_wait
    pthread_mutex_unlock(&g->mtx);

    // Shutdown thread pool
    if (g_thread_pool) {
        g_thread_pool->shutdown();
        delete g_thread_pool;
        g_thread_pool = nullptr;
    }

    pthread_join(mon, nullptr);
    pthread_join(ren, nullptr);
    pthread_join(sch, nullptr);

    // Log final arena stats
    char final_buf[256];
    arena_stats(&g_mem_arena, final_buf, sizeof(final_buf));
    cout << "[Arena] " << final_buf << endl;
    arena_destroy(&g_mem_arena);

    cleanup_shared(g, true);

    cout << "\n[OK] SFML Arbiter exited cleanly." << endl;
    return 0;
}
