// shared.hpp - all structs that live in shared memory between the 3 processes

#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

// shared memory object name
constexpr const char* SHM_NAME = "/chrono_rift_shm";

// limits for fixed-size arrays
constexpr int MAX_PLAYERS   = 4;
constexpr int MAX_NPCS      = 9;
constexpr int INVENTORY_SLOTS = 20;
constexpr int MAX_STORAGE   = 32;
constexpr int MAX_LOG       = 64;
constexpr int MAX_LOG_LEN   = 256;
constexpr int MAX_WAITING   = 16;

// teams
enum TeamType { TEAM_PLAYER = 0, TEAM_NPC = 1 };

// scheduling modes selectable at runtime
enum SchedulerMode {
    SCHED_STAMINA   = 0,   // existing stamina-based scheduling
    SCHED_MODE_RR   = 1,   // round-robin, alternating player/NPC
    SCHED_MODE_FIFO = 2,   // strict arrival order by index
    SCHED_PRIORITY  = 3    // highest speed acts first
};

// different ways to handle deadlock, switchable at runtime
enum DeadlockStrategy {
    DETECT_ONLY = 0,      // detect circular wait and force release (existing)
    NO_HOLD_WAIT = 1,      // force release of held artifact before acquiring new
    PREEMPT = 2           // preempt lower-priority artifact holder
};

// all possible actions a character can take
enum ActionType {
    ACT_NONE     = 0,
    ACT_STRIKE   = 1,
    ACT_EXHAUST  = 2,
    ACT_USE_WEAPON = 3,
    ACT_SWAP_IN  = 4,
    ACT_HEAL     = 5,
    ACT_SKIP     = 6,
    ACT_ULTIMATE = 7,
    ACT_QUIT     = 8
};

// weapon ids - each weapon takes a certain number of inventory slots
enum WeaponId {
    W_NONE         = 0,
    W_SOLAR_CORE   = 1,  // 10 slots, 95 dmg (artifact)
    W_LUNAR_BLADE  = 2,  // 10 slots, 90 dmg (artifact)
    W_IRON_HALBERD = 3,  // 7 slots, 55 dmg
    W_VENOM_DAGGER = 4,  // 4 slots, 30 dmg
    W_THUNDERSTAFF = 5,  // 6 slots, 50 dmg
    W_OBSIDIAN_AXE = 6,  // 5 slots, 45 dmg
    W_FROSTBOW     = 7,  // 6 slots, 48 dmg
    W_SPLINTER_STICK = 8,// 2 slots, 12 dmg
    W_ECLIPSE_RELIC = 9  // 3 slots, 60 dmg (dynamic artifact)
};

// weapon stats table
struct WeaponDef {
    WeaponId id;
    const char* name;
    int slots;
    int damage;
};

constexpr WeaponDef WEAPONS[] = {
    {W_NONE,         "None",           0,   0},
    {W_SOLAR_CORE,   "Solar Core",    10,  95},
    {W_LUNAR_BLADE,  "Lunar Blade",   10,  90},
    {W_IRON_HALBERD, "Iron Halberd",   7,  55},
    {W_VENOM_DAGGER, "Venom Dagger",   4,  30},
    {W_THUNDERSTAFF, "Thunderstaff",   6,  50},
    {W_OBSIDIAN_AXE, "Obsidian Axe",   5,  45},
    {W_FROSTBOW,     "Frostbow",       6,  48},
    {W_SPLINTER_STICK,"Splinter Stick", 2,  12},
    {W_ECLIPSE_RELIC,"Eclipse Relic",   3,  60}
};

// per-character state (player or npc)
struct CharacterState {
    int alive;
    int id;
    TeamType team;
    int hp;
    int max_hp;
    int dmg;
    int speed;                  // stamina regen rate per second
    int stamina;
    int max_stamina;
    int stunned_until_epoch;    // unix time when stun expires
    int ready_epoch;            // unix time when stamina first became full (for FCFS scheduling)
    int inventory[INVENTORY_SLOTS]; // contiguous weapon slots (0=empty)
    int storage[MAX_STORAGE];   // long-term storage
    int storage_count;
};

// artifact state - tracks who holds each global artifact
struct ArtifactState {
    WeaponId id;
    int present;            // 1 if spawned (eclipse relic starts at 0)
    int holder_team;        // -1 if free
    int holder_id;          // -1 if free
    int waiting_count;      // characters waiting for this artifact
    int waiters_team[MAX_WAITING];
    int waiters_id[MAX_WAITING];
};

// action submitted by hip/asp, arbiter picks it up when ready==1
struct PendingAction {
    int ready;
    ActionType action;
    int actor_team;
    int actor_id;
    int target_team;
    int target_id;
    WeaponId weapon;
};

// main shared state struct - mapped into all processes
struct SharedState {
    // synchronization (process-shared mutex and condvars)
    pthread_mutex_t mtx;
    pthread_cond_t turn_cv;     // signals stamina updates and turn changes
    pthread_cond_t action_cv;   // signals when hip/asp submits an action

    // semaphores for syncing actions between processes
    sem_t action_sem;   // posted when a new action is ready (hip/asp -> arbiter)
    sem_t turn_sem;     // posted when stamina is replenished
    sem_t log_sem;      // binary semaphore for thread-safe log writes

    int initialized;            // hip/asp wait on this before starting

    // scheduling configuration
    SchedulerMode scheduler_mode;   // current scheduling mode
    int quantum_ms;                  // round-robin quantum in milliseconds

    // deadlock configuration
    DeadlockStrategy deadlock_strategy; // active deadlock prevention strategy

    // game state
    int running;
    int roll_seed;

    // character arrays
    int num_players;
    int num_npcs;
    CharacterState players[MAX_PLAYERS];
    CharacterState npcs[MAX_NPCS];

    // current turn - who's active right now
    int active_team;
    int active_id;
    int turn_seq;

    // pending action from hip/asp
    PendingAction pending;

    // artifacts (solar core, lunar blade, eclipse relic)
    ArtifactState artifacts[3];
    int eclipse_present;

    // alternating turn order: 0 = player's turn to act, 1 = NPC's turn to act
    int turn_order;

    // currently selected player in the UI cage (written by Visualizer, read by arbiter for targeting)
    int selected_player_id;

    // win/lose tracking
    int kills;                  // player team kills (win at 10)
    int win;

    // circular log buffer for the UI
    int log_head;
    char logs[MAX_LOG][MAX_LOG_LEN];

    // process pids for signal delivery between processes
    pid_t arbiter_pid;
    pid_t hip_pid;
    pid_t asp_pid;
};
