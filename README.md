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

## File Structure

```
chrono_rift/
├── shared.hpp              — Shared data structures & types
├── common.hpp              — Utility functions (shm_open, weapon ops)
├── hip/
│   └── hip.cpp             — Human Interfacing Process
├── asp/
│   └── asp.cpp             — Automated Strategic Process (AI)
├── sfml_ui/
│   ├── sfml_arbiter.cpp    — SFML arbiter (game logic + Visualizer UI)
│   ├── main.cpp            — Passive SFML visualizer
│   ├── standalone_main.cpp — Multi-process launcher
│   ├── Visualizer.hpp/cpp  — SFML Visualizer (3 view modes)
│   ├── UIComponents.hpp/cpp — UI elements
│   └── UITheme.hpp         — Neon dark theme
├── Makefile                — Build system
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
