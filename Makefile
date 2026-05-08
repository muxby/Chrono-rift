# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║  CHRONO RIFT — Complete Multi-Mode Build System                         ║
# ║  Three Modes: Terminal (ncurses) | SFML Visualizer | Standalone         ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

CXX         = g++
CXXFLAGS    = -Wall -Wextra -std=c++17 -pthread -O2
DEBUG_FLAGS = -Wall -Wextra -std=c++17 -pthread -g -DDEBUG

# SFML Libraries
SFML_LIBS   = -lsfml-graphics -lsfml-window -lsfml-system

# POSIX / System Libraries
SYS_LIBS    = -lrt

# Original ncurses libraries (for legacy arbiter build)
NCURSES_LIBS = -lncurses

# ═══════════════════════════════════════════════════════════════════════════
# TARGETS
# ═══════════════════════════════════════════════════════════════════════════

# Legacy modules (ncurses)
LEGACY_TARGETS = arbiter/arbiter hip/hip asp/asp

# SFML Arbiter (complete game logic + Visualizer UI)
SFML_ARBITER = sfml_ui/sfml_arbiter

# SFML Visualizer (connects to running arbiter)
SFML_TARGET  = sfml_ui/chrono_rift_visualizer

# Standalone launcher (forks all processes)
SFML_STANDALONE = sfml_ui/chrono_rift_standalone

# All targets
ALL_TARGETS  = $(LEGACY_TARGETS) $(SFML_ARBITER) $(SFML_TARGET) $(SFML_STANDALONE)

# ═══════════════════════════════════════════════════════════════════════════
# SOURCE FILES
# ═══════════════════════════════════════════════════════════════════════════

# Visualizer UI sources (shared between sfml_arbiter and visualizer)
VISUALIZER_SOURCES = \
	sfml_ui/Visualizer.cpp \
	sfml_ui/UIComponents.cpp \
	sfml_ui/LogPanel.cpp

VISUALIZER_OBJECTS = $(VISUALIZER_SOURCES:.cpp=.o)

# ═══════════════════════════════════════════════════════════════════════════
# BUILD RULES
# ═══════════════════════════════════════════════════════════════════════════

.PHONY: all clean debug sfml sfml_standalone sfml_arbiter legacy install-deps help \
        run run-sfml-arbiter run-standalone run-legacy run-visualizer

# Default: build everything
all: legacy sfml_arbiter sfml sfml_standalone
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════════╗"
	@echo "║  Chrono Rift — Build Complete!                                    ║"
	@echo "╠═══════════════════════════════════════════════════════════════════╣"
	@echo "║                                                                   ║"
	@echo "║  MODE 1: Terminal (ncurses) — ./arbiter/arbiter                   ║"
	@echo "║    make run-legacy                                                ║"
	@echo "║                                                                   ║"
	@echo "║  MODE 2: SFML Arbiter (Full Game + Visualizer UI) —              ║"
	@echo "║    ./sfml_ui/sfml_arbiter                                         ║"
	@echo "║    make run-sfml-arbiter   (manual multi-terminal)                ║"
	@echo "║    make run-standalone     (auto-launches all processes)          ║"
	@echo "║                                                                   ║"
	@echo "║  MODE 3: Passive Visualizer (connects to arbiter) —              ║"
	@echo "║    ./sfml_ui/chrono_rift_visualizer                               ║"
	@echo "║    make run-visualizer                                            ║"
	@echo "║                                                                   ║"
	@echo "║  Standalone Launcher:                                             ║"
	@echo "║    ./sfml_ui/chrono_rift_standalone                               ║"
	@echo "╚═══════════════════════════════════════════════════════════════════╝"

# ── Legacy targets (original ncurses-based modules) ───────────────────────
legacy: $(LEGACY_TARGETS)

arbiter/arbiter: arbiter/arbiter.cpp shared.hpp common.hpp
	@mkdir -p arbiter
	$(CXX) $(CXXFLAGS) arbiter/arbiter.cpp -o $@ $(NCURSES_LIBS) $(SYS_LIBS)
	@echo "[OK] Built: arbiter/arbiter"

