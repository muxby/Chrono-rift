// standalone_main.cpp - Launcher that forks arbiter, hip, and asp as separate processes.
// Reads input BEFORE forking so child processes don't fight over stdin.

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>

using namespace std;

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

// Helper: validate roll number is a positive integer (no negatives, no letters)
static bool is_valid_positive_integer(const string& input, int& out_value) {
    if (input.empty()) return false;
    // Check all characters are digits
    for (char c : input) {
        if (c < '0' || c > '9') return false;
    }
    // Check no overflow and positive
    try {
        long long val = stoll(input);
        if (val <= 0 || val > 2147483647LL) return false;
        out_value = static_cast<int>(val);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse mode
    bool visualizer_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--visualizer-only") == 0) visualizer_only = true;
    }

    cout << "=== CHRONO RIFT - Multi-Process Launcher ===\n";
    if (visualizer_only) {
        cout << "Mode: Passive Visualizer (connect to existing arbiter)\n";
    } else {
        cout << "Mode: SFML Arbiter + HIP + ASP\n";
    }
    cout << "\n";

    // get input before forking (children share stdin otherwise)

    string roll_input;
    int roll = 0;
    int party = 3;

    if (!visualizer_only) {
        cout << "----------------------------------------\n";

        // --- ROLL NUMBER INPUT WITH STRICT VALIDATION ---
        // Per project spec: Roll number must be a positive integer
        while (true) {
            cout << "Enter roll number (positive integer only): ";
            cin >> ws; // skip leading whitespace
            getline(cin, roll_input);
            if (is_valid_positive_integer(roll_input, roll)) {
                break;
            }
            cout << "[INVALID] Roll number must be a positive integer (1, 2, 123, etc.).\n";
            cout << "          No letters, no negative numbers, no zero, no decimals.\n\n";
        }

        // --- PARTY SIZE INPUT WITH VALIDATION ---
        while (true) {
            cout << "Enter party size (1-4) [default: 3]: ";
            string p_input;
            if (getline(cin, p_input)) {
                if (p_input.empty()) {
                    party = 3;
                    break;
                }
                try {
                    party = stoi(p_input);
                    if (party >= 1 && party <= 4) break;
                    cout << "[INVALID] Party size must be between 1 and 4.\n";
                } catch (...) {
                    cout << "[INVALID] Please enter a number between 1 and 4.\n";
                }
            } else {
                party = 3;
                break;
            }
        }

        cout << "Final Seed: " << roll << ", Party: " << party << "\n";
        cout << "----------------------------------------\n\n";
    }

    // Check if binaries exist
    const char* arbiter_path = "./sfml_ui/sfml_arbiter";
    const char* hip_path = "./hip/hip";
    const char* asp_path = "./asp/asp";

    if (access(arbiter_path, X_OK) != 0) {
        cerr << "ERROR: " << arbiter_path << " not found or not executable.\n";
        cerr << "Please build with: make all\n";
        return 1;
    }
    if (access(hip_path, X_OK) != 0) {
        cerr << "ERROR: " << hip_path << " not found or not executable.\n";
        return 1;
    }
    if (access(asp_path, X_OK) != 0) {
        cerr << "ERROR: " << asp_path << " not found or not executable.\n";
        return 1;
    }

    // fork arbiter
    cout << "[1/3] Launching SFML Arbiter...\n";

    pid_t arbiter_pid = fork();
    if (arbiter_pid == 0) {
        execl(arbiter_path, "sfml_arbiter",
              to_string(roll).c_str(),
              to_string(party).c_str(),
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

    // fork hip
    cout << "[2/3] Launching Console HIP...\n";
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

    // fork asp
    cout << "[3/3] Launching ASP...\n";
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

    cout << "\n[OK] All processes launched!\n";
    cout << "  Arbiter PID: " << arbiter_pid << "\n";
    cout << "  HIP PID:     " << hip_pid << "\n";
    cout << "  ASP PID:     " << asp_pid << "\n";
    cout << "\nInstructions:\n";
    cout << "  - Watch the SFML window for game state\n";
    cout << "  - Enter player actions in this terminal when prompted (HIP)\n";
    cout << "  - Press Ctrl+C to force quit all processes\n\n";

    // Wait for arbiter to finish (game over or quit)
    int status;
    pid_t finished = waitpid(arbiter_pid, &status, 0);

    if (finished == arbiter_pid) {
        if (WIFEXITED(status)) {
            cout << "\n[OK] Arbiter exited normally with code " << WEXITSTATUS(status) << "\n";
        } else if (WIFSIGNALED(status)) {
            cout << "\n[!] Arbiter terminated by signal " << WTERMSIG(status) << "\n";
        }
    }

    // Clean up remaining children
    cout << "[ ] Terminating HIP and ASP...\n";
    kill_children(hip_pid, asp_pid);

    cout << "[OK] Standalone session complete.\n";
    return 0;
}
