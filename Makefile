# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║  CHRONO RIFT — SFML Build System                                        ║
# ║  Modes: SFML Arbiter | SFML Visualizer | Standalone Launcher            ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

CXX         = g++
CXXFLAGS    = -Wall -Wextra -std=c++17 -pthread -O2
DEBUG_FLAGS = -Wall -Wextra -std=c++17 -pthread -g -DDEBUG

# SFML Libraries
SFML_LIBS   = -lsfml-graphics -lsfml-window -lsfml-system

# POSIX / System Libraries
SYS_LIBS    = -lrt

# ═══════════════════════════════════════════════════════════════════════════
# TARGETS
# ═══════════════════════════════════════════════════════════════════════════

# Process modules (no UI dependency)
PROCESS_TARGETS = hip/hip asp/asp

# SFML Arbiter (complete game logic + Visualizer UI)
SFML_ARBITER = sfml_ui/sfml_arbiter

# SFML Visualizer (connects to running arbiter)
SFML_TARGET  = sfml_ui/chrono_rift_visualizer

# Standalone launcher (forks all processes)
SFML_STANDALONE = sfml_ui/chrono_rift_standalone

# All targets
ALL_TARGETS  = $(PROCESS_TARGETS) $(SFML_ARBITER) $(SFML_TARGET) $(SFML_STANDALONE)

# ═══════════════════════════════════════════════════════════════════════════
# SOURCE FILES
# ═══════════════════════════════════════════════════════════════════════════

# Visualizer UI sources (shared between sfml_arbiter and visualizer)
VISUALIZER_SOURCES = \
	sfml_ui/Visualizer.cpp \
	sfml_ui/UIComponents.cpp \
	sfml_ui/LogPanel.cpp \
	sfml_ui/ThreadPool.cpp \
	sfml_ui/SpriteAnimation.cpp

VISUALIZER_OBJECTS = $(VISUALIZER_SOURCES:.cpp=.o)

# ═══════════════════════════════════════════════════════════════════════════
# BUILD RULES
# ═══════════════════════════════════════════════════════════════════════════

.PHONY: all clean debug sfml sfml_standalone sfml_arbiter install-deps help \
        run run-sfml-arbiter run-standalone run-visualizer test check

# Default: build everything
all: $(PROCESS_TARGETS) sfml_arbiter sfml sfml_standalone
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════════╗"
	@echo "║  Chrono Rift — Build Complete!                                    ║"
	@echo "╠═══════════════════════════════════════════════════════════════════╣"
	@echo "║                                                                   ║"
	@echo "║  MODE 1: SFML Arbiter (Full Game + Visualizer UI) —              ║"
	@echo "║    ./sfml_ui/sfml_arbiter                                         ║"
	@echo "║    make run-sfml-arbiter   (manual multi-terminal)                ║"
	@echo "║    make run-standalone     (auto-launches all processes)          ║"
	@echo "║                                                                   ║"
	@echo "║  MODE 2: Passive Visualizer (connects to arbiter) —              ║"
	@echo "║    ./sfml_ui/chrono_rift_visualizer                               ║"
	@echo "║    make run-visualizer                                            ║"
	@echo "║                                                                   ║"
	@echo "║  Standalone Launcher:                                             ║"
	@echo "║    ./sfml_ui/chrono_rift_standalone                               ║"
	@echo "║  Smoke Test:   make test                                           ║"
	@echo "╚═══════════════════════════════════════════════════════════════════╝"

# ── Process targets (HIP + ASP, no UI dependency) ─────────────────────────
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

$(SFML_ARBITER): sfml_ui/sfml_arbiter.cpp memory_demo.cpp $(VISUALIZER_OBJECTS) shared.hpp common.hpp
	@mkdir -p sfml_ui
	$(CXX) $(CXXFLAGS) sfml_ui/sfml_arbiter.cpp memory_demo.cpp $(VISUALIZER_OBJECTS) -o $@ $(SFML_LIBS) $(SYS_LIBS)
	@echo "[OK] Built: $(SFML_ARBITER)"

# ── SFML Visualizer (connects to shared memory) ───────────────────────────
sfml: $(SFML_TARGET)

VISUALIZER_ONLY_SOURCES = \
	sfml_ui/main.cpp \
	sfml_ui/Visualizer.cpp \
	sfml_ui/UIComponents.cpp \
	sfml_ui/LogPanel.cpp \
	sfml_ui/SpriteAnimation.cpp

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

# ── SFML Arbiter (manual multi-terminal) ──────────────────────────────────
run-sfml-arbiter: sfml_arbiter
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  SFML Arbiter (manual multi-terminal)                         ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "Launching SFML Arbiter..."
	@echo "NOTE: Run ./hip/hip and ./asp/asp in separate terminals."
	./$(SFML_ARBITER)