hip/hip: hip/hip.cpp shared.hpp common.hpp
	@mkdir -p hip
	$(CXX) $(CXXFLAGS) hip/hip.cpp -o $@ $(SYS_LIBS)
	@echo "[OK] Built: hip/hip"

asp/asp: asp/asp.cpp shared.hpp common.hpp
	@mkdir -p asp
	$(CXX) $(CXXFLAGS) asp/asp.cpp -o $@ $(SYS_LIBS)
	@echo "[OK] Built: asp/asp"

# ── SFML Arbiter (complete game logic + Visualizer UI) ───────────────────
sfml_arbiter: $(SFML_ARBITER)

$(SFML_ARBITER): sfml_ui/sfml_arbiter.cpp $(VISUALIZER_OBJECTS) shared.hpp common.hpp
	@mkdir -p sfml_ui
	$(CXX) $(CXXFLAGS) sfml_ui/sfml_arbiter.cpp $(VISUALIZER_OBJECTS) -o $@ $(SFML_LIBS) $(SYS_LIBS)
	@echo "[OK] Built: $(SFML_ARBITER)"

# ── SFML Visualizer (connects to shared memory) ───────────────────────────
sfml: $(SFML_TARGET)

VISUALIZER_ONLY_SOURCES = \
	sfml_ui/main.cpp \
	sfml_ui/Visualizer.cpp \
	sfml_ui/UIComponents.cpp \
	sfml_ui/LogPanel.cpp

VISUALIZER_ONLY_OBJECTS = $(VISUALIZER_ONLY_SOURCES:.cpp=.o)

$(SFML_TARGET): $(VISUALIZER_ONLY_OBJECTS) shared.hpp common.hpp
	@mkdir -p sfml_ui
	$(CXX) $(CXXFLAGS) $(VISUALIZER_ONLY_OBJECTS) -o $@ $(SFML_LIBS) $(SYS_LIBS)
	@echo "[OK] Built: $(SFML_TARGET)"

# ── SFML Standalone (forks real processes) ────────────────────────────────
sfml_standalone: $(SFML_STANDALONE)

$(SFML_STANDALONE): sfml_ui/standalone_main.cpp shared.hpp common.hpp
	@mkdir -p sfml_ui
	$(CXX) $(CXXFLAGS) sfml_ui/standalone_main.cpp -o $@ $(SYS_LIBS)
	@echo "[OK] Built: $(SFML_STANDALONE)"

