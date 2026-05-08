/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  SHARED.HPP — Shared Data Structures for Chrono Rift                   ║
 * ║  OS Concepts: Shared Memory IPC, Data Structures                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * This file defines all data structures that live in POSIX shared memory.
 * They are accessed by three processes: Arbiter (arbiter.cpp), HIP (hip.cpp),
 * and ASP (asp.cpp) through the /dev/shm/chrono_rift_shm region.
 *
 * OS Concepts:
 *   - SHARED MEMORY IPC: All structs below are memory-mapped and accessible
 *     across process boundaries. No serialization needed.
 *   - PROCESS-SHARED SYNCHRONIZATION: pthread_mutex_t and pthread_cond_t
 *     must be initialized with PTHREAD_PROCESS_SHARED attribute (done in
 *     arbiter.cpp:init_state()).
 *   - DATA LAYOUT: Structures use fixed-size types (int, char[]) to ensure
 *     consistent layout across processes on the same architecture.
 *
 * Key Structures:
 *   - SharedState: Root structure containing all game state
 *   - CharacterState: Per-character data (HP, stamina, inventory, storage)
 *   - ArtifactState: Artifact metadata (holder, waiting queue)
 *   - PendingAction: In-flight action from HIP/ASP to Arbiter
 */

#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

// ═══════════════════════════════════════════════════════════════════════════
// SHARED MEMORY NAME — POSIX shared memory object path
// ═══════════════════════════════════════════════════════════════════════════

constexpr const char* SHM_NAME = "/chrono_rift_shm";

// ═══════════════════════════════════════════════════════════════════════════
// SYSTEM LIMITS — Array sizes for fixed-size structures in shared memory
// ═══════════════════════════════════════════════════════════════════════════

constexpr int MAX_PLAYERS   = 4;   // Max human players
constexpr int MAX_NPCS      = 9;   // Max NPC enemies
constexpr int INVENTORY_SLOTS = 20; // Inventory slots per character
constexpr int MAX_STORAGE   = 32; // Storage slots per character
constexpr int MAX_LOG       = 64;  // Circular log buffer size
constexpr int MAX_LOG_LEN   = 128; // Max characters per log entry
constexpr int MAX_WAITING   = 16;  // Max waiters on a single artifact

// ═══════════════════════════════════════════════════════════════════════════
// ENUMERATIONS
// ═══════════════════════════════════════════════════════════════════════════

// Team classification — used to route actions and identify character type.
enum TeamType { TEAM_PLAYER = 0, TEAM_NPC = 1 };

// All possible player/NPC actions in the game.
// OS Concept: Action types map to scheduling decisions.
enum ActionType {
    ACT_NONE     = 0,
    ACT_STRIKE   = 1,  // Basic melee attack
    ACT_EXHAUST  = 2,  // Drain target stamina
    ACT_USE_WEAPON = 3,// Equip and use a weapon
    ACT_SWAP_IN  = 4,  // Retrieve weapon from storage
    ACT_HEAL     = 5,  // Restore 10% HP
    ACT_SKIP     = 6,  // Skip turn, retain 50% stamina
    ACT_ULTIMATE = 7,  // Special (requires Solar Core + Lunar Blade)
    ACT_QUIT     = 8   // Terminate game
};

// Weapon definitions — each weapon occupies a fixed number of inventory slots.
// OS Concept: Resource management — slot allocation is a packing problem.
enum WeaponId {
    W_NONE         = 0,
    W_SOLAR_CORE   = 1,  // Artifact: 10 slots, 95 dmg
    W_LUNAR_BLADE  = 2,  // Artifact: 10 slots, 90 dmg
    W_IRON_HALBERD = 3,  // 7 slots, 55 dmg
    W_VENOM_DAGGER = 4,  // 4 slots, 30 dmg
    W_THUNDERSTAFF = 5,  // 6 slots, 50 dmg
    W_OBSIDIAN_AXE = 6,  // 5 slots, 45 dmg
    W_FROSTBOW     = 7,  // 6 slots, 48 dmg
    W_SPLINTER_STICK = 8,// 2 slots, 12 dmg
    W_ECLIPSE_RELIC = 9  // Artifact: 3 slots, 60 dmg (spawns randomly)
};

// ═══════════════════════════════════════════════════════════════════════════
// WEAPON DEFINITIONS — Static table for weapon metadata
// ═══════════════════════════════════════════════════════════════════════════

