#include "StandaloneSim.hpp"
#include "os_helpers.hpp"
#include "../common.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

using namespace std;

namespace ChronoRift {

StandaloneSimulation::StandaloneSimulation()
    : state(nullptr), running(false), threadStarted(false) {
    pthread_mutex_init(&running_mtx, NULL);
    srand(static_cast<unsigned>(time(NULL)));
}

StandaloneSimulation::~StandaloneSimulation() {
    stop();
    pthread_mutex_destroy(&running_mtx);
}

void StandaloneSimulation::init(SharedState* s) {
    state = s;
    memset(state, 0, sizeof(SharedState));

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

    int roll = 42;
    int party = 3;

    state->roll_seed = roll;
    state->num_players = party;

    int last = roll % 10;
    int second_last = (roll / 10) % 10;
    int last2 = roll % 100;

    for (int i = 0; i < state->num_players; ++i) {
        memset(&state->players[i], 0, sizeof(CharacterState));
        state->players[i].alive = 1;
        state->players[i].id = i;
        state->players[i].team = TEAM_PLAYER;
        state->players[i].hp = roll + (rand() % 901 + 100);
        state->players[i].max_hp = state->players[i].hp;
        state->players[i].dmg = last + 10;
        state->players[i].speed = 100 / state->num_players;
        state->players[i].stamina = 0;
        state->players[i].max_stamina = 100;
    }

    state->num_npcs = rand() % 5 + 3;
    for (int i = 0; i < state->num_npcs; ++i) {
        memset(&state->npcs[i], 0, sizeof(CharacterState));
        state->npcs[i].alive = 1;
        state->npcs[i].id = i;
        state->npcs[i].team = TEAM_NPC;
        state->npcs[i].hp = last2 + (rand() % 151 + 50);
        state->npcs[i].max_hp = state->npcs[i].hp;
        state->npcs[i].dmg = second_last + 10;
        state->npcs[i].speed = rand() % 21 + 10;
        state->npcs[i].stamina = 0;
        state->npcs[i].max_stamina = 150;
    }

    state->artifacts[0] = {W_SOLAR_CORE, 1, -1, -1, 0, {0}, {0}};
    state->artifacts[1] = {W_LUNAR_BLADE, 1, -1, -1, 0, {0}, {0}};
    state->artifacts[2] = {W_ECLIPSE_RELIC, 0, -1, -1, 0, {0}, {0}};
    state->eclipse_present = 0;

    state->active_team = TEAM_PLAYER;
    state->active_id = 0;
    state->turn_seq = 1;
    state->initialized = 1;

    add_log(state, "Standalone simulation initialized");
    add_log(state, "Party size: " + to_string(party));
    add_log(state, "NPC count: " + to_string(state->num_npcs));
}

void StandaloneSimulation::start() {
    pthread_mutex_lock(&running_mtx);
    if (running) {
        pthread_mutex_unlock(&running_mtx);
        return;
    }
    running = true;
    pthread_mutex_unlock(&running_mtx);

    threadStarted = true;
    pthread_create(&simThread, NULL, &StandaloneSimulation::threadEntry, this);
}

void StandaloneSimulation::stop() {
    pthread_mutex_lock(&running_mtx);
    running = false;
    pthread_mutex_unlock(&running_mtx);

    if (threadStarted) {
        pthread_join(simThread, NULL);
        threadStarted = false;
    }
}

void* StandaloneSimulation::threadEntry(void* arg) {
    StandaloneSimulation* self = static_cast<StandaloneSimulation*>(arg);
    self->simulationLoop();
    return NULL;
}

void StandaloneSimulation::simulationLoop() {
    while (state && state->running) {
        pthread_mutex_lock(&running_mtx);
        bool r = running;
        pthread_mutex_unlock(&running_mtx);
        if (!r) break;

        pthread_mutex_lock(&state->mtx);
        simulateTurn();
        pthread_mutex_unlock(&state->mtx);
        usleep(500000);
    }
}

void StandaloneSimulation::simulateTurn() {
    if (!state->running) return;

    updateStamina();

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
        state->players[i].stamina = cr_min(state->players[i].max_stamina,
                                           state->players[i].stamina + state->players[i].speed);
    }
    for (int i = 0; i < state->num_npcs; ++i) {
        if (!state->npcs[i].alive) continue;
        if (now < state->npcs[i].stunned_until_epoch) continue;
        state->npcs[i].stamina = cr_min(state->npcs[i].max_stamina,
                                        state->npcs[i].stamina + state->npcs[i].speed);
    }
}

