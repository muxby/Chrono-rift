# Chrono Rift — OS Semester Project (CS2006)

A multi-process tactical game demonstrating core operating system concepts including process synchronization, inter-process communication, deadlocks, stun mechanics via signal delivery, and custom CPU scheduling.

## Group Members

- Aisha Ishtiaq (24K-0559)
- Fizza (24K-0562)
- Rameen Afzal (24K-0590)

## Quick Start

```bash
# Install dependencies
sudo apt-get update && sudo apt-get install -y libsfml-dev g++ make libncurses5-dev

# Build everything
make all

# Run (SFML recommended)
./sfml_ui/sfml_arbiter 42 3

# Or with launcher (prompts for input first, no stdin conflicts)
./sfml_ui/chrono_rift_standalone

# Or run in terminal mode (ncurses)
./sfml_ui/chrono_rift_standalone --ncurses
```

## Three Execution Modes

| Mode | Binary | Description |
|------|--------|-------------|
| **NCurses (Terminal)** | `./arbiter/arbiter` | Two views: Combat + Scheduler (Tab toggle) |
| **SFML Arbiter** | `./sfml_ui/sfml_arbiter` | Three views: Combat/Scheduler/Hybrid (1/2/3 keys) |
| **SFML Visualizer** | `./sfml_ui/chrono_rift_visualizer` | Passive, connects to running arbiter |

## OS Concepts Covered

| Concept | Where Demonstrated |
|---------|-------------------|
| Process Management | `standalone_main.cpp` — fork/exec of 3 processes |
| Shared Memory IPC | `common.hpp:map_shared()` — shm_open + mmap |
| Threading | `arbiter.cpp` — 3 threads: render, deadlock_monitor, turn_scheduler |
| Synchronization | `arbiter.cpp:init_state()` — pthread_mutex + condvar (PTHREAD_PROCESS_SHARED) |
| Signal-based Stun | `hip.cpp`, `asp.cpp` — SIGUSR1 async delivery |
| Deadlock Detection | `arbiter.cpp:deadlock_monitor()` — circular wait on artifacts |
| CPU Scheduling | `arbiter.cpp:schedule_next_turn()` — stamina-based priority scheduling |
| Real-time Visualization | `arbiter.cpp:render_thread()` — NCurses TUI with 2 views |

## File Structure

```
chrono_rift_merged/
├── shared.hpp              — Shared data structures & types
├── common.hpp              — Utility functions (shm_open, weapon ops)
├── arbiter/arbiter.cpp     — NCurses arbiter (game logic + 2-view TUI)
├── hip/hip.cpp             — Human Interfacing Process
├── asp/asp.cpp             — Automated Strategic Process (AI)
├── sfml_ui/
│   ├── sfml_arbiter.cpp    — SFML arbiter (game logic + Visualizer UI)
│   ├── main.cpp            — Passive SFML visualizer
│   ├── standalone_main.cpp — Multi-process launcher
│   ├── Visualizer.hpp/cpp  — SFML Visualizer (3 view modes)
│   ├── UIComponents.hpp/cpp — UI elements
│   └── UITheme.hpp         — Neon dark theme
├── Makefile                — Build system (all modes)
├── CMakeLists.txt          — CMake configuration
├── EXECUTION_GUIDE.md      — Full documentation
└── README.md               — This file
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

### NCurses Scheduler View
Added Tab-toggle to ncurses arbiter showing:
- Process blocks with state indicators (RUNNING/READY/BLOCKED)
- Gantt-style turn history
- OS metrics (process states, throughput)

## Build Targets

```bash
make all              # Everything
make legacy           # NCurses only
make sfml_arbiter     # SFML arbiter
make sfml             # Passive visualizer
make sfml_standalone  # Launcher
make clean            # Remove binaries
make help             # Show all targets
```

See `EXECUTION_GUIDE.md` for complete documentation.