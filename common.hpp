/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  COMMON.HPP — Utility Functions for Chrono Rift                        ║
 * ║  OS Concepts: Shared Memory IPC, Utility Functions                       ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * Utility functions for shared memory operations and weapon management.
 * Included by all three processes (arbiter, hip, asp).
 *
 * OS Concepts:
 *   - Shared Memory IPC: map_shared() wraps shm_open + mmap for creation/attachment
 *   - Synchronization: add_log() writes to the circular log buffer atomically
 *   - Resource Management: Weapon inventory allocation with contiguous slot packing
 *
 * Usage:
 *   - Arbiter (creator): SharedState* s = map_shared(true);  // creates region
 *   - HIP/ASP (attach):  SharedState* s = map_shared(false); // attaches
 */

#pragma once

#include "shared.hpp"

#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// ═══════════════════════════════════════════════════════════════════════════
// TIME — Unix epoch time for stun scheduling and timeouts

// Returns current Unix timestamp as int.
// Used for stun expiration checks and condition variable timeouts.
inline int now_epoch() {
    return static_cast<int>(std::time(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════════
// WEAPON DEFINITION LOOKUP

// Returns the WeaponDef for a given WeaponId.
// OS Concept: Static dispatch on enum — weapons are global constants.
inline WeaponDef weapon_def(WeaponId id) {
    for (auto& w : WEAPONS) {
        if (w.id == id) return w;
    }
    return WEAPONS[0];
}

// ═══════════════════════════════════════════════════════════════════════════
// SHARED MEMORY IPC — POSIX shared memory attachment/creation
// ═══════════════════════════════════════════════════════════════════════════
//
// OS Concepts: shm_open + mmap for POSIX shared memory IPC.
//
// map_shared(true)  — Arbiter: Creates /dev/shm/chrono_rift_shm, truncates to
//                      sizeof(SharedState), maps into own address space.
//
// map_shared(false) — HIP/ASP: Opens existing region, maps into own address
//                      space. Returns nullptr if region doesn't exist yet.
//
// The region is created at /dev/shm/chrono_rift_shm (visible as /dev/shm/).
// On Linux, /dev/shm is a tmpfs mounted at /dev/shm.
//
// Memory layout: SharedState struct is mapped at a fixed virtual address
// in all three processes, allowing direct pointer access across boundaries.

inline SharedState* map_shared(bool create) {
    // Unlink first if creating — ensures a fresh state every launch.
    if (create) shm_unlink(SHM_NAME);

    int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
    int fd = shm_open(SHM_NAME, flags, 0666);
    if (fd < 0) {
        if (create || errno != ENOENT) {
            perror("shm_open");
        }
        return nullptr;
    }

    // Size the shared memory object to exactly sizeof(SharedState)
    if (create) {
        if (ftruncate(fd, sizeof(SharedState)) != 0) {
            perror("ftruncate");
            close(fd);
            return nullptr;
        }
    }

    // Map into process address space with read-write, shared access
    void* mem = mmap(nullptr, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd); // fd no longer needed after mmap
    if (mem == MAP_FAILED) {
        perror("mmap");
        return nullptr;
    }
    return static_cast<SharedState*>(mem);
}

// ═══════════════════════════════════════════════════════════════════════════
// LOGGING — Thread-safe circular log buffer write
// ═══════════════════════════════════════════════════════════════════════════

// Writes a message to the circular log buffer.
// OS Concept: Atomic string write — snprintf + index update is the write point.
// The caller holds g->mtx when calling this (ensuring single-writer).
inline void add_log(SharedState* s, const std::string& msg) {
    std::snprintf(s->logs[s->log_head], MAX_LOG_LEN, "%s", msg.c_str());
    s->log_head = (s->log_head + 1) % MAX_LOG;
}

// ═══════════════════════════════════════════════════════════════════════════
// CHARACTER LOOKUP — Resolve team+id to CharacterState pointer
// ═══════════════════════════════════════════════════════════════════════════

// Returns pointer to the character for given team and id.
// OS Concept: Array dispatch — players[0..num_players-1], npcs[0..num_npcs-1].
// Returns nullptr if out of bounds (caller must check).
inline CharacterState* get_character(SharedState* s, int team, int id) {
    if (team == TEAM_PLAYER) {
        if (id < 0 || id >= s->num_players) return nullptr;
        return &s->players[id];
    }
    if (id < 0 || id >= s->num_npcs) return nullptr;
    return &s->npcs[id];
}

// ═══════════════════════════════════════════════════════════════════════════
// FIRST LIVING — Find first alive character of a team

inline int first_living_player(SharedState* s) {
    for (int i = 0; i < s->num_players; ++i) if (s->players[i].alive) return i;
    return -1;
}

inline int first_living_npc(SharedState* s) {
    for (int i = 0; i < s->num_npcs; ++i) if (s->npcs[i].alive) return i;
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════════
// WEAPON MANAGEMENT — Inventory slot allocation (OS Concept: Packing)
// ═══════════════════════════════════════════════════════════════════════════

// Checks if a character has a weapon in their inventory.
inline bool has_weapon(CharacterState* c, WeaponId id) {
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        if (c->inventory[i] == static_cast<int>(id)) return true;
    }
    return false;
}

// Removes all slots occupied by a given weapon from inventory.
inline void clear_weapon_from_inventory(CharacterState* c, WeaponId id) {
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        if (c->inventory[i] == static_cast<int>(id)) c->inventory[i] = 0;
    }
}

// Allocates consecutive inventory slots for a weapon.
// OS Concept: First-fit contiguous allocation — finds the first run of
// free slots large enough to hold the weapon.
// Returns true on success, false if not enough contiguous space.
inline bool allocate_weapon(CharacterState* c, WeaponId id) {
    auto w = weapon_def(id);
    for (int i = 0; i + w.slots <= INVENTORY_SLOTS; ++i) {
        bool ok = true;
        for (int j = i; j < i + w.slots; ++j) if (c->inventory[j] != 0) ok = false;
        if (ok) {
            for (int j = i; j < i + w.slots; ++j) c->inventory[j] = static_cast<int>(id);
            return true;
        }
    }
    return false;
}

// Swaps out weapons to make room for a needed weapon.
// Strategy: Try direct allocation, then evict weapons one at a time.
// OS Concept: Memory management with eviction — LRU-style weapon eviction.
// Evicted weapons go to storage, which is checked by SWAP_IN action.
inline bool swap_out_minimal(CharacterState* c, WeaponId need) {
    auto try_after_clear = [&]() {
        return allocate_weapon(c, need);
    };

    if (try_after_clear()) return true;

    // Find unique weapons in inventory
    bool seen[W_ECLIPSE_RELIC + 1] = {false};
    WeaponId unique[INVENTORY_SLOTS];
    int unique_count = 0;

    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        int wid = c->inventory[i];
        if (wid <= 0 || wid > W_ECLIPSE_RELIC) continue;
        WeaponId id = static_cast<WeaponId>(wid);
        if (!seen[id]) {
            seen[id] = true;
            unique[unique_count++] = id;
        }
    }

    // Evict weapons one at a time until allocation succeeds
    for (int i = 0; i < unique_count; ++i) {
        WeaponId id = unique[i];
        clear_weapon_from_inventory(c, id);
        if (c->storage_count < MAX_STORAGE) c->storage[c->storage_count++] = static_cast<int>(id);
        if (try_after_clear()) return true;
    }

    return false;
}