# ── Object compilation rules ──────────────────────────────────────────────
sfml_ui/%.o: sfml_ui/%.cpp sfml_ui/*.hpp shared.hpp common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ═══════════════════════════════════════════════════════════════════════════
# RUN TARGETS
# ═══════════════════════════════════════════════════════════════════════════

# ── Mode 1: Legacy (ncurses arbiter + hip + asp) ──────────────────────────
# CRITICAL FIX: Prompt for input BEFORE launching to avoid stdin conflicts
run-legacy: legacy
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  MODE 1: Terminal (ncurses)                                  ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "Enter roll number seed: \c"
	@read roll; \
	echo "Enter party size (1-4): \c"; \
	read party; \
	echo ""; \
	echo "Starting Chrono Rift (ncurses mode) with roll=$$roll, party=$$party..."; \
	./arbiter/arbiter & \
	ARBITER_PID=$$!; \
	sleep 1; \
	./hip/hip & \
	HIP_PID=$$!; \
	./asp/asp & \
	ASP_PID=$$!; \
	wait $$ARBITER_PID; \
	kill $$HIP_PID $$ASP_PID 2>/dev/null; \
	wait

# ── Mode 2a: SFML Arbiter (manual multi-terminal) ─────────────────────────
run-sfml-arbiter: sfml_arbiter
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  MODE 2a: SFML Arbiter (manual multi-terminal)               ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "Launching SFML Arbiter..."
	@echo "NOTE: Run ./hip/hip and ./asp/asp in separate terminals."
	./$(SFML_ARBITER)

# ── Mode 2b: SFML Arbiter (auto-launches all processes) ───────────────────
# CRITICAL FIX: Launcher prompts for input BEFORE forking
run-standalone: all
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  MODE 2b: SFML Arbiter + Auto-launch                         ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	./$(SFML_STANDALONE)

# ── Mode 2c: SFML Arbiter with direct arguments (no prompts) ──────────────
run-sfml-direct: sfml_arbiter
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  MODE 2c: SFML Arbiter (direct arguments)                    ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	./$(SFML_ARBITER) 42 3

# ── Mode 3: Passive Visualizer (connects to running arbiter) ──────────────
run-visualizer: sfml
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  MODE 3: Passive Visualizer                                  ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	./$(SFML_TARGET)

# ═══════════════════════════════════════════════════════════════════════════
# UTILITY TARGETS
# ═══════════════════════════════════════════════════════════════════════════

# ── Debug build ───────────────────────────────────────────────────────────
debug: CXXFLAGS := $(DEBUG_FLAGS)
debug: all

# ── Clean ─────────────────────────────────────────────────────────────────
clean:
	rm -f $(ALL_TARGETS)
	rm -f sfml_ui/*.o
	@echo "[OK] Cleaned all build artifacts"

# ── Install dependencies (Ubuntu/Debian) ──────────────────────────────────
install-deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y libsfml-dev g++ make libncurses5-dev
	@echo "[OK] Dependencies installed"

# ── Help ──────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════════╗"
	@echo "║  Chrono Rift — Multi-Mode Build System                            ║"
	@echo "╠═══════════════════════════════════════════════════════════════════╣"
	@echo "║                                                                   ║"
	@echo "║  BUILD TARGETS:                                                   ║"
	@echo "║    make all              — Build everything (all 3 modes)         ║"
	@echo "║    make legacy           — Build arbiter/hip/asp (terminal)       ║"
	@echo "║    make sfml_arbiter     — Build SFML arbiter (Mode 2)            ║"
	@echo "║    make sfml             — Build passive visualizer (Mode 3)      ║"
	@echo "║    make sfml_standalone  — Build standalone launcher              ║"
	@echo "║    make debug            — Build with debug symbols               ║"
	@echo "║    make clean            — Remove all build artifacts             ║"
	@echo "║    make install-deps     — Install system dependencies            ║"
	@echo "║                                                                   ║"
	@echo "║  RUN TARGETS:                                                     ║"
	@echo "║    make run-legacy       — Terminal mode (ncurses)                ║"
	@echo "║    make run-sfml-arbiter — SFML arbiter (manual, prompts input)   ║"
	@echo "║    make run-standalone   — Auto-launch all (prompts first)        ║"
	@echo "║    make run-sfml-direct  — SFML arbiter (roll=42, party=3)        ║"
	@echo "║    make run-visualizer   — Passive visualizer only                ║"
	@echo "║                                                                   ║"
	@echo "║  THREE MODES:                                                     ║"
	@echo "║    Mode 1: Terminal    — ./arbiter/arbiter + ./hip/hip + ./asp/asp║"
	@echo "║    Mode 2: SFML Arbiter— ./sfml_ui/sfml_arbiter (+ hip + asp)    ║"
	@echo "║    Mode 3: Visualizer  — ./sfml_ui/chrono_rift_visualizer         ║"
	@echo "║                                                                   ║"
	@echo "║  The standalone launcher fixes terminal input timing:             ║"
	@echo "║    ./sfml_ui/chrono_rift_standalone                               ║"
	@echo "║                                                                   ║"
	@echo "║  SFML Arbiter accepts command-line arguments:                     ║"
	@echo "║    ./sfml_ui/sfml_arbiter <roll_number> <party_size>             ║"
	@echo "║    Example: ./sfml_ui/sfml_arbiter 42 3                          ║"
	@echo "╚═══════════════════════════════════════════════════════════════════╝"
