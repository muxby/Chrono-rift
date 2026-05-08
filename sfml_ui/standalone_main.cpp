/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  CHRONO RIFT — Standalone Multi-Process Launcher                        ║
 * ║  Forks: SFML Arbiter + Console HIP + Headless ASP                       ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * Multi-mode launcher that prompts for input BEFORE forking any processes,
 * then launches arbiter, HIP, and ASP with pre-resolved arguments.
 *
 * Modes:
 *   ./chrono_rift_standalone                 — SFML Arbiter (Visualizer UI)
 *   ./chrono_rift_standalone --visualizer-only — Passive visualizer only
 *
 * OS Concepts:
 *   - Process Management: fork() + execl() for creating child processes
 *   - IPC: Shared memory created by arbiter, attached by HIP and ASP
 *   - Signal Handling: SIGINT/SIGTERM cleanup of child processes
 *   - Shared Memory IPC: All three processes communicate via /dev/shm/chrono_rift_shm
 *
 * Critical Design: Prompts for input BEFORE fork to avoid stdin conflicts.
 * Child processes inherit stdin but can't share it reliably, so the parent
 * reads all inputs first and passes values via command-line arguments.
 */

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>

static volatile sig_atomic_t g_shutdown = 0;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown = 1;
    }
}

static void kill_children(pid_t hip_pid, pid_t asp_pid) {
    if (hip_pid > 0) {
        kill(hip_pid, SIGTERM);
        waitpid(hip_pid, nullptr, 0);
    }
    if (asp_pid > 0) {
        kill(asp_pid, SIGTERM);
        waitpid(asp_pid, nullptr, 0);
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse mode
    bool visualizer_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--visualizer-only") == 0) visualizer_only = true;
    }

    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CHRONO RIFT — Multi-Process Launcher                         ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";

    if (visualizer_only) {
        std::cout << "║  Mode: Passive Visualizer (connect to existing arbiter)       ║\n";
    } else {
        std::cout << "║  Mode: SFML Arbiter (Visualizer UI) + HIP + ASP              ║\n";
    }

    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    // ═══════════════════════════════════════════════════════════════════════
    // CRITICAL FIX: Prompt for input BEFORE forking any child processes.
    // This ensures the parent process has exclusive access to stdin.
    // Values are then passed to children via command-line arguments.
    // ═══════════════════════════════════════════════════════════════════════

    int roll = 0;
    int party = 0;

    if (!visualizer_only) {
        std::cout << "----------------------------------------\n";
        std::cout << "Enter roll number seed: ";
        std::cin >> roll;

        std::cout << "Enter party size (1-4): ";
        std::cin >> party;

        if (party < 1 || party > 4) {
            std::cout << "Invalid party size. Using default: 3\n";
            party = 3;
        }

        std::cout << "Roll: " << roll << ", Party: " << party << "\n";
        std::cout << "----------------------------------------\n\n";
    }

    // Check if binaries exist
    const char* arbiter_path = "./sfml_ui/sfml_arbiter";
    const char* hip_path = "./hip/hip";
    const char* asp_path = "./asp/asp";

    if (access(arbiter_path, X_OK) != 0) {
        std::cerr << "ERROR: " << arbiter_path << " not found or not executable.\n";
        std::cerr << "Please build with: make all\n";
        return 1;
    }
    if (access(hip_path, X_OK) != 0) {
        std::cerr << "ERROR: " << hip_path << " not found or not executable.\n";
        return 1;
    }
    if (access(asp_path, X_OK) != 0) {
        std::cerr << "ERROR: " << asp_path << " not found or not executable.\n";
        return 1;
    }

    // ── Fork 1: Arbiter ───────────────────────────────────────────────────
    std::cout << "[1/3] Launching SFML Arbiter...\n";

    pid_t arbiter_pid = fork();
    if (arbiter_pid == 0) {
        execl(arbiter_path, "sfml_arbiter",
              std::to_string(roll).c_str(),
              std::to_string(party).c_str(),
              (char*)nullptr);
        perror("execl arbiter");
        _exit(1);
    }
    if (arbiter_pid < 0) {
        perror("fork arbiter");
        return 1;
    }

    // Wait for arbiter to initialize shared memory
    sleep(2);

    // ── Fork 2: HIP ───────────────────────────────────────────────────────
    std::cout << "[2/3] Launching Console HIP...\n";
    pid_t hip_pid = fork();
    if (hip_pid == 0) {
        execl(hip_path, "hip", (char*)nullptr);
        perror("execl hip");
        _exit(1);
    }
    if (hip_pid < 0) {
        perror("fork hip");
        kill(arbiter_pid, SIGKILL);
        waitpid(arbiter_pid, nullptr, 0);
        return 1;
    }

    // ── Fork 3: ASP ───────────────────────────────────────────────────────
    std::cout << "[3/3] Launching ASP...\n";
    pid_t asp_pid = fork();
    if (asp_pid == 0) {
        execl(asp_path, "asp", (char*)nullptr);
        perror("execl asp");
        _exit(1);
    }
    if (asp_pid < 0) {
        perror("fork asp");
        kill_children(hip_pid, 0);
        kill(arbiter_pid, SIGKILL);
        waitpid(arbiter_pid, nullptr, 0);
        return 1;
    }

    std::cout << "\n[OK] All processes launched!\n";
    std::cout << "  Arbiter PID: " << arbiter_pid << "\n";
    std::cout << "  HIP PID:     " << hip_pid << "\n";
    std::cout << "  ASP PID:     " << asp_pid << "\n";
    std::cout << "\nInstructions:\n";
    std::cout << "  - Watch the SFML window for game state\n";
    std::cout << "  - Enter player actions in this terminal when prompted (HIP)\n";
    std::cout << "  - Press Ctrl+C to force quit all processes\n\n";

    // Wait for arbiter to finish (game over or quit)
    int status;
    pid_t finished = waitpid(arbiter_pid, &status, 0);

    if (finished == arbiter_pid) {
        if (WIFEXITED(status)) {
            std::cout << "\n[OK] Arbiter exited normally with code " << WEXITSTATUS(status) << "\n";
        } else if (WIFSIGNALED(status)) {
            std::cout << "\n[!] Arbiter terminated by signal " << WTERMSIG(status) << "\n";
        }
    }

    // Clean up remaining children
    std::cout << "[ ] Terminating HIP and ASP...\n";
    kill_children(hip_pid, asp_pid);

    std::cout << "[OK] Standalone session complete.\n";
    return 0;
}
