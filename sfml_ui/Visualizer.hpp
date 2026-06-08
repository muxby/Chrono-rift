#pragma once

#include "UIComponents.hpp"
#include "UITheme.hpp"
#include "ThreadPool.hpp"
#include "SpriteAnimation.hpp"
#include "../shared.hpp"
#include "../common.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace ChronoRift {

enum class ViewMode {
    COMBAT,
    SCHEDULER,
    HYBRID
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    bool initialize();
    void run(SharedState* state);
    void shutdown();

private:
    // ── Window ──────────────────────────────────────────────────────────────
    sf::RenderWindow window;
    sf::Font mainFont;
    sf::Font monoFont;
    bool running;
    ViewMode currentMode;

    // ── Intro Animation ───────────────────────────────────────────────────────
    std::vector<sf::Texture> introFrames;
    sf::Sprite                introSprite;
    bool                      isIntroActive;
    int                       introFrameIndex;
    float                     introFrameTimer;

    // ── State Reference ─────────────────────────────────────────────────────
    SharedState* sharedState;

    // ── UI Components ───────────────────────────────────────────────────────
    CharacterCard* playerCards[MAX_PLAYERS];
    int playerCardCount;
    CharacterCard* npcCards[MAX_NPCS];
    int npcCardCount;
    ArtifactDisplay* artifactDisplays[3];
    ArbiterNode* arbiterNode;
    ProcessBlock* processBlocks[MAX_PLAYERS + MAX_NPCS];
    int processBlockCount;
    HIPSlot* hipSlots[4];
    int hipSlotCount;
    Button* buttons[4];
    int buttonCount;
    Slider* sliders[4];
    int sliderCount;
    Button* schedulingBtns[4];  // STAMINA/RR/FIFO/PRIORITY
    Button* deadlockBtns[3];     // DETECT/NO_HOLD_WAIT/PREEMPT
    ParticleSystem particles;
    ConnectionLine connectionLines[4];

    // ── Fighter Sprites (UFC Cage View) ─────────────────────────────────────
    FighterSprite playerFighters[MAX_PLAYERS];  // 4 player fighter sprites
    FighterSprite enemyFighters[4];             // 4 enemy fighter sprites
    int  playerInCage;      // index of player currently shown in cage
    int  enemyInCage;       // index of enemy currently shown in cage
    int  lastHPPlayers[MAX_PLAYERS]; // previous HP values for attack detection
    int  lastHPNpcs[MAX_NPCS];      // previous HP values for attack detection
    sf::Texture bannerTextures[8];  // banner portrait textures (optional)
    bool bannersLoaded;

    // ── Background Video (image sequence) ───────────────────────────────────
    std::vector<sf::Texture> bgFrames;   // all pre-extracted JPG frames
    sf::Texture               bgTexture; // current active texture (pointer to bgFrames[i])
    sf::Sprite                bgSprite;  // drawn fullscreen each frame
    int                       bgFrameIndex;  // which frame is currently shown
    float                     bgFrameTimer;  // seconds since last frame advance
    static constexpr float    BG_FPS = 30.f; // match the extraction frame-rate

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

    // ── Keyboard / Turn-Order Control ──────────────────────────────────────
    // 0 = player's turn to input an attack, 1 = waiting for enemy to act
    int  localTurnOrder;
    // helpers for finding next alive character when switching
    int  nextAlivePlayer(int from, int dir) const;
    int  nextAliveEnemy(int from, int dir) const;
    // dispatch a player attack action to the arbiter
    void submitPlayerAttack(ActionType act);
    // dispatch an enemy attack action to the arbiter (U/O keys)
    void submitEnemyAttack(ActionType act);

    // ── Layout Constants ────────────────────────────────────────────────────
    static constexpr float WINDOW_WIDTH      = 1600.f;
    static constexpr float WINDOW_HEIGHT     = 960.f;
    static constexpr float HEADER_HEIGHT     = 70.f;
    static constexpr float FOOTER_HEIGHT     = 0.f;
    static constexpr float LEFT_PANEL_WIDTH  = 280.f;
    static constexpr float RIGHT_PANEL_WIDTH = 280.f;
    static constexpr float ROSTER_HEIGHT     = 260.f; // bottom roster panel height
    static constexpr float CAGE_BOTTOM       = WINDOW_HEIGHT - ROSTER_HEIGHT;

    // ---
    // RENDERING METHODS
    // ---

    void render();
    void renderBackground();
    void renderHeader();
    void renderFooter();
    void renderCombatView();
    void renderSchedulerView();
    void renderHybridView();
    void renderGameOver();
    void renderConnections();

    // ---
    // UPDATE METHODS
    // ---

    void update(float dt);
    void updateComponents(float dt);
    void updateProcessBlocks();
    void updateConnections();

    // ---
    // EVENT HANDLING
    // ---

    void handleEvents();
    void handleMouseClick(float x, float y);
    void handleKeyPress(sf::Keyboard::Key key);

    // ---
    // INITIALIZATION
    // ---

    void initComponents();
    void initPlayerCards();
    void initNpcCards();
    void initArtifacts();
    void initControls();
    void initSchedulerView();
    bool loadFonts();
    void loadBackgroundFrames();
    void loadIntroFrames();
    void loadFighterSprites();
    void loadBannerImages();

    // ---
    // UFC CAGE RENDERING
    // ---

    void renderCageFighters();
    void renderTeamRoster();
    void renderOpponentRoster();
    void renderFighterHP(float x, float y, float width,
                         int hp, int maxHp, const std::string& name,
                         sf::Color barColor, bool alignRight = false);

    // ---
    // HELPERS
    // ---

    void spawnNewProcess();
    void removeProcess(int pid);
    void switchMode(ViewMode mode);
    sf::Color getStateColor(int state) const;
    std::string actionToString(ActionType action) const;

    // ── Static callbacks for buttons ────────────────────────────────────────
    static void onSpawnProcess(void* ctx);
    static void onSwitchCombat(void* ctx);
    static void onSchedMode(void* ctx, int mode);
    static void onDeadlockStrategy(void* ctx, int strategy);
};

} // namespace ChronoRift
