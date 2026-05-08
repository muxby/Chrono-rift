# Chrono Rift — Execution Guide

## Overview

**Chrono Rift** is a multi-process tactical game demonstrating core operating system concepts through an interactive battle system. Three processes (Arbiter, HIP, ASP) communicate via POSIX shared memory, with synchronization primitives, signal-based interrupts, deadlock detection, and custom CPU scheduling.

### OS Concepts Demonstrated

| OS Concept | Implementation | Location |
|-----------|---------------|----------|
| **Process Management** | fork/exec for 3 processes; PIDs tracked in shared memory | `standalone_main.cpp`, `shared.hpp` |
| **Shared Memory IPC** | shm_open + mmap; `/dev/shm/chrono_rift_shm` | `common.hpp:map_shared()` |
| **Threading** | 3 threads per arbiter (render, deadlock_monitor, turn_scheduler) | `arbiter.cpp` |
| **Synchronization** | pthread_mutex + pthread_cond (PTHREAD_PROCESS_SHARED) | `arbiter.cpp:init_state()` |
| **Signal-based Stun** | SIGUSR1 async delivery; SIGALRM for Ultimate pause | `arbiter.cpp`, `hip.cpp`, `asp.cpp` |
| **Deadlock Detection** | Circular-wait detection on Solar Core + Lunar Blade | `arbiter.cpp:deadlock_monitor()` |
| **CPU Scheduling** | Stamina-based priority scheduling via dedicated thread | `arbiter.cpp:schedule_next_turn()` |
| **Real-time Visualization** | NCurses TUI (Combat + Scheduler views) + SFML Visualizer | `arbiter.cpp:render_thread()` |

---

## Building

### Dependencies

```bash
sudo apt-get update
sudo apt-get install -y libsfml-dev g++ make libncurses5-dev libncursesw5-dev
```

### Build Targets

```bash
make all              # Build everything (ncurses + SFML)
make legacy           # Terminal modules only (arbiter + hip + asp)
make sfml_arbiter     # SFML arbiter with Visualizer UI
make sfml             # Passive SFML visualizer
make sfml_standalone  # Standalone launcher
make debug            # Debug build with symbols
make clean            # Remove all build artifacts
make install-deps     # Install system dependencies
```

---

## Three Execution Modes

### Mode 1: Terminal (ncurses) — **Two Views**

The original terminal-based UI using ncurses. Press **Tab** to toggle between:
- **Combat View** [Tab]: Character cards, HP/stamina bars, inventory, artifacts, logs
- **Scheduler View** [Tab]: Process blocks, Gantt timeline, state indicators (READY/RUNNING/BLOCKED), OS metrics

```bash
# Build
make legacy

# Run (recommended: use launcher to avoid stdin conflicts)
./sfml_ui/chrono_rift_standalone --ncurses
# or via Makefile (prompts for input first):
make run-legacy

# Manual multi-terminal:
./arbiter/arbiter    # Terminal 1: Enter roll and party size when prompted
./hip/hip             # Terminal 2: Player input
./asp/asp             # Terminal 3: AI opponent
```

**Controls:**
- `Tab` — Toggle between Combat and Scheduler views
- `Ctrl+C` — Force quit all processes

**Combat View Features:**
- Color-coded panels (green=players, red=NPCs)
- HP bars with blinking critical indicator
- Stamina pip display
- Inventory bar with weapon abbreviations (SC, LB, IH, VD, etc.)
- Artifact status with waiting queues
- Recent log panel

**Scheduler View Features:**
- Process blocks for each NPC (state: RUNNING/READY/BLOCKED)
- Resource slots for each player (state: RUNNING/READY/BLOCKED)
- Gantt-style turn history timeline
- OS metrics: process states, throughput, kills/turn

---

### Mode 2: SFML Arbiter (Full Game + Visualizer UI) — **RECOMMENDED**

The complete game with beautiful SFML visualization. Three view modes via keyboard:

```bash
# Build everything
make all

# Option A: Interactive (prompts for roll and party)
./sfml_ui/sfml_arbiter

# Option B: Direct arguments (no prompts)
./sfml_ui/sfml_arbiter 42 3

# Option C: Auto-launch with standalone launcher
./sfml_ui/chrono_rift_standalone
# Prompts for roll and party BEFORE forking (no stdin conflicts)

# Option D: Manual multi-terminal
./sfml_ui/sfml_arbiter 42 3   # Terminal 1
./hip/hip                    # Terminal 2
./asp/asp                    # Terminal 3
```

**Controls:**
- `1` — Combat View (character cards, HP/stamina, artifacts)
- `2` — Scheduler View (process blocks, HIP slots, Gantt chart)
- `3` — Hybrid View (combined combat + scheduler)
- `Space` — Spawn particle effect
- `Click` — Particle effect at cursor
- `ESC` — Exit

---

### Mode 3: Passive SFML Visualizer

A standalone visualizer that connects to an already-running arbiter.

```bash
# Build
make sfml

# First, start any arbiter:
./sfml_ui/sfml_arbiter 42 3   # or ./arbiter/arbiter

# Then, in another terminal:
./sfml_ui/chrono_rift_visualizer
```

---

### Mode 4: Docker (WSL)

Run the entire multi-process game inside a container. This is the most reliable way to handle dependencies and GUI setup on Windows.

```bash
# 1. Build the image
docker build -t chrono-rift .

# 2. Run the container (Windows 11 / WSLg)
docker run -it --rm \
    -e DISPLAY=:0 \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    --shm-size=128m \
    chrono-rift

# 3. Alternative: Using Docker Compose
docker-compose run --rm game
```

**Note for Windows 10:** If not using WSLg, you must start **VcXsrv** with "Disable access control" and set `DISPLAY` to your host IP (e.g., `-e DISPLAY=172.xx.xx.xx:0.0`).

