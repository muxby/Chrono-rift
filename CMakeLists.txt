# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║  CHRONO RIFT — Complete Multi-Mode CMake Build                          ║
# ║  Three Modes: Terminal (ncurses) | SFML Visualizer | Standalone        ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

cmake_minimum_required(VERSION 3.16)
project(ChronoRift VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find SFML ─────────────────────────────────────────────────────────────
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# ── Find Threads ──────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ═══════════════════════════════════════════════════════════════════════════
# MODE 1: LEGACY TARGETS (Terminal ncurses Arbiter + HIP + ASP)
# ═══════════════════════════════════════════════════════════════════════════

# Arbiter (ncurses TUI)
add_executable(arbiter_original arbiter/arbiter.cpp)
target_include_directories(arbiter_original PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(arbiter_original PRIVATE ncurses Threads::Threads rt)
set_target_properties(arbiter_original PROPERTIES OUTPUT_NAME "arbiter/arbiter")

# HIP (Human Interface Player)
add_executable(hip_original hip/hip.cpp)
target_include_directories(hip_original PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(hip_original PRIVATE Threads::Threads rt)
set_target_properties(hip_original PROPERTIES OUTPUT_NAME "hip/hip")

# ASP (AI Simulation Player)
add_executable(asp_original asp/asp.cpp)
target_include_directories(asp_original PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(asp_original PRIVATE Threads::Threads rt)
set_target_properties(asp_original PROPERTIES OUTPUT_NAME "asp/asp")

# ═══════════════════════════════════════════════════════════════════════════
# MODE 2: SFML ARBITER (Complete Game + Visualizer UI)
# ═══════════════════════════════════════════════════════════════════════════

# Visualizer UI library (shared)
set(VISUALIZER_SOURCES
    sfml_ui/Visualizer.cpp
    sfml_ui/UIComponents.cpp
    sfml_ui/LogPanel.cpp
)

add_library(chrono_rift_visualizer_lib STATIC ${VISUALIZER_SOURCES})

target_include_directories(chrono_rift_visualizer_lib PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sfml_ui
)

target_link_libraries(chrono_rift_visualizer_lib PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)

# SFML Arbiter (uses visualizer UI + full game logic)
add_executable(sfml_arbiter sfml_ui/sfml_arbiter.cpp)

target_include_directories(sfml_arbiter PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sfml_ui
)

target_link_libraries(sfml_arbiter PRIVATE
    chrono_rift_visualizer_lib
    sfml-graphics
    sfml-window
    sfml-system
    Threads::Threads
    rt
)

set_target_properties(sfml_arbiter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sfml_ui
)

# ═══════════════════════════════════════════════════════════════════════════
# MODE 3: PASSIVE SFML VISUALIZER (connects to shared memory)
# ═══════════════════════════════════════════════════════════════════════════

add_executable(chrono_rift_visualizer
    sfml_ui/main.cpp
)

target_include_directories(chrono_rift_visualizer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sfml_ui
)

target_link_libraries(chrono_rift_visualizer PRIVATE
    chrono_rift_visualizer_lib
    sfml-graphics
    sfml-window
    sfml-system
    Threads::Threads
    rt
)

set_target_properties(chrono_rift_visualizer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sfml_ui
)

# ═══════════════════════════════════════════════════════════════════════════
# STANDALONE LAUNCHER (forks all processes, no SFML libs needed)
# ═══════════════════════════════════════════════════════════════════════════

add_executable(chrono_rift_standalone sfml_ui/standalone_main.cpp)

target_include_directories(chrono_rift_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(chrono_rift_standalone PRIVATE
    Threads::Threads
    rt
)

set_target_properties(chrono_rift_standalone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sfml_ui
)

# ═══════════════════════════════════════════════════════════════════════════
# COMPILER FLAGS
# ═══════════════════════════════════════════════════════════════════════════

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chrono_rift_visualizer_lib PRIVATE -Wall -Wextra -O2)
    target_compile_options(chrono_rift_visualizer PRIVATE -Wall -Wextra -O2)
    target_compile_options(sfml_arbiter PRIVATE -Wall -Wextra -O2)
    target_compile_options(chrono_rift_standalone PRIVATE -Wall -Wextra -O2)
    target_compile_options(arbiter_original PRIVATE -Wall -Wextra)
    target_compile_options(hip_original PRIVATE -Wall -Wextra)
    target_compile_options(asp_original PRIVATE -Wall -Wextra)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# INSTALL
# ═══════════════════════════════════════════════════════════════════════════

install(TARGETS
    sfml_arbiter
    chrono_rift_visualizer
    chrono_rift_standalone
    arbiter_original
    hip_original
    asp_original
    RUNTIME DESTINATION bin
)