# ── SFML Arbiter (auto-launches all processes) ────────────────────────────
run-standalone: all
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  SFML Arbiter + Auto-launch                                   ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	./$(SFML_STANDALONE)

# ── SFML Arbiter with direct arguments (no prompts) ──────────────────────
run-sfml-direct: sfml_arbiter
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  SFML Arbiter (direct arguments)                              ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	./$(SFML_ARBITER) 42 3

# ── Passive Visualizer (connects to running arbiter) ──────────────────────
run-visualizer: sfml
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  Passive Visualizer                                           ║"
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
	sudo apt-get install -y libsfml-dev g++ make
	@echo "[OK] Dependencies installed"

# ── Docker targets ───────────────────────────────────────────────────────
docker-build:
	docker build -t chrono-rift .

docker-run:
	docker-compose up game

docker-clean:
	docker-compose down
	docker rmi chrono-rift

# ── Smoke test ───────────────────────────────────────────────────────────────
test: all
	@echo ""
	@echo "Running smoke tests..."
	@test -x hip/hip && echo "  hip: OK" || echo "  hip: MISSING"
	@test -x asp/asp && echo "  asp: OK" || echo "  asp: MISSING"
	@test -x sfml_ui/sfml_arbiter && echo "  sfml_arbiter: OK" || echo "  sfml_arbiter: MISSING"
	@test -x sfml_ui/chrono_rift_visualizer && echo "  visualizer: OK" || echo "  visualizer: MISSING"
	@test -x sfml_ui/chrono_rift_standalone && echo "  standalone: OK" || echo "  standalone: MISSING"
	@./scratch/check_seed 42 >/dev/null 2>&1 && echo "  check_seed: PASS" || echo "  check_seed: FAIL"
	@echo "[OK] All smoke tests complete"

# ── Binary check ─────────────────────────────────────────────────────────
check: all
	@echo ""
	@echo "Binary sizes:"
	@ls -lh hip/hip asp/asp sfml_ui/sfml_arbiter sfml_ui/chrono_rift_visualizer sfml_ui/chrono_rift_standalone 2>/dev/null || echo "  Some binaries missing"
	@echo "[OK] Check complete"

# ── Help ──────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════════╗"
	@echo "║  Chrono Rift — SFML Build System                                  ║"
	@echo "╠═══════════════════════════════════════════════════════════════════╣"
	@echo "║                                                                   ║"
	@echo "║  BUILD TARGETS:                                                   ║"
	@echo "║    make all              — Build everything                       ║"
	@echo "║    make sfml_arbiter     — Build SFML arbiter                     ║"
	@echo "║    make sfml             — Build passive visualizer               ║"
	@echo "║    make sfml_standalone  — Build standalone launcher              ║"
	@echo "║    make debug            — Build with debug symbols               ║"
	@echo "║    make clean            — Remove all build artifacts             ║"
	@echo "║    make install-deps     — Install system dependencies            ║"
	@echo "║                                                                   ║"
	@echo "║  DOCKER TARGETS:                                                  ║"
	@echo "║    make docker-build     — Build Docker image                     ║"
	@echo "║    make docker-run       — Run in Docker (with X11/GUI)           ║"
	@echo "║    make docker-clean     — Remove Docker artifacts                ║"
	@echo "║                                                                   ║"
	@echo "║  RUN TARGETS:                                                     ║"
	@echo "║    make run-sfml-arbiter — SFML arbiter (manual, prompts input)   ║"
	@echo "║    make run-standalone   — Auto-launch all (prompts first)        ║"
	@echo "║    make run-sfml-direct  — SFML arbiter (roll=42, party=3)        ║"
	@echo "║    make run-visualizer   — Passive visualizer only                ║"
	@echo "║                                                                   ║"
	@echo "║  SMOKE TEST TARGETS:                                               ║"
	@echo "║    make test            — Run smoke tests on all binaries          ║"
	@echo "║    make check           — Show binary sizes                       ║"
	@echo "║  TWO MODES:                                                       ║"
	@echo "║    Mode 1: SFML Arbiter— ./sfml_ui/sfml_arbiter (+ hip + asp)    ║"
	@echo "║    Mode 2: Visualizer  — ./sfml_ui/chrono_rift_visualizer         ║"
	@echo "║                                                                   ║"
	@echo "║  The standalone launcher fixes terminal input timing:             ║"
	@echo "║    ./sfml_ui/chrono_rift_standalone                               ║"
	@echo "║                                                                   ║"
	@echo "║  SFML Arbiter accepts command-line arguments:                     ║"
	@echo "║    ./sfml_ui/sfml_arbiter <roll_number> <party_size>             ║"
	@echo "║    Example: ./sfml_ui/sfml_arbiter 42 3                          ║"
	@echo "╚═══════════════════════════════════════════════════════════════════╝"
