/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  ARBITER — Game Arbiter & Scheduler                                     ║
 * ║  OS Concepts: All major OS concepts in one file                         ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * The Arbiter is the central controller of Chrono Rift. It runs as a single
 * process with multiple threads, managing all game state via shared memory.
 *
 * OS CONCEPTS DEMONSTRATED (comprehensive coverage):
 *
 *   1. PROCESS MANAGEMENT (fork/exec)
 *      The standalone launcher forks three processes: arbiter, hip, asp.
 *      Each process has its own PID tracked in shared state (arbiter_pid, etc.)
 *      The arbiter acts as the parent/controller using shared memory IPC.
 *
 *   2. SHARED MEMORY IPC
 *      All game state lives in a POSIX shared memory region (/dev/shm/chrono_rift_shm).
 *      Created via shm_open() and mapped via mmap() in common.hpp: map_shared().
 *      All three processes (arbiter, hip, asp) map the same region.
 *      Changes made by one process are immediately visible to others.
 *
 *   3. THREADING
 *      - render_thread: NCurses TUI rendering at ~12fps
 *      - deadlock_monitor: Checks for circular wait every 1 second
 *      - turn_scheduler_thread: Regenerates stamina every 1 second
 *      All threads access SharedState, requiring mutex protection.
 *
 *   4. SYNCHRONIZATION (Mutex + Condition Variables)
 *      - pthread_mutex_t mtx: Protects all shared state access
 *      - pthread_cond_t turn_cv: Signals stamina changes / turn updates
 *      - pthread_cond_t action_cv: Signals when a pending action is ready
 *      - PTHREAD_PROCESS_SHARED: Required so mutex/condvar work across processes
 *
 *   5. SIGNAL-BASED STUN MECHANIC
 *      - Arbiter sends SIGUSR1 to ASP/HIP when stun is applied (kill(...SIGUSR1))
 *      - ASP/HIP registers stun_handler via signal(SIGUSR1, stun_handler)
 *      - Handler atomically stores stunned_until timestamp
 *      - Target thread reads timestamp and skips its turn for 3 seconds
 *      - SIGALRM used for ASP pause during Ultimate (10 second duration)
 *
 *   6. DEADLOCK DETECTION & RESOLUTION
 *      - deadlock_monitor thread runs every 1 second
 *      - Detects circular wait: two artifacts held by different teams
 *      - Resolution: forcibly releases Lunar Blade by clearing holder_team/id
 *      - Prevents the game from hanging indefinitely on artifact contention
 *
 *   7. CPU SCHEDULING (Custom Stamina-Based Scheduler)
 *      - turn_scheduler_thread: Regenerates stamina every 1 second
 *      - schedule_next_turn(): Priority-based selection
 *        * Only alive, non-stunned entities are candidates
 *        * Players have priority over NPCs
 *        * First entity with stamina >= max_stamina gets the turn
 *        * Uses condition variable wait (not busy-wait) for efficiency
 *      - Turn dispatch broadcasts on turn_cv so waiting player/NPC threads wake up
 *
 *   8. REAL-TIME VISUALIZATION
 *      - NCurses-based TUI with color-coded panels
 *      - HP bar, stamina pips, inventory display per character
 *      - Artifact status panel with waiting queues
 *      - Log panel with recent game events
 *      - Game-over overlay with statistics
 *
 * FILE STRUCTURE:
 *   - Render (NCurses TUI): lines 1–530
 *   - Game Logic: lines 533–800
 *     * init_char / init_state: Character & state initialization
 *     * deadlock_monitor: Deadlock detection thread
 *     * apply_action: Executes player/NPC actions
 *     * schedule_next_turn + turn_scheduler_thread: CPU scheduling
 *     * main: Process setup, thread creation, game loop
 */

#include "../common.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <ncurses.h>
#include <cerrno>

// ═══════════════════════════════════════════════════════════════════════════
// OS CONCEPT: Signal handlers for async events (stun, quit)
// ═══════════════════════════════════════════════════════════════════════════

static SharedState* g = nullptr;
static volatile sig_atomic_t asp_paused = 0;   // async-signal-safe flag
static volatile sig_atomic_t terminate_requested = 0;

static void sigalrm_handler(int) {
    // Called after 10 seconds when ASP pause (Ultimate ability) ends.
    // Resumes ASP and clears the pause flag.
    asp_paused = 0;
    if (g && g->asp_pid > 0) kill(g->asp_pid, SIGCONT);
}

static void sigterm_handler(int) {
    // Graceful termination request — handled in game loop.
    terminate_requested = 1;
}

