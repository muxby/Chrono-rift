#include "StandaloneSim.hpp"
#include "../common.hpp"
#include <chrono>
#include <sstream>
#include <unistd.h>

namespace ChronoRift {

StandaloneSimulation::StandaloneSimulation()
    : state(nullptr), running(false),
      rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())) {}

StandaloneSimulation::~StandaloneSimulation() {
    stop();
}

void StandaloneSimulation::init(SharedState* s) {
    state = s;
    std::memset(state, 0, sizeof(SharedState));

    pthread_mutexattr_t ma;
    pthread_condattr_t ca;
    pthread_mutexattr_init(&ma);
    pthread_condattr_init(&ca);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&state->mtx, &ma);
    pthread_cond_init(&state->turn_cv, &ca);
    pthread_cond_init(&state->action_cv, &ca);

    state->running = 1;
    state->pending.ready = 0;
    state->log_head = 0;
    state->kills = 0;

    // Default parameters
    int roll = 42;
    int party = 3;

    state->roll_seed = roll;
    state->num_players = party;

    std::uniform_int_distribution<int> hpP(100, 1000), hpE(50, 200), nE(3, 7), spE(10, 30);

    int last = roll % 10;
    int second_last = (roll / 10) % 10;
    int last2 = roll % 100;

    // Initialize players
    for (int i = 0; i < state->num_players; ++i) {
        std::memset(&state->players[i], 0, sizeof(CharacterState));
        state->players[i].alive = 1;
        state->players[i].id = i;
        state->players[i].team = TEAM_PLAYER;
        state->players[i].hp = roll + hpP(rng);
        state->players[i].max_hp = state->players[i].hp;
        state->players[i].dmg = last + 10;
        state->players[i].speed = 100 / state->num_players;
        state->players[i].stamina = 0;
        state->players[i].max_stamina = 100;
    }

    // Initialize NPCs
    state->num_npcs = nE(rng);
    for (int i = 0; i < state->num_npcs; ++i) {
        std::memset(&state->npcs[i], 0, sizeof(CharacterState));
        state->npcs[i].alive = 1;
        state->npcs[i].id = i;
        state->npcs[i].team = TEAM_NPC;
        state->npcs[i].hp = last2 + hpE(rng);
        state->npcs[i].max_hp = state->npcs[i].hp;
        state->npcs[i].dmg = second_last + 10;
        state->npcs[i].speed = spE(rng);
        state->npcs[i].stamina = 0;
        state->npcs[i].max_stamina = 150;
    }

    // Initialize artifacts
    state->artifacts[0] = {W_SOLAR_CORE, 1, -1, -1, 0, {0}, {0}};
    state->artifacts[1] = {W_LUNAR_BLADE, 1, -1, -1, 0, {0}, {0}};
    state->artifacts[2] = {W_ECLIPSE_RELIC, 0, -1, -1, 0, {0}, {0}};
    state->eclipse_present = 0;

    state->active_team = TEAM_PLAYER;
    state->active_id = 0;
    state->turn_seq = 1;
    state->initialized = 1;

    add_log(state, "Standalone simulation initialized");
    add_log(state, "Party size: " + std::to_string(party));
    add_log(state, "NPC count: " + std::to_string(state->num_npcs));
}

void StandaloneSimulation::start() {
    if (running.load()) return;
    running = true;
    simThread = std::thread(&StandaloneSimulation::simulationLoop, this);
}

void StandaloneSimulation::stop() {
    running = false;
    if (simThread.joinable()) {
        simThread.join();
    }
}

void StandaloneSimulation::simulationLoop() {
    while (running.load() && state && state->running) {
        pthread_mutex_lock(&state->mtx);
        simulateTurn();
        pthread_mutex_unlock(&state->mtx);
        usleep(500000); // 500ms per turn
    }
}

void StandaloneSimulation::simulateTurn() {
    if (!state->running) return;

    // Update stamina
    updateStamina();

    // Find next active character
    int chosen_team = -1, chosen_id = -1;

    for (int i = 0; i < state->num_players && chosen_team == -1; ++i) {
        if (state->players[i].alive && state->players[i].stamina >= state->players[i].max_stamina) {
            chosen_team = TEAM_PLAYER;
            chosen_id = i;
        }
    }

    for (int i = 0; i < state->num_npcs && chosen_team == -1; ++i) {
        if (state->npcs[i].alive && state->npcs[i].stamina >= state->npcs[i].max_stamina) {
            chosen_team = TEAM_NPC;
            chosen_id = i;
        }
    }

    if (chosen_team == -1) return;

    state->active_team = chosen_team;
    state->active_id = chosen_id;
    state->turn_seq++;

    // Simulate action
    if (chosen_team == TEAM_NPC) {
        simulateNPCAction(chosen_id);
    } else {
        simulatePlayerAction(chosen_id);
    }

    checkWinConditions();
    spawnArtifacts();
}

void StandaloneSimulation::updateStamina() {
    int now = now_epoch();
    for (int i = 0; i < state->num_players; ++i) {
        if (!state->players[i].alive) continue;
        if (now < state->players[i].stunned_until_epoch) continue;
        state->players[i].stamina = std::min(state->players[i].max_stamina,
                                             state->players[i].stamina + state->players[i].speed);
    }
    for (int i = 0; i < state->num_npcs; ++i) {
        if (!state->npcs[i].alive) continue;
        if (now < state->npcs[i].stunned_until_epoch) continue;
        state->npcs[i].stamina = std::min(state->npcs[i].max_stamina,
                                          state->npcs[i].stamina + state->npcs[i].speed);
    }
}