---

## Game Mechanics

### Player Actions (8 total)

| Action | Key | Effect |
|--------|-----|--------|
| Strike | 1 | Base damage attack |
| Exhaust | 2 | Drain target stamina |
| Use Weapon | 3 | 55 dmg (Iron Halberd) |
| Swap In | 4 | Retrieve weapon from storage |
| Heal | 5 | Restore 10% HP |
| Skip | 6 | Skip turn, retain 50% stamina |
| Ultimate | 7 | Requires Solar Core + Lunar Blade; pauses ASP 10s |
| Quit | 8 | Exit game |

### Stamina System

- Each second, all alive entities gain stamina equal to their **speed** stat
- Entity can act only when stamina reaches **max_stamina**
- After acting, stamina resets to 0 (except Skip which retains 50%)
- A dedicated `turn_scheduler_thread` handles stamina regeneration every 1 second
- Scheduling uses condition variable wait (not busy-wait) for efficiency

### Stun Mechanic (OS Concept: Signal Delivery)

- 20% chance on successful attack
- Target is stunned for 3 seconds (SIGUSR1 delivered to HIP/ASP)
- Stunned entities skip their turn automatically
- Signal handler atomically stores `stunned_until` timestamp (async-signal-safe)

### Artifacts & Deadlock (OS Concept: Deadlock Detection)

| Artifact | Slots | Damage | Notes |
|----------|-------|--------|-------|
| Solar Core | 10 | 95 | Required for Ultimate |
| Lunar Blade | 10 | 90 | Required for Ultimate |
| Eclipse Relic | 3 | 60 | Spawns randomly (10% chance) |

**Deadlock Detection:** `deadlock_monitor` thread runs every 1 second checking for circular wait. If both Solar Core and Lunar Blade are held by **different** teams, the arbiter forces Lunar Blade release.

### Win/Lose Conditions

- **Win**: Kill 10 NPCs
- **Lose**: All players die
- **Quit**: SIGTERM to arbiter

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  STANDALONE LAUNCHER (standalone_main.cpp)                       │
│  • Prompts for input BEFORE forking (no stdin conflicts)        │
│  • fork() + execl() three processes                             │
└──────────┬─────────────────┬─────────────────┬──────────────────┘
           │                 │                 │
    ┌──────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐
    │  ARBITER    │   │     HIP     │   │     ASP     │
    │ (ncurses or │   │ (Human      │   │ (AI         │
    │  sfml_ui)   │   │  Players)   │   │  Opponents) │
    │             │   │             │   │             │
    │ Threads:    │   │ Threads:    │   │ Threads:    │
    │ • render    │   │ • player[0]│   │ • npc[0..n]  │
    │ • deadlock  │   │ • player[1]│   │             │
    │ • scheduler │   │ • ...       │   │             │
    └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
           │                 │                 │
           └────────────┬────┴──────────────────┘
                        │
              ┌─────────▼──────────┐
              │  /dev/shm/chrono_  │  SharedState
              │  rift_shm          │  • mtx (mutex)
              │                     │  • turn_cv (condvar)
              │                     │  • action_cv (condvar)
              │                     │  • players[4]
              │                     │  • npcs[9]
              │                     │  • artifacts[3]
              └─────────────────────┘  • pending action
                                         • logs[64]
```

---

## Terminal Input Timing Fix

### The Problem

When launching multiple processes manually, stdin becomes contested:
1. Arbiter prompts for "roll number" and "party size"
2. Before user enters input, HIP/ASP processes also read from stdin
3. Input gets consumed by wrong process

### The Solution

**For ncurses mode:** Use `./sfml_ui/chrono_rift_standalone --ncurses` which pipes input to the arbiter.

**For SFML mode:** The standalone launcher prompts for input **BEFORE** forking, then passes values as command-line arguments.

**For manual mode:** Launch arbiter first, wait 1 second, then launch HIP and ASP.

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `shm_open: No such file or directory` | Run arbiter first (creates shared memory) |
| `Failed to load font` | Install fonts: `sudo apt-get install fonts-dejavu` |
| SFML window not appearing | Check X11 forwarding (WSL: install VcXsrv) |
| Input goes to wrong process | Use `make run-legacy` or standalone launcher |
| Build fails on Windows | Use WSL, MSYS2, or Docker environment |

---

## File Structure

```
chrono_rift_merged/
├── shared.hpp              — Shared data structures & types
├── common.hpp              — Utility functions (shm_open, weapon ops)
│
├── arbiter/
│   └── arbiter.cpp         — NCurses arbiter (game logic + 2-view TUI)
│
├── hip/
│   └── hip.cpp             — Human Interfacing Process (player input)
│
├── asp/
│   └── asp.cpp             — Automated Strategic Process (AI opponent)
│
├── sfml_ui/
│   ├── sfml_arbiter.cpp    — SFML Arbiter (game logic + Visualizer UI)
│   ├── main.cpp            — Passive visualizer (connects to arbiter)
│   ├── standalone_main.cpp — Multi-process launcher (ncurses + SFML modes)
│   ├── Visualizer.hpp/cpp  — Full SFML Visualizer (3 view modes)
│   ├── UIComponents.hpp/cpp — UI elements (CharacterCard, Button, etc.)
│   ├── UITheme.hpp         — Neon dark theme colors & constants
│   ├── LogPanel.cpp        — Log message rendering
│   ├── StandaloneSim.hpp/cpp — Demo simulation (no processes)
│   └── chrono_rift_standalone — Standalone launcher binary
│
├── Makefile                — Complete multi-mode build system
├── CMakeLists.txt          — CMake build configuration
├── EXECUTION_GUIDE.md      — This file
└── README.md               — Project documentation
```