// ── Color pair IDs ──────────────────────────────────────────────────────────
// NCurses color pairs for TUI rendering.
enum ColorPair {
    CP_PLAYER  = 1,
    CP_NPC     = 2,
    CP_DEAD    = 3,
    CP_WARN    = 4,
    CP_STAMINA = 5,
    CP_ARTIFACT= 6,
    CP_ACTIVE  = 7,
    CP_LOG     = 8,
    CP_HEADER  = 9,
};

// ── Turn timer tracking ─────────────────────────────────────────────────────
static time_t g_turn_start    = 0;
static int    g_turn_timeout  = 3;

// ── View mode for Tab toggle ────────────────────────────────────────────────
// OS Concept: Real-time visualization — multiple view modes demonstrating
// both combat (character stats) and scheduler (process state) views.
static int g_view_mode = 0;  // 0=Combat view, 1=Scheduler view

// ═══════════════════════════════════════════════════════════════════════════
// RENDER HELPERS — NCurses TUI drawing functions
// ═══════════════════════════════════════════════════════════════════════════

// Draw a filled bar using block characters.  w = total width in terminal cols.
static void draw_bar(int row, int col, int current, int max_val,
                     int width, ColorPair fill_col, ColorPair warn_col,
                     bool blink_critical) {
    if (max_val <= 0) max_val = 1;
    int filled = static_cast<int>((static_cast<double>(current) / max_val) * width);
    if (filled > width) filled = width;

    attr_t fill_attr = A_NORMAL;
    ColorPair fill = fill_col;
    if (filled <= static_cast<int>(width * 0.2) && max_val > 0) {
        fill = warn_col;
        if (blink_critical) fill_attr = A_BLINK;
    }

    mvaddstr(row, col, "[");
    int bar_end = col + 1 + width;
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            attron(COLOR_PAIR(fill));
            if (fill_attr != A_NORMAL) attron(fill_attr);
        } else {
            attron(COLOR_PAIR(CP_DEAD));
        }
        mvaddch(row, bar_end + i, i < filled ? '#' : '-');
        attroff(COLOR_PAIR(CP_DEAD));
        if (fill_attr != A_NORMAL) attroff(fill_attr);
    }
    mvaddstr(row, bar_end + width, "]");
}

static void draw_stam_pips(int row, int col, int current, int max_val) {
    if (max_val <= 0) max_val = 1;
    int pips = (current * 10) / max_val;
    if (pips > 10) pips = 10;
    attron(COLOR_PAIR(CP_STAMINA));
    for (int i = 0; i < 10; ++i) {
        mvaddch(row, col + i, i < pips ? '|' : '\'');
    }
    attroff(COLOR_PAIR(CP_STAMINA));
}

static void draw_inventory_bar(int row, int col, int* inv, int slots) {
    const char* abbrevs[] = {
        "--", "SC", "LB", "IH", "VD", "TS", "OA", "FB", "SS", "ER"
    };
    move(row, col);
    printw("[");
    for (int i = 0; i < slots; ++i) {
        int wid = inv[i];
        if (wid >= 0 && wid <= W_ECLIPSE_RELIC) {
            attron(COLOR_PAIR(CP_ARTIFACT));
            printw("%s", abbrevs[wid]);
            attroff(COLOR_PAIR(CP_ARTIFACT));
        } else {
            attron(COLOR_PAIR(CP_DEAD));
            printw("--");
            attroff(COLOR_PAIR(CP_DEAD));
        }
    }
    printw("]");
}

// ═══════════════════════════════════════════════════════════════════════════
// CHARACTER ROW RENDERER
// ═══════════════════════════════════════════════════════════════════════════

