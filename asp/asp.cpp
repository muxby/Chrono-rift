// asp.cpp - enemy AI process, one thread per npc
// 80% chance to attack, 20% skip. handles stun via SIGUSR1

#include "../common.hpp"

#include <csignal>
#include <cstdlib>

static SharedState* g = nullptr;
static volatile sig_atomic_t stunned_until = 0;

// stun handler - arbiter sends SIGUSR1 on stun
void stun_handler(int) {
    stunned_until = now_epoch() + 3;
}

// submit npc action: 80% strike, 20% skip
void submit_npc_action(int nid) {
    PendingAction a{};
    a.actor_team = TEAM_NPC;
    a.actor_id = nid;

    int target = first_living_player(g);
    if (target < 0) {
        a.action = ACT_SKIP;
        a.target_team = TEAM_PLAYER;
        a.target_id = 0;
    } else {
        a.action = (rand() % 100 < 80) ? ACT_STRIKE : ACT_SKIP;
        a.target_team = TEAM_PLAYER;
        a.target_id = target;
    }

    g->pending = a;
    g->pending.ready = 1;
    pthread_cond_signal(&g->action_cv);
    pthread_mutex_unlock(&g->mtx);
    sem_post(&g->action_sem);
    pthread_mutex_lock(&g->mtx);
}

// one of these runs per npc, waits for its turn then submits an action
void* npc_thread(void* arg) {
    int nid = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    pthread_mutex_lock(&g->mtx);
    while (!g->initialized) pthread_cond_wait(&g->turn_cv, &g->mtx);

    while (g->running) {
        // wait for this npc's turn — arbiter broadcasts turn_cv when active changes
        while (g->running && !(g->active_team == TEAM_NPC && g->active_id == nid)) {
            pthread_cond_wait(&g->turn_cv, &g->mtx);
        }
        if (!g->running) break;

        if (!g->npcs[nid].alive) continue;

        // Check stun state
        int now = now_epoch();
        if (now < stunned_until || now < g->npcs[nid].stunned_until_epoch) {
            PendingAction s{};
            s.action = ACT_SKIP;
            s.actor_team = TEAM_NPC;
            s.actor_id = nid;
            g->pending = s;
            g->pending.ready = 1;
            pthread_cond_signal(&g->action_cv);
            pthread_mutex_unlock(&g->mtx);
            sem_post(&g->action_sem);
            pthread_mutex_lock(&g->mtx);
            continue;
        }

        // not stunned, submit ai action
        submit_npc_action(nid);
    }

    pthread_mutex_unlock(&g->mtx);
    return nullptr;
}

// separate thread just for recieving stun signals, keeps npc threads clean
void* signal_thread(void* arg) {
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (g->running) {
        sigsuspend(&oldmask);
        // stun_handler already ran, stunned_until is set
    }
    return nullptr;
}

int main() {
    signal(SIGUSR1, stun_handler);

    g = map_shared(false);
    if (!g) return 1;

    g->asp_pid = getpid();

    // spawn one thread per npc
    pthread_t threads[MAX_NPCS];
    for (int i = 0; i < g->num_npcs; ++i) {
        int* id = new int(i);
        pthread_create(&threads[i], nullptr, npc_thread, id);
    }

    // spawn a dedicated signal-handler thread (detached)
    pthread_t sig_thr;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&sig_thr, &attr, signal_thread, nullptr);
    pthread_attr_destroy(&attr);

    for (int i = 0; i < g->num_npcs; ++i) pthread_join(threads[i], nullptr);

    cleanup_shared(g, false);
    return 0;
}