void StandaloneSimulation::simulateNPCAction(int npcId) {
    std::uniform_int_distribution<int> actionDist(0, 100);
    int action = actionDist(rng);

    CharacterState* npc = &state->npcs[npcId];
    int target = first_living_player(state);

    std::ostringstream log;
    log << "N" << npcId << " ";

    if (target < 0 || action < 20) {
        // Skip
        npc->stamina = npc->max_stamina / 2;
        log << "skipped turn";
    } else {
        // Strike
        CharacterState* t = &state->players[target];
        int dmg = npc->dmg;
        t->hp -= dmg;
        npc->stamina = 0;
        log << "struck P" << target << " for " << dmg << " dmg";

        if (t->hp <= 0) {
            t->hp = 0;
            t->alive = 0;
            state->kills++;
            log << " (KILLED)";

            // 50% chance to drop weapon
            if ((rand() % 100) < 50) {
                WeaponId drop = static_cast<WeaponId>((rand() % 8) + 1);
                int p = first_living_player(state);
                if (p >= 0) {
                    if (!allocate_weapon(&state->players[p], drop))
                        swap_out_minimal(&state->players[p], drop);
                    log << " [Weapon dropped]";
                }
            }
        }

        // 20% chance to stun
        if ((rand() % 100) < 20 && t->alive) {
            t->stunned_until_epoch = now_epoch() + 3;
            log << " [STUN 3s]";
        }
    }

    add_log(state, log.str());
}

void StandaloneSimulation::simulatePlayerAction(int playerId) {
    std::uniform_int_distribution<int> actionDist(0, 100);
    int action = actionDist(rng);

    CharacterState* player = &state->players[playerId];
    int target = first_living_npc(state);

    std::ostringstream log;
    log << "P" << playerId << " ";

    if (target < 0) {
        player->stamina = player->max_stamina / 2;
        log << "skipped (no targets)";
        add_log(state, log.str());
        return;
    }

    CharacterState* t = &state->npcs[target];

    if (action < 50) {
        // Strike
        int dmg = player->dmg;
        t->hp -= dmg;
        player->stamina = 0;
        log << "struck N" << target << " for " << dmg << " dmg";
    } else if (action < 60) {
        // Heal
        player->hp = std::min(player->max_hp, player->hp + player->max_hp / 10);
        player->stamina = 0;
        log << "healed +" << (player->max_hp / 10) << " HP";
    } else if (action < 70) {
        // Exhaust
        t->stamina = std::max(0, t->stamina - player->dmg);
        player->stamina = 0;
        log << "exhausted N" << target;
    } else if (action < 80) {
        // Use weapon
        int dmg = 55; // Iron Halberd damage
        t->hp -= dmg;
        player->stamina = 0;
        log << "used weapon on N" << target << " for " << dmg << " dmg";
    } else if (action < 90) {
        // Ultimate (if has both artifacts)
        if (has_weapon(player, W_SOLAR_CORE) && has_weapon(player, W_LUNAR_BLADE)) {
            int dmg = 200;
            t->hp -= dmg;
            player->stamina = 0;
            log << "ULTIMATE on N" << target << " for " << dmg << " dmg";
        } else {
            player->stamina = player->max_stamina / 2;
            log << "skipped (no ultimate)";
        }
    } else {
        // Skip
        player->stamina = player->max_stamina / 2;
        log << "skipped turn";
    }

    if (t->hp <= 0) {
        t->hp = 0;
        t->alive = 0;
        state->kills++;
        log << " (KILLED)";
    }

    // 20% chance to stun
    if ((rand() % 100) < 20 && t->alive) {
        t->stunned_until_epoch = now_epoch() + 3;
        log << " [STUN 3s]";
    }

    // 10% chance to spawn Eclipse Relic
    if (!state->eclipse_present && (rand() % 100) < 10) {
        state->artifacts[2].present = 1;
        state->eclipse_present = 1;
    }

    add_log(state, log.str());
}

void StandaloneSimulation::checkWinConditions() {
    int alive_players = 0;
    for (int i = 0; i < state->num_players; ++i) {
        if (state->players[i].alive) alive_players++;
    }

    if (alive_players == 0) {
        state->running = 0;
        state->win = 0;
        add_log(state, "DEFEAT — All players dead");
        return;
    }

    if (state->kills >= 10) {
        state->running = 0;
        state->win = 1;
        add_log(state, "VICTORY — 10 NPCs killed");
    }
}

void StandaloneSimulation::spawnArtifacts() {
    // Random artifact pickup
    std::uniform_int_distribution<int> dist(0, 100);
    if (dist(rng) < 5) {
        for (int a = 0; a < 2; ++a) {
            if (state->artifacts[a].holder_team == -1) {
                // Find random living character to pick it up
                int team = dist(rng) < 60 ? TEAM_PLAYER : TEAM_NPC;
                int count = (team == TEAM_PLAYER) ? state->num_players : state->num_npcs;
                for (int i = 0; i < count; ++i) {
                    CharacterState* c = (team == TEAM_PLAYER) ? &state->players[i] : &state->npcs[i];
                    if (c->alive) {
                        state->artifacts[a].holder_team = team;
                        state->artifacts[a].holder_id = i;
                        std::ostringstream log;
                        log << (team == TEAM_PLAYER ? "P" : "N") << i << " acquired "
                            << WEAPONS[state->artifacts[a].id].name;
                        add_log(state, log.str());
                        break;
                    }
                }
                break;
            }
        }
    }
}

} // namespace ChronoRift
