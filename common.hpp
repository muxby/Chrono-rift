// common.hpp - shared helpers used by all three processes

#pragma once

#include "shared.hpp"

#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// current unix timestamp (for stun expiry checks)
inline int now_epoch() {
    return static_cast<int>(std::time(nullptr));
}

// lookup weapon definition by id
inline WeaponDef weapon_def(WeaponId id) {
    for (int i = 0; i < 10; ++i) {
        if (WEAPONS[i].id == id) return WEAPONS[i];
    }
    return WEAPONS[0];
}

// shared memory helpers (System V)

static const int SHM_KEY_ID = 0x43; // 'C' for Chrono

// create=true for arbiter, false for hip/asp to attach
inline SharedState* map_shared(bool create) {
    key_t key = ftok("/tmp", SHM_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return nullptr;
    }

    int shmid;
    if (create) {
        // clear old segment if it exists
        shmid = shmget(key, sizeof(SharedState), 0666);
        if (shmid >= 0) shmctl(shmid, IPC_RMID, nullptr);
        shmid = shmget(key, sizeof(SharedState), IPC_CREAT | 0666);
    } else {
        shmid = shmget(key, sizeof(SharedState), 0666);
    }

    if (shmid < 0) {
        perror("shmget");
        return nullptr;
    }

    void* mem = shmat(shmid, nullptr, 0);
    if (mem == (void*)-1) {
        perror("shmat");
        return nullptr;
    }
    return static_cast<SharedState*>(mem);
}

// detach and optionally destroy shared memory
inline void cleanup_shared(SharedState* s, bool destroy) {
    if (destroy && s != nullptr) {
        // semaphores must be destroyed before we detach, not after
        sem_destroy(&s->action_sem);
        sem_destroy(&s->turn_sem);
        sem_destroy(&s->log_sem);

        key_t key = ftok("/tmp", SHM_KEY_ID);
        int shmid = shmget(key, sizeof(SharedState), 0666);
        if (shmid >= 0) {
            shmctl(shmid, IPC_RMID, nullptr);
        }
    }
    // Now safe to detach
    shmdt(s);
}

// write to circular log buffer (lock before calling this)
inline void add_log(SharedState* s, const char* msg) {
    std::snprintf(s->logs[s->log_head], MAX_LOG_LEN, "%s", msg);
    s->log_head = (s->log_head + 1) % MAX_LOG;
}

// overload so we can pass std::string directly
inline void add_log(SharedState* s, const std::string& msg) {
    add_log(s, msg.c_str());
}

// character lookup

inline CharacterState* get_character(SharedState* s, int team, int id) {
    if (team == TEAM_PLAYER) {
        if (id < 0 || id >= s->num_players) return nullptr;
        return &s->players[id];
    }
    if (id < 0 || id >= s->num_npcs) return nullptr;
    return &s->npcs[id];
}

inline int first_living_player(SharedState* s) {
    for (int i = 0; i < s->num_players; ++i) if (s->players[i].alive) return i;
    return -1;
}

inline int first_living_npc(SharedState* s) {
    for (int i = 0; i < s->num_npcs; ++i) if (s->npcs[i].alive) return i;
    return -1;
}

// weapon inventory helpers

// check if character has a weapon in inventory
inline bool has_weapon(CharacterState* c, WeaponId id) {
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        if (c->inventory[i] == static_cast<int>(id)) return true;
    }
    return false;
}

// remove all slots of a weapon from inventory
inline void clear_weapon_from_inventory(CharacterState* c, WeaponId id) {
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        if (c->inventory[i] == static_cast<int>(id)) c->inventory[i] = 0;
    }
}

// first-fit contiguous allocation for weapon slots
// returns true if we found space and placed it
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

// swap out weapons to make room, evicting one at a time to storage
inline bool swap_out_minimal(CharacterState* c, WeaponId need) {
    auto try_after_clear = [&]() {
        return allocate_weapon(c, need);
    };

    if (try_after_clear()) return true;

    // collect unique weapons currently in inventory
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

    // evict weapons one by one until there's enough space
    for (int i = 0; i < unique_count; ++i) {
        WeaponId id = unique[i];
        clear_weapon_from_inventory(c, id);
        if (c->storage_count < MAX_STORAGE) c->storage[c->storage_count++] = static_cast<int>(id);
        if (try_after_clear()) return true;
    }

    return false;
}