// OS Concept: Static resource table — weapons are shared global constants.
struct WeaponDef {
    WeaponId id;
    const char* name;
    int slots;   // Inventory space required
    int damage;  // Damage dealt per hit
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

// ═══════════════════════════════════════════════════════════════════════════
// CHARACTER STATE — Per-player/NPC game entity
// ═══════════════════════════════════════════════════════════════════════════

// OS Concept: Entity state management — each character is a schedulable unit.
// Fields are plain-old-data (POD) for safe cross-process access.
struct CharacterState {
    int alive;                  // 1 = alive, 0 = dead
    int id;                     // Character index within team
    TeamType team;              // TEAM_PLAYER or TEAM_NPC
    int hp;                     // Current health points
    int max_hp;                 // Maximum health points
    int dmg;                    // Base damage per strike
    int speed;                  // Stamina regeneration rate (per second)
    int stamina;                // Current stamina (action requires >= max_stamina)
    int max_stamina;            // Stamina needed to take a turn
    int stunned_until_epoch;     // Unix timestamp when stun expires (in-game stun)
    int inventory[INVENTORY_SLOTS]; // Contiguous weapon slots (0 = empty, W_ID = weapon)
    int storage[MAX_STORAGE];    // Stored weapons (for SWAP_IN action)
    int storage_count;           // Number of weapons in storage
};

// ═══════════════════════════════════════════════════════════════════════════
// ARTIFACT STATE — Shared resource with waiting queue
// ═══════════════════════════════════════════════════════════════════════════

// OS Concept: Shared resource with lock acquisition queue.
// Implements a simple spin-like queue for artifact waiters.
struct ArtifactState {
    WeaponId id;                // Which artifact (Solar Core, Lunar Blade, Eclipse Relic)
    int present;                // 1 if spawned, 0 if hidden (Eclipse Relic only)
    int holder_team;            // TEAM_PLAYER or TEAM_NPC, -1 if free
    int holder_id;              // Character index holding the artifact, -1 if free
    int waiting_count;          // Number of characters waiting for this artifact
    int waiters_team[MAX_WAITING]; // Team of each waiting character
    int waiters_id[MAX_WAITING];   // ID of each waiting character
};

// ═══════════════════════════════════════════════════════════════════════════
// PENDING ACTION — In-flight action from HIP/ASP to Arbiter
// ═══════════════════════════════════════════════════════════════════════════

// OS Concept: Inter-process communication message — producer-consumer pattern.
// HIP/ASP write this, Arbiter reads it. ready flag gates the consumer.
struct PendingAction {
    int ready;              // 1 = valid action pending, 0 = no action
    ActionType action;      // Which action to perform
    int actor_team;         // Team of the acting character
    int actor_id;           // ID of the acting character
    int target_team;        // Team of the target character
    int target_id;          // ID of the target character
    WeaponId weapon;        // Which weapon to use (for ACT_USE_WEAPON)
};

// ═══════════════════════════════════════════════════════════════════════════
// SHARED STATE — Root structure mapped into all processes' address spaces
// ═══════════════════════════════════════════════════════════════════════════
//
// OS Concepts:
//   - SHARED MEMORY IPC: This entire struct is memory-mapped via shm_open/mmap
//     and visible to arbiter, hip, and asp processes simultaneously.
//   - SYNCHRONIZATION: mtx, turn_cv, action_cv are process-shared primitives.
//     They protect ALL fields below for safe concurrent access.
//   - CONDITION VARIABLES: turn_cv signals stamina changes / turn assignments.
//     action_cv signals when HIP/ASP has submitted a pending action.

struct SharedState {
    // Synchronization primitives (OS Concept: Process-shared mutex/condvar)
    pthread_mutex_t mtx;       // Protects all fields below
    pthread_cond_t turn_cv;    // Signals: stamina updated, turn changed
    pthread_cond_t action_cv;   // Signals: pending action ready

    // Initialization flag — HIP/ASP threads spin on this until arbiter is ready
    int initialized;

    // Game state
    int running;               // 1 = game active, 0 = game over
    int roll_seed;             // Seed for RNG (deterministic game generation)

    // Character counts
    int num_players;           // Party size (1-4)
    int num_npcs;              // Number of NPC enemies (2-9, derived from roll)

    // Character arrays — these are the primary data structures
    // OS Concept: Process isolation — each process sees the same arrays
    CharacterState players[MAX_PLAYERS];
    CharacterState npcs[MAX_NPCS];

    // Current turn assignment — which character is active
    int active_team;           // TEAM_PLAYER or TEAM_NPC
    int active_id;             // Character index within team
    int turn_seq;              // Turn counter (increments each turn)

    // Pending action from HIP/ASP — producer-consumer pattern
    PendingAction pending;

    // Artifacts — shared resources with potential for deadlock
    ArtifactState artifacts[3]; // Solar Core, Lunar Blade, Eclipse Relic
    int eclipse_present;       // 0 = hidden, 1 = spawned

    // Win/lose tracking
    int kills;                 // NPC kills by player team (win at 10)
    int win;                   // 1 = victory, 0 = defeat

    // Circular log buffer — recent game events
    int log_head;              // Index of next write position
    char logs[MAX_LOG][MAX_LOG_LEN];

    // Process IDs — used for inter-process signal delivery
    // OS Concept: Signal delivery across processes (kill(pid, SIGUSR1))
    pid_t arbiter_pid;         // For HIP/ASP to send signals
    pid_t hip_pid;
    pid_t asp_pid;
};