void StandaloneSimulation::simulateNPCAction(int npcId) {
    int action = rand() % 101;

    CharacterState* npc = &state->npcs[npcId];
    int target = first_living_player(state);

    char logbuf[256];
    int pos = snprintf(logbuf, sizeof(logbuf), "N%d ", npcId);

    if (target < 0 || action < 20) {
        npc->stamina = npc->max_stamina / 2;
        snprintf(logbuf + pos, sizeof(logbuf) - pos, "skipped turn");
    } else {
        CharacterState* t = &state->players[target];
        int dmg = npc->dmg;
        t->hp -= dmg;
        npc->stamina = 0;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "struck P%d for %d dmg", target, dmg);

        if (t->hp <= 0) {
            t->hp = 0;
            t->alive = 0;
            state->kills++;
            pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, " (KILLED)");

            if ((rand() % 100) < 50) {
                WeaponId drop = static_cast<WeaponId>((rand() % 8) + 1);
                int p = first_living_player(state);
                if (p >= 0) {
                    if (!allocate_weapon(&state->players[p], drop))
                        swap_out_minimal(&state->players[p], drop);
                    pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, " [Weapon dropped]");
                }
            }
        }

        if ((rand() % 100) < 20 && t->alive) {
            t->stunned_until_epoch = now_epoch() + 3;
            pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, " [STUN 3s]");
        }
    }

    add_log(state, string(logbuf));
}

void StandaloneSimulation::simulatePlayerAction(int playerId) {
    int action = rand() % 101;

    CharacterState* player = &state->players[playerId];
    int target = first_living_npc(state);

    char logbuf[256];
    int pos = snprintf(logbuf, sizeof(logbuf), "P%d ", playerId);

    if (target < 0) {
        player->stamina = player->max_stamina / 2;
        snprintf(logbuf + pos, sizeof(logbuf) - pos, "skipped (no targets)");
        add_log(state, string(logbuf));
        return;
    }

    CharacterState* t = &state->npcs[target];

    if (action < 50) {
        int dmg = player->dmg;
        t->hp -= dmg;
        player->stamina = 0;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "struck N%d for %d dmg", target, dmg);
    } else if (action < 60) {
        player->hp = cr_min(player->max_hp, player->hp + player->max_hp / 10);
        player->stamina = 0;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "healed +%d HP", player->max_hp / 10);
    } else if (action < 70) {
        t->stamina = cr_max(0, t->stamina - player->dmg);
        player->stamina = 0;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "exhausted N%d", target);
    } else if (action < 80) {
        int dmg = 55;
        t->hp -= dmg;
        player->stamina = 0;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "used weapon on N%d for %d dmg", target, dmg);
    } else if (action < 90) {
        if (has_weapon(player, W_SOLAR_CORE) && has_weapon(player, W_LUNAR_BLADE)) {
            int dmg = 200;
            t->hp -= dmg;
            player->stamina = 0;
            pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "ULTIMATE on N%d for %d dmg", target, dmg);
        } else {
            player->stamina = player->max_stamina / 2;
            pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "skipped (no ultimate)");
        }
    } else {
        player->stamina = player->max_stamina / 2;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "skipped turn");
    }

    if (t->hp <= 0) {
        t->hp = 0;
        t->alive = 0;
        state->kills++;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, " (KILLED)");
    }

    if ((rand() % 100) < 20 && t->alive) {
        t->stunned_until_epoch = now_epoch() + 3;
        pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, " [STUN 3s]");
    }

    if (!state->eclipse_present && (rand() % 100) < 10) {
        state->artifacts[2].present = 1;
        state->eclipse_present = 1;
    }

    add_log(state, string(logbuf));
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
    if (rand() % 101 < 5) {
        for (int a = 0; a < 2; ++a) {
            if (state->artifacts[a].holder_team == -1) {
                int team = (rand() % 100) < 60 ? TEAM_PLAYER : TEAM_NPC;
                int count = (team == TEAM_PLAYER) ? state->num_players : state->num_npcs;
                for (int i = 0; i < count; ++i) {
                    CharacterState* c = (team == TEAM_PLAYER) ? &state->players[i] : &state->npcs[i];
                    if (c->alive) {
                        state->artifacts[a].holder_team = team;
                        state->artifacts[a].holder_id = i;
                        char logbuf[256];
                        snprintf(logbuf, sizeof(logbuf), "%s%d acquired %s",
                                 (team == TEAM_PLAYER ? "P" : "N"), i,
                                 WEAPONS[state->artifacts[a].id].name);
                        add_log(state, string(logbuf));
                        break;
                    }
                }
                break;
            }
        }
    }
}

} // namespace ChronoRift
