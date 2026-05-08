#pragma once

#include "UIComponents.hpp"
#include "UITheme.hpp"
#include "../shared.hpp"
#include "../common.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <map>

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
// MAIN VISUALIZER — Chrono Rift SFML UI
// ═══════════════════════════════════════════════════════════════════════════

enum class ViewMode {
    COMBAT,      // Original character combat view
    SCHEDULER,   // OS process scheduling view
    HYBRID       // Combined view
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    // Initialize the SFML window and resources
    bool initialize();

    // Main render loop
    void run(SharedState* state);

    // Shutdown
    void shutdown();

private:
    // ── Window ──────────────────────────────────────────────────────────────
    sf::RenderWindow window;
    sf::Font mainFont;
    sf::Font monoFont;
    bool running;
    ViewMode currentMode;

    // ── State Reference ─────────────────────────────────────────────────────
    SharedState* sharedState;

    // ── UI Components ───────────────────────────────────────────────────────
    std::vector<std::unique_ptr<CharacterCard>> playerCards;
    std::vector<std::unique_ptr<CharacterCard>> npcCards;
    std::vector<std::unique_ptr<ArtifactDisplay>> artifactDisplays;
    std::unique_ptr<LogPanel> logPanel;
    std::unique_ptr<MetricsDashboard> metricsDashboard;
    std::unique_ptr<GanttChart> ganttChart;
    std::unique_ptr<ArbiterNode> arbiterNode;
    std::vector<std::unique_ptr<ProcessBlock>> processBlocks;
    std::vector<std::unique_ptr<HIPSlot>> hipSlots;
    std::vector<std::unique_ptr<Button>> buttons;
    std::vector<std::unique_ptr<Slider>> sliders;
    ParticleSystem particles;
    std::vector<ConnectionLine> connectionLines;

    // ── Timing & Metrics ────────────────────────────────────────────────────
    sf::Clock deltaClock;
    sf::Clock gameClock;
    sf::Clock metricsClock;
    float simulationTime;
    float avgWaitingTime;
    float avgTurnaroundTime;
    float cpuThroughput;
    int totalProcessesSpawned;
    int lastTurnSeq;
    int lastKills;

    // ── Scheduling Parameters ───────────────────────────────────────────────
    float quantumTime;
    int schedulingPriority;

    // ── Layout Constants ────────────────────────────────────────────────────
    static constexpr float WINDOW_WIDTH = 1400.f;
    static constexpr float WINDOW_HEIGHT = 900.f;
    static constexpr float HEADER_HEIGHT = 60.f;
    static constexpr float FOOTER_HEIGHT = 200.f;
    static constexpr float LEFT_PANEL_WIDTH = 320.f;
    static constexpr float RIGHT_PANEL_WIDTH = 320.f;

    // ═══════════════════════════════════════════════════════════════════════
    // RENDERING METHODS
    // ═══════════════════════════════════════════════════════════════════════

    void render();
    void renderBackground();
    void renderHeader();
    void renderFooter();
    void renderCombatView();
    void renderSchedulerView();
    void renderHybridView();
    void renderGameOver();
    void renderConnections();

    // ═══════════════════════════════════════════════════════════════════════
    // UPDATE METHODS
    // ═══════════════════════════════════════════════════════════════════════

    void update(float dt);
    void updateComponents(float dt);
    void updateMetrics();
    void updateGanttChart();
    void updateProcessBlocks();
    void updateConnections();

    // ═══════════════════════════════════════════════════════════════════════
    // EVENT HANDLING
    // ═══════════════════════════════════════════════════════════════════════

    void handleEvents();
    void handleMouseClick(float x, float y);
    void handleKeyPress(sf::Keyboard::Key key);

    // ═══════════════════════════════════════════════════════════════════════
    // INITIALIZATION
    // ═══════════════════════════════════════════════════════════════════════

    void initComponents();
    void initPlayerCards();
    void initNpcCards();
    void initArtifacts();
    void initControls();
    void initSchedulerView();
    bool loadFonts();

    // ═══════════════════════════════════════════════════════════════════════
    // HELPERS
    // ═══════════════════════════════════════════════════════════════════════

    void spawnNewProcess();
    void removeProcess(int pid);
    void switchMode(ViewMode mode);
    sf::Color getStateColor(int state) const;
    std::string actionToString(ActionType action) const;
};

} // namespace ChronoRift
