# Chrono Rift — OS Semester Project (CS2006)

A multi-process tactical game demonstrating core operating system concepts including process synchronization, inter-process communication, deadlocks, stun mechanics via signal delivery, and custom CPU scheduling.

## Group Members
Mubeen Khalid (24i-0605)
Muhammad Abubakar (24i-0753)


## Quick Start

```bash
# Install dependencies
sudo apt-get update && sudo apt-get install -y libsfml-dev g++ make

# Build everything
make all

# Run (recommended)
./sfml_ui/sfml_arbiter 42 3

# Or with launcher (prompts for input first, no stdin conflicts)
./sfml_ui/chrono_rift_standalone
```

## Execution Modes

| Mode | Binary | Description |
|------|--------|-------------|
| **SFML Arbiter** | `./sfml_ui/sfml_arbiter` | Three views: Combat/Scheduler/Hybrid (1/2/3 keys) |
| **SFML Visualizer** | `./sfml_ui/chrono_rift_visualizer` | Passive, connects to running arbiter |

## OS Concepts Covered

| Concept | Where Demonstrated |
|---------|-------------------|
| Process Management | `standalone_main.cpp` — fork/exec of 3 processes |
| Shared Memory IPC | `common.hpp:map_shared()` — shm_open + mmap |
| Threading | `sfml_arbiter.cpp` — 3 threads: render, deadlock_monitor, turn_scheduler |
| Synchronization | `sfml_arbiter.cpp:init_state()` — pthread_mutex + condvar (PTHREAD_PROCESS_SHARED) |
| Signal-based Stun | `hip.cpp`, `asp.cpp` — SIGUSR1 async delivery |
| Deadlock Detection | `sfml_arbiter.cpp:deadlock_monitor()` — circular wait on artifacts |
| CPU Scheduling | `sfml_arbiter.cpp:schedule_next_turn()` — stamina-based priority scheduling |
| Real-time Visualization | `Visualizer.cpp` — SFML UI with 3 view modes |

## UI Overview

The game features a full SFML-based graphical interface with three view modes switchable via keyboard (1/2/3 keys):

| Mode | Key | Description |
|------|-----|-------------|
| **Combat View** | `1` | UFC-style cage view with animated fighter sprites, HP bars, stamina bars, and team/opponent rosters. Player attacks are submitted via keyboard (Q/W/E/R for weapon selection). |
| **Scheduler View** | `2` | Process control panel showing process blocks (states: running/ready/blocked/dead), Gantt chart, CPU metrics (throughput, waiting time, turnaround time), and scheduling controls (STAMINA/RR/FIFO/PRIORITY). |
| **Hybrid View** | `3` | Split-screen combining combat visuals and scheduler metrics side-by-side. |

### UI Features
- **Intro Animation** — Fullscreen video-like intro sequence played on launch
- **Background Video** — Animated background rendered from a 121-frame image sequence at 30 FPS
- **Fighter Sprites** — Animated sprite sheets for 4 players and 4 enemies with attack animations
- **Banners** — Team banner images displayed in roster panels
- **Character Cards** — Live stats cards for each player and NPC showing HP, stamina, artifacts held
- **Particle System** — Visual effects for attacks and events
- **Connection Lines** — Visual mapping between processes and their states
- **Deadlock Controls** — Toggle detection strategies: DETECT / NO_HOLD_WAIT / PREEMPT
- **Theme** — Clean white/muted professional theme (defined in `UITheme.hpp`)

## File Structure

```
chrono_rift/
│
├── shared.hpp              — Shared data structures, enums & types
├── common.hpp              — Utility functions (shm_open, weapon ops, helpers)
│
├── hip/
│   └── hip.cpp             — Human Interfacing Process (player input)
│
├── asp/
│   └── asp.cpp             — Automated Strategic Process (AI opponent)
│
├── sfml_ui/                          — SFML graphical interface
│   ├── sfml_arbiter.cpp             — Main arbiter: game logic, threading, UI
│   ├── Visualizer.hpp / .cpp        — Core visualizer (3 view modes, rendering)
│   ├── UIComponents.hpp / .cpp      — Reusable UI elements (cards, buttons, sliders)
│   ├── UITheme.hpp                  — Color scheme & layout constants
│   ├── SpriteAnimation.hpp / .cpp   — Animated sprite sheet player
│   ├── ThreadPool.hpp / .cpp        — Thread pool for async tasks
│   ├── LogPanel.hpp / .cpp          — Scrollable log output panel
│   ├── StandaloneSim.hpp / .cpp     — Standalone simulation manager
│   ├── main.cpp                     — Passive visualizer entry point
│   ├── standalone_main.cpp          — Multi-process launcher (fork/exec)
│   └── os_helpers.hpp               — OS-level helper functions
│
├── SPRITESHEET/                     — Game sprite assets
│   ├── Players/
│   │   ├── player_standing_sprites/ — Idle stance sprites (4 fighters)
│   │   └── player_attack/           — Attack animation sprite sheets
│   ├── enemy/
│   │   ├── enemy standing/          — Enemy idle stance sprites
│   │   └── enemy_attack/            — Enemy attack animation sheets
│   └── banners/                     — Team banner portraits
│
├── background/                      — Animated background frames (121 JPGs)
├── intro_frames/                    — Intro animation frames (121 JPGs)
│
├── scratch/
│   └── check_seed.cpp              — Seed testing utility
│
├── Dockerfile                       — Containerized build & run
├── Makefile                         — Build system
├── requirements.txt                 — System dependency list
├── EXECUTION_GUIDE.md               — Full documentation
└── README.md                        — This file
```

## Key Implementation Details

### CPU Scheduling Fix
The original `schedule_next_turn()` had an unlock-sleep-lock anti-pattern (race condition). Fixed by:
- Dedicated `turn_scheduler_thread` that regenerates stamina every 1 second
- `schedule_next_turn()` now waits on `turn_cv` condition variable instead of busy-waiting
- Stamina regen happens atomically inside the scheduler thread

### HIP Input Fix
Original `hip.cpp` held the mutex while reading from stdin. Fixed by:
- Reading stdin outside the mutex lock
- Submitting the action atomically inside the critical section

## Build Targets

```bash
make all              # Everything
make sfml_arbiter     # SFML arbiter
make sfml             # Passive visualizer
make sfml_standalone  # Launcher
make clean            # Remove binaries
make help             # Show all targets
```

See `EXECUTION_GUIDE.md` for complete documentation.