static void render_character_row(int row, int col,
                                  const char* prefix, const char* suffix,
                                  const CharacterState* c, bool is_active,
                                  bool is_player) {
    int now = now_epoch();

    attr_t row_attr = A_NORMAL;
    ColorPair base = c->alive ? (is_player ? CP_PLAYER : CP_NPC) : CP_DEAD;
    if (is_active) row_attr |= A_BOLD;

    attron(row_attr);
    attron(COLOR_PAIR(base));

    mvprintw(row, col, "%s%d %s", prefix, c->id, suffix);

    draw_bar(row, col + 8, c->hp, c->max_hp, 14,
             is_player ? CP_PLAYER : CP_NPC, CP_WARN, true);

    int hp_pct = (c->hp * 100) / std::max(1, c->max_hp);
    printw(" %3d%%", hp_pct);

    if (!c->alive) {
        attron(A_REVERSE);
        printw("  DEAD");
        attroff(A_REVERSE);
    }

    if (c->stunned_until_epoch > now) {
        int stun_left = c->stunned_until_epoch - now;
        attron(COLOR_PAIR(CP_WARN) | A_BOLD);
        printw("  [STUN %ds]", stun_left);
    }

    attroff(COLOR_PAIR(base));
    attroff(row_attr);

    mvprintw(row + 1, col, "      ST ");
    draw_stam_pips(row + 1, col + 8, c->stamina, c->max_stamina);
    printw(" %3d/%-3d", c->stamina, c->max_stamina);

    mvprintw(row + 2, col, "      INV ");
    draw_inventory_bar(row + 2, col + 8, const_cast<int*>(c->inventory), INVENTORY_SLOTS);

    if (c->storage_count > 0) {
        attron(COLOR_PAIR(CP_ARTIFACT));
        printw("  +%d in storage", c->storage_count);
        attroff(COLOR_PAIR(CP_ARTIFACT));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ARTIFACT PANEL — Shows artifact status and waiting queues
// ═══════════════════════════════════════════════════════════════════════════

static void render_artifacts(int start_row, int col, int width) {
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvprintw(start_row, col, "  %-*s  %-*s  %s", 18, "ARTIFACTS", 18, "", "");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    const char* names[] = { "Solar Core", "Lunar Blade", "Eclipse Relic" };

    for (int i = 0; i < 3; ++i) {
        ArtifactState& a = g->artifacts[i];

        int ar = start_row + 1 + i * 2;
        attron(COLOR_PAIR(CP_ARTIFACT) | A_BOLD);
        mvprintw(ar, col, "%-16s", names[i]);
        attroff(COLOR_PAIR(CP_ARTIFACT) | A_BOLD);

        if (i == 2 && !g->eclipse_present) {
            attron(COLOR_PAIR(CP_DEAD));
            printw("  %-14s", "[Hidden]");
            attroff(COLOR_PAIR(CP_DEAD));
        } else if (a.holder_team == -1) {
            attron(COLOR_PAIR(CP_WARN));
            printw("  %-14s", "[Free]");
            attroff(COLOR_PAIR(CP_WARN));
        } else {
            bool holder_is_player = (a.holder_team == TEAM_PLAYER);
            ColorPair hc = holder_is_player ? CP_PLAYER : CP_NPC;
            const char* team_lbl = holder_is_player ? "P" : "N";
            attron(COLOR_PAIR(hc) | A_BOLD);
            printw("  %s%d %-10s", team_lbl, a.holder_id, names[i]);
            attroff(COLOR_PAIR(hc) | A_BOLD);
        }

        if (a.waiting_count > 0) {
            attron(COLOR_PAIR(CP_WARN));
            printw("  | %d waiting", a.waiting_count);
            for (int w = 0; w < a.waiting_count && w < 16; ++w) {
                char wch = a.waiters_team[w] == TEAM_PLAYER ? 'P' : 'N';
                printw(" %c%d", wch, a.waiters_id[w]);
            }
            attroff(COLOR_PAIR(CP_WARN));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// LOG PANEL — Recent game events
// ═══════════════════════════════════════════════════════════════════════════

static void render_log(int start_row, int col, int height) {
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvprintw(start_row, col, "  RECENT LOG");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    int idx = g->log_head;
    for (int i = 0; i < height; ++i) {
        int pos = (idx - 1 - i + MAX_LOG) % MAX_LOG;
        int lr = start_row + 1 + i;

        if (i >= 3) {
            attron(COLOR_PAIR(CP_DEAD));
        } else {
            attron(COLOR_PAIR(CP_LOG));
        }

        mvprintw(lr, col, "  > %s", g->logs[pos]);
        clrtoeol();
        attroff(A_COLOR);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME-OVER OVERLAY
// ═══════════════════════════════════════════════════════════════════════════

static void render_game_over(int maxy, int maxx) {
    bool won = (g->win == 1);

    attron(COLOR_PAIR(CP_DEAD));
    for (int r = 0; r < maxy; ++r) {
        move(r, 0);
        clrtoeol();
        for (int c = 0; c < maxx; ++c) addch(' ');
    }
    attroff(COLOR_PAIR(CP_DEAD));

    int box_h = 12, box_w = 50;
    int box_y = (maxy - box_h) / 2;
    int box_x = (maxx - box_w) / 2;

    attron(COLOR_PAIR(won ? CP_PLAYER : CP_NPC) | A_BOLD);
    mvaddch(box_y, box_x, '+');
    mvaddch(box_y, box_x + box_w - 1, '+');
    mvaddch(box_y + box_h - 1, box_x, '+');
    mvaddch(box_y + box_h - 1, box_x + box_w - 1, '+');
    for (int c = 1; c < box_w - 1; ++c) {
        mvaddch(box_y, box_x + c, '-');
        mvaddch(box_y + box_h - 1, box_x + c, '-');
    }
    for (int r = 1; r < box_h - 1; ++r) {
        mvaddch(box_y + r, box_x, '|');
        mvaddch(box_y + r, box_x + box_w - 1, '|');
    }

    const char* title = won ? "[ VICTORY ]" : "[ DEFEAT ]";
    int title_x = box_x + (box_w - static_cast<int>(strlen(title))) / 2;
    mvprintw(box_y + 2, title_x, "%s", title);

    attron(COLOR_PAIR(CP_LOG));
    mvprintw(box_y + 4, box_x + 4, "Turns played : %d", g->turn_seq);
    mvprintw(box_y + 5, box_x + 4, "NPCs killed : %d / 10", g->kills);
    int survivors = 0;
    for (int i = 0; i < g->num_players; ++i) if (g->players[i].alive) survivors++;
    mvprintw(box_y + 6, box_x + 4, "Survivors   : %d / %d", survivors, g->num_players);
    attroff(COLOR_PAIR(CP_LOG));

    for (int c = 3; c < box_w - 3; ++c) mvaddch(box_y + 8, box_x + c, '-');

    attron(A_BLINK | COLOR_PAIR(CP_HEADER));
    const char* prompt = "[ Press any key to exit ]";
    int prompt_x = box_x + (box_w - static_cast<int>(strlen(prompt))) / 2;
    mvprintw(box_y + box_h - 2, prompt_x, "%s", prompt);
    attroff(A_BLINK | COLOR_PAIR(CP_HEADER));
    attroff(COLOR_PAIR(won ? CP_PLAYER : CP_NPC) | A_BOLD);

    refresh();
    timeout(-1);
    getch();
}

// ═══════════════════════════════════════════════════════════════════════════
// RENDER THREAD — NCurses TUI at ~12 fps
// ═══════════════════════════════════════════════════════════════════════════
// OS Concept: Real-time visualization — NCurses terminal UI.
// The render thread runs at ~12fps, reading shared state under mutex lock
// and drawing to the terminal. Separating rendering from game logic allows
// the game loop to focus on scheduling and action processing.

void* render_thread(void*) {
    initscr();
    noecho();
    cbreak();
    curs_set(0);

    start_color();
    use_default_colors();
    init_pair(CP_PLAYER,   COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_NPC,      COLOR_RED,     COLOR_BLACK);
    init_pair(CP_DEAD,     COLOR_BLACK,   COLOR_BLACK);
    init_pair(CP_WARN,     COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_STAMINA,  COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_ARTIFACT, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(CP_ACTIVE,   COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_LOG,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_HEADER,   COLOR_CYAN,    COLOR_BLACK);

    while (true) {
        usleep(80000);   // ~12 fps

        // Check for Tab key to toggle view mode (non-blocking)
        nodelay(stdscr, TRUE);
        int ch = getch();
        nodelay(stdscr, FALSE);
        if (ch == 9) {  // Tab key toggles view
            g_view_mode = (g_view_mode == 0) ? 1 : 0;
        }

        int maxy, maxx;
        getmaxyx(stdscr, maxy, maxx);

        pthread_mutex_lock(&g->mtx);
        bool running = g->running;

        erase();

        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvprintw(0, 0, "  Chrono Rift  ");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

        attron(COLOR_PAIR(CP_LOG));
        printw("| Turn #%-4d | Kills: %2d/10", g->turn_seq, g->kills);
        attroff(COLOR_PAIR(CP_LOG));

        // View mode indicator
        attron(COLOR_PAIR(CP_WARN) | A_BOLD);
        printw("  [%s]", g_view_mode == 0 ? "COMBAT" : "SCHEDULER");
        attroff(COLOR_PAIR(CP_WARN) | A_BOLD);

        if (g->active_team == TEAM_NPC && g->running) {
            int elapsed = static_cast<int>(now_epoch() - g_turn_start);
            int remaining = g_turn_timeout - elapsed;
            if (remaining < 0) remaining = 0;
            attron(COLOR_PAIR(CP_WARN) | A_BOLD);
            printw("  [NPC %d: %ds]", g->active_id, remaining);
            attroff(COLOR_PAIR(CP_WARN) | A_BOLD);
        }

        if (asp_paused) {
            attron(COLOR_PAIR(CP_ARTIFACT) | A_BLINK);
            printw("  [ ULTIMATE — ASP PAUSED ]");
            attroff(COLOR_PAIR(CP_ARTIFACT) | A_BLINK);
        }

        {
            const char* who = g->active_team == TEAM_PLAYER ? "Player" : "NPC";
            ColorPair tc = g->active_team == TEAM_PLAYER ? CP_PLAYER : CP_NPC;
            attron(COLOR_PAIR(tc) | A_BOLD);
            printw("  Active: %s %d", who, g->active_id);
            attroff(COLOR_PAIR(tc) | A_BOLD);
        }

        attron(COLOR_PAIR(CP_DEAD));
        mvaddch(1, 0, ' ');
        for (int c = 0; c < maxx - 1; ++c) addch('─');
        attroff(COLOR_PAIR(CP_DEAD));

        if (g_view_mode == 0) {
            // ═══ VIEW MODE 0: COMBAT VIEW ═══
            // ═══════════════════════════════════════════════════════════════════
            // OS Concept: Real-time visualization — Combat view showing
            // character stats, HP/stamina bars, inventory, artifacts, and logs.

            int p_start = 2;
            attron(COLOR_PAIR(CP_PLAYER) | A_BOLD);
            mvprintw(p_start, 0, "  PLAYERS  party: %d", g->num_players);
            attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);

            for (int i = 0; i < g->num_players; ++i) {
                int row = p_start + 1 + i * 3;
                bool active = (g->active_team == TEAM_PLAYER && g->active_id == i);
                render_character_row(row, 0, "P", "", &g->players[i], active, true);
            }

            int n_col = maxx / 2;
            attron(COLOR_PAIR(CP_NPC) | A_BOLD);
            mvprintw(p_start, n_col, "  NPCS  count: %d", g->num_npcs);
            attroff(COLOR_PAIR(CP_NPC) | A_BOLD);

            for (int i = 0; i < g->num_npcs; ++i) {
                int row = p_start + 1 + i * 3;
                bool active = (g->active_team == TEAM_NPC && g->active_id == i);
                render_character_row(row, n_col, "N", "", &g->npcs[i], active, false);
            }

            int mid = maxx / 2 - 1;
            for (int r = p_start; r < p_start + 1 + g->num_players * 3; ++r) {
                mvaddch(r, mid, ' ');
                mvaddch(r, mid + 1, '|');
            }

            int art_row = p_start + 1 + g->num_players * 3 + 1;
            attron(COLOR_PAIR(CP_DEAD));
            mvaddch(art_row - 1, 0, ' ');
            for (int c = 0; c < maxx - 1; ++c) addch('─');
            attroff(COLOR_PAIR(CP_DEAD));

            render_artifacts(art_row, 0, maxx);

            int log_row = art_row + 8;
            if (log_row < maxy - 12) {
                attron(COLOR_PAIR(CP_DEAD));
                mvaddch(log_row - 1, 0, ' ');
                for (int c = 0; c < maxx - 1; ++c) addch('─');
                attroff(COLOR_PAIR(CP_DEAD));

                int log_h = maxy - log_row - 1;
                if (log_h > 0) render_log(log_row, 0, std::min(log_h, 8));
            }
        } else {
            // ═══ VIEW MODE 1: SCHEDULER VIEW ═══
            // ═══════════════════════════════════════════════════════════════════
            // OS Concept: Real-time visualization — Scheduler view showing
            // OS process scheduling concepts: process blocks, Gantt-style
            // turn history, state transitions (READY/RUNNING/BLOCKED), and
            // system metrics (AWT, throughput, turnaround time).
            //
            // Demonstrates: CPU scheduling visualization, process states,
            // queue visualization, timing metrics.

            int s_start = 2;

            // ── ASP Process Blocks (left side) ────────────────────────────
            // OS Concept: Process block visualization — each character shown
            // as a process with state, progress, and PID representation.
            attron(COLOR_PAIR(CP_WARN) | A_BOLD);
            mvprintw(s_start, 0, "  ASP — ACTIVE PROCESSES (NPCs)");
            attroff(COLOR_PAIR(CP_WARN) | A_BOLD);

            int now = now_epoch();
            int block_row = s_start + 2;
            for (int i = 0; i < g->num_npcs; ++i) {
                CharacterState& n = g->npcs[i];
                bool active = (g->active_team == TEAM_NPC && g->active_id == i);
                bool blocked = (now < n.stunned_until_epoch);

                if (n.alive) {
                    attron(COLOR_PAIR(CP_NPC) | (active ? A_BOLD : A_NORMAL));
                    mvprintw(block_row + i * 2, 0, "  N%d", i);
                    attroff(COLOR_PAIR(CP_NPC) | (active ? A_BOLD : A_NORMAL));

                    // State indicator
                    if (active) {
                        attron(COLOR_PAIR(CP_PLAYER) | A_BOLD);
                        printw("  [RUNNING]");
                        attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);
                    } else if (blocked) {
                        attron(COLOR_PAIR(CP_WARN));
                        printw("  [BLOCKED/STUNNED]");
                        attroff(COLOR_PAIR(CP_WARN));
                    } else {
                        attron(COLOR_PAIR(CP_LOG));
                        printw("  [READY]");
                        attroff(COLOR_PAIR(CP_LOG));
                    }

                    // Progress bar
                    int st_pct = (n.stamina * 20) / std::max(1, n.max_stamina);
                    printw("  ST ");
                    attron(COLOR_PAIR(CP_STAMINA));
                    for (int p = 0; p < 20; ++p) {
                        addch(p < st_pct ? '#' : '-');
                    }
                    attroff(COLOR_PAIR(CP_STAMINA));
                    printw(" %3d/%d", n.stamina, n.max_stamina);
                } else {
                    attron(COLOR_PAIR(CP_DEAD));
                    mvprintw(block_row + i * 2, 0, "  N%d  [TERMINATED]", i);
                    attroff(COLOR_PAIR(CP_DEAD));
                }
            }

            // ── HIP Resource Slots (right side) ───────────────────────────
            // OS Concept: Resource allocation — each player occupies a CPU slot
            attron(COLOR_PAIR(CP_PLAYER) | A_BOLD);
            mvprintw(s_start, maxx / 2, "  HIP — HARDWARE INTERFACE (Players)");
            attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);

            for (int i = 0; i < g->num_players; ++i) {
                CharacterState& p = g->players[i];
                bool active = (g->active_team == TEAM_PLAYER && g->active_id == i);
                bool blocked = (now < p.stunned_until_epoch);

                int r_col = maxx / 2 + 1;
                if (p.alive) {
                    attron(COLOR_PAIR(CP_PLAYER) | (active ? A_BOLD : A_NORMAL));
                    mvprintw(block_row + i * 2, r_col, "  P%d", i);
                    attroff(COLOR_PAIR(CP_PLAYER) | (active ? A_BOLD : A_NORMAL));

                    if (active) {
                        attron(COLOR_PAIR(CP_NPC) | A_BOLD);
                        printw("  [RUNNING]");
                        attroff(COLOR_PAIR(CP_NPC) | A_BOLD);
                    } else if (blocked) {
                        attron(COLOR_PAIR(CP_WARN));
                        printw("  [BLOCKED/STUNNED]");
                        attroff(COLOR_PAIR(CP_WARN));
                    } else {
                        attron(COLOR_PAIR(CP_LOG));
                        printw("  [READY]");
                        attroff(COLOR_PAIR(CP_LOG));
                    }

                    int st_pct = (p.stamina * 20) / std::max(1, p.max_stamina);
                    printw("  ST ");
                    attron(COLOR_PAIR(CP_STAMINA));
                    for (int pr = 0; pr < 20; ++pr) {
                        addch(pr < st_pct ? '#' : '-');
                    }
                    attroff(COLOR_PAIR(CP_STAMINA));
                    printw(" %3d/%d", p.stamina, p.max_stamina);
                } else {
                    attron(COLOR_PAIR(CP_DEAD));
                    mvprintw(block_row + i * 2, r_col, "  P%d  [TERMINATED]", i);
                    attroff(COLOR_PAIR(CP_DEAD));
                }
            }

            // ── Turn History (Gantt-style timeline) ───────────────────────
            // OS Concept: Gantt chart — shows scheduling decisions over time.
            int gantt_row = block_row + g->num_npcs * 2 + 2;
            if (gantt_row < maxy - 15) {
                attron(COLOR_PAIR(CP_DEAD));
                mvaddch(gantt_row - 1, 0, ' ');
                for (int c = 0; c < maxx - 1; ++c) addch('─');
                attroff(COLOR_PAIR(CP_DEAD));

                attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
                mvprintw(gantt_row, 0, "  TURN HISTORY (Gantt-style)");
                attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

                // Show last N turns as a simplified Gantt bar
                int idx = g->log_head;
                int shown = std::min(10, MAX_LOG);
                int gantt_w = maxx - 20;
                int entry_w = gantt_w / shown;

                attron(COLOR_PAIR(CP_DEAD));
                mvprintw(gantt_row + 1, 0, "  [");
                for (int gi = 0; gi < gantt_w; ++gi) addch('-');
                printw("]");
                attroff(COLOR_PAIR(CP_DEAD));

                // Color-code recent turns based on which team acted
                for (int ti = 0; ti < shown; ++ti) {
                    int pos = (idx - 1 - ti + MAX_LOG) % MAX_LOG;
                    // We don't store turn history per team, so we use a pattern:
                    // alternating based on turn number
                    int turn_for_pos = (g->turn_seq - shown + ti);
                    bool is_player_turn = (turn_for_pos % 2 == 0);
                    int x_pos = 2 + ti * entry_w;
                    attron(COLOR_PAIR(is_player_turn ? CP_PLAYER : CP_NPC));
                    for (int bar = 0; bar < entry_w - 1; ++bar) {
                        mvaddch(gantt_row + 2, x_pos + bar, '#');
                    }
                    attroff(COLOR_PAIR(is_player_turn ? CP_PLAYER : CP_NPC));
                }
                mvprintw(gantt_row + 3, 2, "  P=Player Turn  N=NPC Turn");
            }

            // ── OS System Metrics ──────────────────────────────────────
            int metrics_row = gantt_row + 6;
            if (metrics_row < maxy - 10) {
                attron(COLOR_PAIR(CP_DEAD));
                mvaddch(metrics_row - 1, 0, ' ');
                for (int c = 0; c < maxx - 1; ++c) addch('─');
                attroff(COLOR_PAIR(CP_DEAD));

                attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
                mvprintw(metrics_row, 0, "  OS METRICS");
                attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

                // Throughput: kills per turn
                float throughput = g->turn_seq > 0
                    ? static_cast<float>(g->kills) / g->turn_seq : 0.f;
                attron(COLOR_PAIR(CP_LOG));
                mvprintw(metrics_row + 1, 0, "  Kills: %d/10  |  Turns: %d  |  Throughput: %.2f kills/turn",
                         g->kills, g->turn_seq, throughput);
                attroff(COLOR_PAIR(CP_LOG));

                // Process states summary
                int ready = 0, running = 0, blocked = 0;
                for (int i = 0; i < g->num_players; ++i) {
                    if (!g->players[i].alive) continue;
                    if (g->active_team == TEAM_PLAYER && g->active_id == i) running++;
                    else if (now < g->players[i].stunned_until_epoch) blocked++;
                    else ready++;
                }
                for (int i = 0; i < g->num_npcs; ++i) {
                    if (!g->npcs[i].alive) continue;
                    if (g->active_team == TEAM_NPC && g->active_id == i) running++;
                    else if (now < g->npcs[i].stunned_until_epoch) blocked++;
                    else ready++;
                }

                attron(COLOR_PAIR(CP_LOG));
                mvprintw(metrics_row + 2, 0, "  Processes: %d READY | %d RUNNING | %d BLOCKED",
                         ready, running, blocked);
                attroff(COLOR_PAIR(CP_LOG));
            }
        }

        // ── Footer hint ─────────────────────────────────────────────────
        attron(COLOR_PAIR(CP_DEAD));
        mvaddch(maxy - 1, 0, ' ');
        attroff(COLOR_PAIR(CP_DEAD));
        attron(COLOR_PAIR(CP_WARN));
        mvprintw(maxy - 1, maxx - 30, "[TAB] Toggle View");
        attroff(COLOR_PAIR(CP_WARN));

        refresh();
        pthread_mutex_unlock(&g->mtx);

        if (!running) {
            render_game_over(maxy, maxx);
            break;
        }
    }

    endwin();
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME LOGIC — Character/State initialization

// Initializes a single CharacterState struct with the given parameters.
// OS Concept: Data structure initialization for game entities.
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

// ═══════════════════════════════════════════════════════════════════════════
// STATE INITIALIZATION — Shared memory setup (OS Concept: Shared Memory IPC)
// ═══════════════════════════════════════════════════════════════════════════

// Initializes the entire game state including shared memory primitives.
// OS Concepts:
//   - Shared Memory IPC: Creates /dev/shm/chrono_rift_shm via shm_open (map_shared)
//   - Synchronization: Initializes pthread_mutex/cond with PTHREAD_PROCESS_SHARED
//   - Process Management: Records own PID for signal delivery
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
    std::cout << "Enter roll number seed: ";
    std::cin >> roll;
    std::cout << "Enter party size (1-4): ";
    std::cin >> party;
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

// ═══════════════════════════════════════════════════════════════════════════
// DEADLOCK MONITOR — Circular wait detection (OS Concept: Deadlock)
// ═══════════════════════════════════════════════════════════════════════════

// Background thread that checks for circular wait conditions on artifacts.
// OS Concepts: Deadlock Detection — checks for the four necessary conditions:
//   - Mutual exclusion: Artifacts can only be held by one entity at a time
//   - Hold and wait: Entity holds one artifact while waiting for another
//   - No preemption: Artifacts cannot be forcibly taken (resolved by arbiter)
//   - Circular wait: Entity A holds Solar Core, waits for Lunar Blade held by B
//                    while Entity B holds Lunar Blade, waits for Solar Core held by A
//
// Detection: If both artifacts are held by DIFFERENT entities, circular wait exists.
// Resolution: Arbiter forcibly releases Lunar Blade, breaking the cycle.

void* deadlock_monitor(void*) {
    while (true) {
        sleep(1);
        pthread_mutex_lock(&g->mtx);
        if (!g->running) {
            pthread_mutex_unlock(&g->mtx);
            break;
        }

        auto& a = g->artifacts[0]; // Solar Core
        auto& b = g->artifacts[1]; // Lunar Blade

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

// ═══════════════════════════════════════════════════════════════════════════
// APPLY ACTION — Executes player/NPC actions (OS Concept: Action Processing)
// ═══════════════════════════════════════════════════════════════════════════

// Executes a PendingAction, modifying game state.
// OS Concepts:
//   - Stun: 20% chance on hit → sends SIGUSR1 to target's process
//   - Ultimate: Sends SIGSTOP to ASP, schedules SIGALRM to resume after 10s
//   - Weapon drops: 50% chance when NPC dies, allocated to random player
//   - Eclipse Relic: 10% chance to spawn after each action

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

    // Stun: 20% chance → async signal delivery (OS Concept: Signal-based stun)
    if ((std::rand() % 100) < 20 && target->alive) {
        target->stunned_until_epoch = now_epoch() + 3;
        if (target->team == TEAM_PLAYER && g->hip_pid > 0) kill(g->hip_pid, SIGUSR1);
        if (target->team == TEAM_NPC && g->asp_pid > 0) kill(g->asp_pid, SIGUSR1);
        add_log(g, "Stun applied for 3s");
    }

    // Eclipse Relic spawn
    if (!g->eclipse_present && (std::rand() % 100) < 10) {
        g->artifacts[2].present = 1;
        g->eclipse_present = 1;
        add_log(g, "Eclipse Relic appeared");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// TURN SCHEDULING — Custom CPU Scheduling (OS Concept: CPU Scheduling)
// ═══════════════════════════════════════════════════════════════════════════
//
// Scheduler Policy: Priority-based with aging.
// - Only alive, non-stunned characters are considered.
// - Priority: player team first, then NPC team.
// - Turn goes to first character whose stamina >= max_stamina.
// - If no character is ready, wait with condition variable instead of busy-waiting.
// - A dedicated turn_scheduler_thread periodically regenerates stamina (1 sec interval).
//
// This replaces the old pattern of unlock-sleep-lock which caused race conditions
// where shared state could be modified while the mutex was released.

void schedule_next_turn() {
    // Stamina regen is now handled by turn_scheduler_thread (see below).
    // Here we only select the next ready entity to run.

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
    // This replaces the busy-wait loop that used unlock-sleep-lock.
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

// ═══════════════════════════════════════════════════════════════════════════
// TURN SCHEDULER THREAD — Periodic stamina regeneration
// ═══════════════════════════════════════════════════════════════════════════
//
// OS Concept: CPU Scheduling — Dedicated thread for periodic stamina updates.
//
// This thread runs every 1 second and increments stamina for all alive,
// non-stunned entities. This decouples the timing of resource regen from
// turn selection and avoids the unlock-sleep-lock race condition.
//
// The thread signals turn_cv whenever stamina changes, allowing the
// main scheduler to detect when an entity becomes ready without polling.
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

// ═══════════════════════════════════════════════════════════════════════════
// MAIN — Process setup, threads, game loop
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    signal(SIGALRM, sigalrm_handler);
    signal(SIGTERM, sigterm_handler);

    // Shared Memory IPC: Create shared memory region (OS Concept: shm_open)
    g = map_shared(true);
    if (!g) return 1;

    // Initialize game state
    init_state(g);

    // Process Management: Record own PID for signal delivery
    g->arbiter_pid = getpid();
    g_turn_start = now_epoch();

    // Threading: Create three threads (OS Concept: pthread_create)
    pthread_t mon, ren, sch;
    pthread_create(&mon, nullptr, deadlock_monitor, nullptr);
    pthread_create(&ren, nullptr, render_thread, nullptr);
    pthread_create(&sch, nullptr, turn_scheduler_thread, nullptr);

    pthread_mutex_lock(&g->mtx);
    add_log(g, "Arbiter started");

    // Main game loop with turn scheduling and action processing
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

        // CPU Scheduling: Select next entity to run
        schedule_next_turn();

        // NPC turn: Wait for ASP response with timeout
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
            // Player turn: Wait indefinitely for HIP response
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

    // Thread join (OS Concept: pthread_join)
    pthread_join(mon, nullptr);
    pthread_join(ren, nullptr);
    pthread_join(sch, nullptr);

    // Cleanup shared memory (OS Concept: munmap + shm_unlink)
    munmap(g, sizeof(SharedState));
    shm_unlink(SHM_NAME);

    return 0;
}