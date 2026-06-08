#include "Visualizer.hpp"
#include "os_helpers.hpp"
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>

using namespace std;

#include <unistd.h>
#include <limits.h>

static std::string resolvePath(const std::string& path) {
    std::cerr << "[PATH] Resolving: " << path << std::endl;

    // 1. Try relative to the current working directory first
    {
        std::ifstream f(path.c_str());
        if (f.good()) {
            std::cerr << "  -> Found in CWD: " << path << std::endl;
            return path;
        }
    }

    // 2. Try relative to the directory of the running executable (handles VM, Docker, custom script locations)
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::string exeDir = exePath;
        size_t lastSlash = exeDir.find_last_of('/');
        if (lastSlash != std::string::npos) {
            exeDir = exeDir.substr(0, lastSlash);
        }
        std::cerr << "  -> Executable Dir: " << exeDir << std::endl;

        // Try directly inside executable directory
        std::string try1 = exeDir + "/" + path;
        {
            std::ifstream f(try1.c_str());
            if (f.good()) {
                std::cerr << "  -> Found in Exe Dir: " << try1 << std::endl;
                return try1;
            }
        }

        // Try in the parent directory of the executable (common when running from inside sfml_ui/)
        std::string try2 = exeDir + "/../" + path;
        {
            std::ifstream f(try2.c_str());
            if (f.good()) {
                std::cerr << "  -> Found in Parent of Exe: " << try2 << std::endl;
                return try2;
            }
        }
    } else {
        std::cerr << "  -> readlink() failed to get executable path!" << std::endl;
    }

    // 3. Fallback to default relative path
    std::cerr << "  -> Fallback to default path: " << path << std::endl;
    return path;
}

namespace ChronoRift {

// constructor / destructor

// ── Fighter name tables ──────────────────────────────────────────────────
static const char* PLAYER_NAMES[4] = {"KHABIB", "KHAMZAT", "ILLIA", "JUSTIN"};
static const char* ENEMY_NAMES[4]  = {"McGREGOR", "ALEX P.", "JON JONES", "PADDY"};

Visualizer::Visualizer()
    : window(sf::VideoMode(static_cast<unsigned>(WINDOW_WIDTH),
                           static_cast<unsigned>(WINDOW_HEIGHT)),
             "Chrono Rift - OS Process Visualizer", sf::Style::Close),
      running(false), sharedState(nullptr), currentMode(ViewMode::COMBAT),
      isIntroActive(true), introFrameIndex(0), introFrameTimer(0.f),
      playerCardCount(0), npcCardCount(0), arbiterNode(nullptr),
      processBlockCount(0), hipSlotCount(0), buttonCount(0), sliderCount(0),
      schedulingBtns{nullptr,nullptr,nullptr,nullptr},
      deadlockBtns{nullptr,nullptr,nullptr},
      playerInCage(0), enemyInCage(0), bannersLoaded(false),
      bgFrameIndex(0), bgFrameTimer(0.f),
      simulationTime(0.f), avgWaitingTime(0.f), avgTurnaroundTime(0.f),
      cpuThroughput(0.f), totalProcessesSpawned(0), lastTurnSeq(0), lastKills(0),
      quantumTime(2.0f), schedulingPriority(5),
      localTurnOrder(0)
{
    memset(lastHPPlayers, 0, sizeof(lastHPPlayers));
    memset(lastHPNpcs, 0, sizeof(lastHPNpcs));
}

Visualizer::~Visualizer() {
    shutdown();

    for (int i = 0; i < playerCardCount; ++i) delete playerCards[i];
    for (int i = 0; i < npcCardCount; ++i) delete npcCards[i];
    for (int i = 0; i < 3; ++i) delete artifactDisplays[i];
    delete arbiterNode;
    for (int i = 0; i < processBlockCount; ++i) delete processBlocks[i];
    for (int i = 0; i < hipSlotCount; ++i) delete hipSlots[i];
    for (int i = 0; i < buttonCount; ++i) delete buttons[i];
    for (int i = 0; i < 4; ++i) delete schedulingBtns[i];
    for (int i = 0; i < 3; ++i) delete deadlockBtns[i];
    for (int i = 0; i < sliderCount; ++i) delete sliders[i];
}

// init

bool Visualizer::initialize() {
    window.setFramerateLimit(30);
    window.setVerticalSyncEnabled(true); // enabled vsync

    if (!loadFonts()) {
        cerr << "Failed to load fonts!" << endl;
        return false;
    }

    loadBackgroundFrames();
    loadIntroFrames();
    loadFighterSprites();
    loadBannerImages();
    initComponents();
    running = true;
    return true;
}

bool Visualizer::loadFonts() {
    const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/droid/DroidSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/mnt/c/Windows/Fonts/arial.ttf",
        "/mnt/c/Windows/Fonts/segoeui.ttf",
    };

    bool loaded = false;
    for (const auto& path : fontPaths) {
        if (mainFont.loadFromFile(path)) {
            loaded = true;
            break;
        }
    }

    if (!loaded) {
        cerr << "WARNING: Could not load font. Text rendering will fail." << endl;
        return false;
    }

    const char* monoPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/mnt/c/Windows/Fonts/consola.ttf",
    };

    for (const auto& path : monoPaths) {
        if (monoFont.loadFromFile(path)) {
            break;
        }
    }
    if (monoFont.getInfo().family.empty()) {
        monoFont = mainFont;
    }

    return true;
}

// ── Background image-sequence loader ────────────────────────────────────────
void Visualizer::loadBackgroundFrames() {
    // Frames are at  background/frame_XXXX.jpg  relative to the working dir.
    // We load up to 9999 frames, stopping at the first missing file.
    char path[256];
    for (int i = 0; i < 9999; ++i) {
        snprintf(path, sizeof(path), "background/frame_%04d.jpg", i);
        sf::Texture tex;
        tex.setSmooth(true);
        if (!tex.loadFromFile(resolvePath(path))) break;  // stop at first missing frame
        bgFrames.push_back(std::move(tex));
    }

    if (bgFrames.empty()) {
        cerr << "WARNING: No background frames found in background/ folder." << endl;
        return;
    }

    cerr << "[BG] Loaded " << bgFrames.size() << " background frames." << endl;

    // Initialise the sprite to the first frame and scale to fill the window
    bgSprite.setTexture(bgFrames[0]);
    sf::Vector2u texSz = bgFrames[0].getSize();
    if (texSz.x > 0 && texSz.y > 0) {
        bgSprite.setScale(
            WINDOW_WIDTH  / static_cast<float>(texSz.x),
            WINDOW_HEIGHT / static_cast<float>(texSz.y));
    }
}

// ── Intro image-sequence loader ────────────────────────────────────────────
void Visualizer::loadIntroFrames() {
    char path[256];
    for (int i = 0; i < 9999; ++i) {
        snprintf(path, sizeof(path), "intro_frames/frame_%04d.jpg", i);
        sf::Texture tex;
        tex.setSmooth(true);
        if (!tex.loadFromFile(resolvePath(path))) break;
        introFrames.push_back(std::move(tex));
    }

    if (!introFrames.empty()) {
        cerr << "[INTRO] Loaded " << introFrames.size() << " intro frames." << endl;
        introSprite.setTexture(introFrames[0]);
        sf::Vector2u texSz = introFrames[0].getSize();
        if (texSz.x > 0 && texSz.y > 0) {
            introSprite.setScale(
                WINDOW_WIDTH  / static_cast<float>(texSz.x),
                WINDOW_HEIGHT / static_cast<float>(texSz.y));
        }
    } else {
        isIntroActive = false; // Disable if no frames
        cerr << "WARNING: No intro frames found." << endl;
    }
}

// ── Load all fighter spritesheets ────────────────────────────────────────
void Visualizer::loadFighterSprites() {
    // Players: standing + attack
    playerFighters[0].loadStanding(resolvePath("SPRITESHEET/Players/player_standing_sprites/khabib_standing.png"));
    playerFighters[0].loadAttack(resolvePath("SPRITESHEET/Players/player_attack/khabib_attack.png"));

    playerFighters[1].loadStanding(resolvePath("SPRITESHEET/Players/player_standing_sprites/khamzat_standing.png"));
    playerFighters[1].loadAttack(resolvePath("SPRITESHEET/Players/player_attack/khamzat_attack_sprite.png"));

    playerFighters[2].loadStanding(resolvePath("SPRITESHEET/Players/player_standing_sprites/illia toporia_standing.png"));
    playerFighters[2].loadAttack(resolvePath("SPRITESHEET/Players/player_attack/illia_attack.png"));

    playerFighters[3].loadStanding(resolvePath("SPRITESHEET/Players/player_standing_sprites/justin gagje_standing.png"));
    playerFighters[3].loadAttack(resolvePath("SPRITESHEET/Players/player_attack/justin_attack.png"));

    // Enemies: standing + attack
    enemyFighters[0].loadStanding(resolvePath("SPRITESHEET/enemy/enemy standing/mcgregor_static.png"));
    enemyFighters[0].loadAttack(resolvePath("SPRITESHEET/enemy/enemy_attack/mcgregor_attack.png"));

    enemyFighters[1].loadStanding(resolvePath("SPRITESHEET/enemy/enemy standing/alex_preira_static.png"));
    enemyFighters[1].loadAttack(resolvePath("SPRITESHEET/enemy/enemy_attack/alex_attack.png"));

    enemyFighters[2].loadStanding(resolvePath("SPRITESHEET/enemy/enemy standing/john_jones_static.png"));
    enemyFighters[2].loadAttack(resolvePath("SPRITESHEET/enemy/enemy_attack/john_jones_attack.png"));

    enemyFighters[3].loadStanding(resolvePath("SPRITESHEET/enemy/enemy standing/paddy pimblett_static.png"));
    enemyFighters[3].loadAttack(resolvePath("SPRITESHEET/enemy/enemy_attack/paddy_attack.png"));

    // Set animation FPS for all fighters — 10 FPS for normal animations
    for (int i = 0; i < MAX_PLAYERS; ++i) playerFighters[i].setFPS(10.f);
    for (int i = 0; i < 4; ++i) enemyFighters[i].setFPS(10.f);

    cerr << "[FIGHTERS] All fighter spritesheets loaded." << endl;
}

// ── Load optional banner portrait images ─────────────────────────────────
void Visualizer::loadBannerImages() {
    const char* bannerPaths[8] = {
        "SPRITESHEET/banners/khabib_banner.png",
        "SPRITESHEET/banners/khamzat_banner.png",
        "SPRITESHEET/banners/illia_banner.png",
        "SPRITESHEET/banners/justin_banner.png",
        "SPRITESHEET/banners/mcgregor_banner.png",
        "SPRITESHEET/banners/alex_banner.png",
        "SPRITESHEET/banners/jones_banner.png",
        "SPRITESHEET/banners/paddy_banner.png"
    };
    bannersLoaded = true;
    for (int i = 0; i < 8; ++i) {
        if (!bannerTextures[i].loadFromFile(resolvePath(bannerPaths[i]))) {
            bannersLoaded = false;  // will fall back to spritesheet portraits
        }
    }
    cerr << "[BANNERS] Banner images " << (bannersLoaded ? "loaded" : "not found, using sprite portraits") << endl;
}

void Visualizer::initComponents() {
    initPlayerCards();
    initNpcCards();
    initArtifacts();
    initControls();
    initSchedulerView();

    // Arbiter node: centered in the header bar
    arbiterNode = new ArbiterNode(
        WINDOW_WIDTH / 2.f, HEADER_HEIGHT / 2.f + 5.f, 26.f, mainFont);
}

void Visualizer::initPlayerCards() {
    // Left panel: 4 player cards stacked vertically
    float cardW = LEFT_PANEL_WIDTH - 20.f;  // 260px
    float cardH = 72.f;
    float startX = 10.f;
    float startY = HEADER_HEIGHT + 26.f;    // below section label
    float gap    = 6.f;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        playerCards[playerCardCount++] = new CharacterCard(
            startX, startY + i * (cardH + gap), cardW, cardH, mainFont, true);
    }
}

void Visualizer::initNpcCards() {
    // Center area: NPC cards in dynamic columns based on count
    // Use 2 columns minimum, add more if needed to prevent overlap
    float centerW = WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH; // 1040
    float cardH   = 62.f;
    float startX  = LEFT_PANEL_WIDTH + 10.f;
    float startY  = HEADER_HEIGHT + 26.f;      // below section label
    float gap     = 5.f;

    // Calculate columns needed: max 5 per column
    int maxPerCol = 5;
    int numCols = (MAX_NPCS + maxPerCol - 1) / maxPerCol;  // ceil division
    numCols = cr_max(2, numCols);  // At least 2 columns
    float colW = (centerW - (numCols + 1) * 10.f) / numCols;

    for (int i = 0; i < MAX_NPCS; ++i) {
        int col = i / maxPerCol;
        int row = i % maxPerCol;
        float cx = startX + col * (colW + 10.f);
        float cy = startY + row * (cardH + gap);
        npcCards[npcCardCount++] = new CharacterCard(
            cx, cy, colW, cardH, mainFont, false);
    }
}

void Visualizer::initArtifacts() {
    // Artifacts: row just above the footer in the center area
    float centerW = WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
    float artW    = (centerW - 40.f) / 3.f;
    float startX  = LEFT_PANEL_WIDTH + 10.f;
    float startY  = WINDOW_HEIGHT - FOOTER_HEIGHT - 58.f;

    for (int i = 0; i < 3; ++i) {
        artifactDisplays[i] = new ArtifactDisplay(
            startX + i * (artW + 10.f), startY, artW, mainFont);
    }
}

void Visualizer::onSpawnProcess(void* ctx) {
    // Particles disabled - plain UI
    static_cast<Visualizer*>(ctx)->spawnNewProcess();
}

void Visualizer::onSwitchCombat(void* ctx) {
    static_cast<Visualizer*>(ctx)->switchMode(ViewMode::COMBAT);
}

// Tag-based callback for scheduling mode buttons (0=STAMINA, 1=RR, 2=FIFO, 3=PRIO)
void Visualizer::onSchedMode(void* ctx, int mode) {
    auto* vis = static_cast<Visualizer*>(ctx);
    if (vis->sharedState) vis->sharedState->scheduler_mode = static_cast<SchedulerMode>(mode);
}

// Tag-based callback for deadlock strategy buttons (0=DETECT, 1=NO_HOLD, 2=PREEMPT)
void Visualizer::onDeadlockStrategy(void* ctx, int strategy) {
    auto* vis = static_cast<Visualizer*>(ctx);
    if (vis->sharedState) vis->sharedState->deadlock_strategy = static_cast<DeadlockStrategy>(strategy);
}

void Visualizer::initControls() {
    // Right panel controls: below MetricsDashboard (starts at y=HEADER+8, height=140)
    float btnX = WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 10.f;
    float btnY = HEADER_HEIGHT + 160.f;  // 70+8+140+12 margin

    Button* spawnBtn = new Button(
        btnX, btnY, 120.f, 30.f, "+ PROCESS", mainFont, FONT_SMALL);
    spawnBtn->setColors(sf::Color(220, 220, 220), sf::Color(200, 200, 200), TEXT_PRIMARY);
    spawnBtn->setCallback(&Visualizer::onSpawnProcess, this);
    buttons[buttonCount++] = spawnBtn;

    Button* combatBtn = new Button(
        btnX + 128.f, btnY, 80.f, 30.f, "COMBAT", mainFont, FONT_SMALL);
    combatBtn->setColors(sf::Color(220, 220, 220), sf::Color(200, 200, 200), TEXT_PRIMARY);
    combatBtn->setCallback(&Visualizer::onSwitchCombat, this);
    buttons[buttonCount++] = combatBtn;

    Slider* qSlider = new Slider(
        btnX, btnY + 42.f, RIGHT_PANEL_WIDTH - 20.f, 0.5f, 10.f, quantumTime,
        "Quantum (s)", mainFont);
    sliders[sliderCount++] = qSlider;

    Slider* pSlider = new Slider(
        btnX, btnY + 92.f, RIGHT_PANEL_WIDTH - 20.f, 1.f, 10.f,
        static_cast<float>(schedulingPriority), "Priority", mainFont);
    sliders[sliderCount++] = pSlider;

    // -- Scheduling Mode Buttons (row of 4) --
    const char* schedLabels[4] = {"STAM", "RR", "FIFO", "PRIO"};
    float sbW = (RIGHT_PANEL_WIDTH - 20.f) / 4.f - 2.f;
    for (int i = 0; i < 4; ++i) {
        Button* sb = new Button(
            btnX + i * (sbW + 2.f), btnY + 136.f, sbW, 22.f,
            schedLabels[i], mainFont, FONT_TINY);
        sb->setColors(sf::Color(220, 220, 220), sf::Color(200, 200, 200), TEXT_PRIMARY);
        sb->setCallbackWithTag(&Visualizer::onSchedMode, this, i);
        schedulingBtns[i] = sb;
    }

    // -- Deadlock Strategy Buttons (row of 3) --
    const char* dlLabels[3] = {"DETECT", "NO_HLD", "PRMT"};
    float dbW = (RIGHT_PANEL_WIDTH - 20.f) / 3.f - 2.f;
    for (int i = 0; i < 3; ++i) {
        Button* db = new Button(
            btnX + i * (dbW + 2.f), btnY + 162.f, dbW, 22.f,
            dlLabels[i], mainFont, FONT_TINY);
        db->setColors(sf::Color(220, 220, 220), sf::Color(200, 200, 200), TEXT_PRIMARY);
        db->setCallbackWithTag(&Visualizer::onDeadlockStrategy, this, i);
        deadlockBtns[i] = db;
    }
}

void Visualizer::initSchedulerView() {
    // Deferred: actual counts set in run() once sharedState is known.
    // Create placeholder blocks (max possible = MAX_PLAYERS + MAX_NPCS = 13)
    float blockW = LEFT_PANEL_WIDTH - 20.f;
    float blockH = 46.f;
    float startX = 10.f;
    float startY = HEADER_HEIGHT + 26.f;

    int total = MAX_PLAYERS + MAX_NPCS;
    for (int i = 0; i < total; ++i) {
        processBlocks[processBlockCount++] = new ProcessBlock(
            startX, startY + i * (blockH + 6.f), blockW, blockH, i + 100, mainFont);
    }

    // Right panel: 4 CPU/HIP slots
    float slotW = RIGHT_PANEL_WIDTH - 20.f;
    float slotH = 50.f;
    float slotX = WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 10.f;
    float slotY = HEADER_HEIGHT + 360.f;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        hipSlots[hipSlotCount++] = new HIPSlot(
            slotX, slotY + i * (slotH + 6.f), slotW, slotH, i, "Core", mainFont);
    }
}

// main loop

void Visualizer::run(SharedState* state) {
    sharedState = state;
    gameClock.restart();
    deltaClock.restart();  // reset so init time doesn't bleed into first frame dt

    while (running && window.isOpen()) {
        float dt = deltaClock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;  // clamp to prevent frame-skipping on first frame
        simulationTime = gameClock.getElapsedTime().asSeconds();

        handleEvents();
        update(dt);
        render();

        if (sharedState && !sharedState->running) {
            render();  // one last frame showing final state
            renderGameOver();
            running = false;
        }
    }
}

void Visualizer::shutdown() {
    running = false;
    if (window.isOpen()) {
        window.close();
    }
}

// event handling

void Visualizer::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            running = false;
            window.close();
        }

        if (event.type == sf::Event::KeyPressed) {
            handleKeyPress(event.key.code);
        }

        for (int i = 0; i < buttonCount; ++i) {
            buttons[i]->handleEvent(event, window);
        }
        for (int i = 0; i < sliderCount; ++i) {
            sliders[i]->handleEvent(event, window);
        }
        for (int i = 0; i < 4; ++i) {
            if (schedulingBtns[i]) schedulingBtns[i]->handleEvent(event, window);
        }
        for (int i = 0; i < 3; ++i) {
            if (deadlockBtns[i]) deadlockBtns[i]->handleEvent(event, window);
        }

        // Mouse click - no particle effects (plain white UI)
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            // No-op: no particle effects in plain UI
        }
    }
}

void Visualizer::handleKeyPress(sf::Keyboard::Key key) {
    if (isIntroActive) {
        isIntroActive = false;
        return;
    }
    switch (key) {
        // ── View Mode Switching ──────────────────────────────────────────
        case sf::Keyboard::Num1:
            switchMode(ViewMode::COMBAT);
            break;
        case sf::Keyboard::Num2:
            switchMode(ViewMode::SCHEDULER);
            break;
        case sf::Keyboard::Num3:
            switchMode(ViewMode::HYBRID);
            break;
        case sf::Keyboard::Space:
            spawnNewProcess();
            break;
        case sf::Keyboard::Escape:
            running = false;
            break;

        // ── Player Character Switching (W/S/A/D) ────────────────────────
        // W = previous player, S = next player, A = player -1, D = player +1
        case sf::Keyboard::W:
        case sf::Keyboard::A:
            playerInCage = nextAlivePlayer(playerInCage, -1);
            if (sharedState) {
                pthread_mutex_lock(&sharedState->mtx);
                if (sharedState->active_team == TEAM_PLAYER)
                    sharedState->active_id = playerInCage;
                // Always update target so enemy attacks hit the selected player
                sharedState->selected_player_id = playerInCage;
                pthread_mutex_unlock(&sharedState->mtx);
            }
            break;
        case sf::Keyboard::S:
        case sf::Keyboard::D:
            playerInCage = nextAlivePlayer(playerInCage, +1);
            if (sharedState) {
                pthread_mutex_lock(&sharedState->mtx);
                if (sharedState->active_team == TEAM_PLAYER)
                    sharedState->active_id = playerInCage;
                // Always update target so enemy attacks hit the selected player
                sharedState->selected_player_id = playerInCage;
                pthread_mutex_unlock(&sharedState->mtx);
            }
            break;

        // ── Enemy Character Switching (J/K/L/I) ─────────────────────────
        // J = prev enemy, I = prev enemy (up), L = next enemy, K = next enemy (down)
        case sf::Keyboard::J:
        case sf::Keyboard::I:
            enemyInCage = nextAliveEnemy(enemyInCage, -1);
            if (sharedState) {
                pthread_mutex_lock(&sharedState->mtx);
                if (sharedState->active_team == TEAM_NPC)
                    sharedState->active_id = enemyInCage;
                pthread_mutex_unlock(&sharedState->mtx);
            }
            break;
        case sf::Keyboard::L:
        case sf::Keyboard::K:
            enemyInCage = nextAliveEnemy(enemyInCage, +1);
            if (sharedState) {
                pthread_mutex_lock(&sharedState->mtx);
                if (sharedState->active_team == TEAM_NPC)
                    sharedState->active_id = enemyInCage;
                pthread_mutex_unlock(&sharedState->mtx);
            }
            break;

        // ── Attack Keys — Q/E = player attacks, U/O = enemy attacks ───────────
        case sf::Keyboard::Q:
            submitPlayerAttack(ACT_STRIKE);
            break;
        case sf::Keyboard::E:
            submitPlayerAttack(ACT_EXHAUST);
            break;
        case sf::Keyboard::U:
            submitEnemyAttack(ACT_STRIKE);
            break;
        case sf::Keyboard::O:
            submitEnemyAttack(ACT_EXHAUST);
            break;

        default:
            break;
    }
}

// ── Helper: find next alive player in direction dir (+1/-1) ─────────────
int Visualizer::nextAlivePlayer(int from, int dir) const {
    if (!sharedState) return from;
    int n = sharedState->num_players;
    if (n <= 0) return from;
    int idx = from;
    for (int i = 0; i < n; ++i) {
        idx = (idx + dir + n) % n;
        if (sharedState->players[idx].alive)
            return idx;
    }
    return from; // no alive player found, keep current
}

// ── Helper: find next alive enemy in direction dir (+1/-1) ──────────────
int Visualizer::nextAliveEnemy(int from, int dir) const {
    if (!sharedState) return from;
    int n = sharedState->num_npcs;
    if (n <= 0) return from;
    int idx = from;
    for (int i = 0; i < n; ++i) {
        idx = (idx + dir + n) % n;
        if (sharedState->npcs[idx].alive)
            return idx;
    }
    return from; // no alive enemy found, keep current
}

// ── Submit a player attack to the arbiter via shared memory ─────────────
void Visualizer::submitPlayerAttack(ActionType act) {
    if (!sharedState) return;

    pthread_mutex_lock(&sharedState->mtx);

    // Gate: only when it is the player's turn (turn_order==0)
    // We do NOT require active_id==playerInCage because the user may have
    // switched fighters; we override active_id below.
    if (sharedState->turn_order != 0 || sharedState->pending.ready) {
        pthread_mutex_unlock(&sharedState->mtx);
        return;
    }

    // Make sure selected player is alive
    if (playerInCage < 0 || playerInCage >= sharedState->num_players
        || !sharedState->players[playerInCage].alive) {
        pthread_mutex_unlock(&sharedState->mtx);
        return;
    }
    // Make sure target enemy is alive; auto-pick another if not
    if (enemyInCage < 0 || enemyInCage >= sharedState->num_npcs
        || !sharedState->npcs[enemyInCage].alive) {
        enemyInCage = nextAliveEnemy(enemyInCage, +1);
        if (enemyInCage < 0 || !sharedState->npcs[enemyInCage].alive) {
            pthread_mutex_unlock(&sharedState->mtx);
            return;
        }
    }

    // Override the scheduler's active_id so the correct fighter acts
    sharedState->active_team = TEAM_PLAYER;
    sharedState->active_id   = playerInCage;

    PendingAction pa{};
    pa.action      = act;
    pa.actor_team  = TEAM_PLAYER;
    pa.actor_id    = playerInCage;
    pa.target_team = TEAM_NPC;
    pa.target_id   = enemyInCage;
    pa.ready = 1;
    sharedState->pending = pa;

    // Flip to enemy's turn
    sharedState->turn_order = 1;
    localTurnOrder = 1;

    // Trigger both fighters' attack animations
    playerFighters[playerInCage].triggerAttack();
    enemyFighters[enemyInCage].triggerAttack();

    pthread_cond_broadcast(&sharedState->action_cv);
    pthread_mutex_unlock(&sharedState->mtx);
}

// ── Submit an enemy attack to the arbiter (user presses U/O) ─────────────
void Visualizer::submitEnemyAttack(ActionType act) {
    if (!sharedState) return;

    pthread_mutex_lock(&sharedState->mtx);

    // Gate: only when it is the enemy's turn (turn_order==1)
    if (sharedState->turn_order != 1 || sharedState->pending.ready) {
        pthread_mutex_unlock(&sharedState->mtx);
        return;
    }

    // Make sure selected enemy is alive; auto-pick another if not
    if (enemyInCage < 0 || enemyInCage >= sharedState->num_npcs
        || !sharedState->npcs[enemyInCage].alive) {
        enemyInCage = nextAliveEnemy(enemyInCage, +1);
        if (enemyInCage < 0 || !sharedState->npcs[enemyInCage].alive) {
            pthread_mutex_unlock(&sharedState->mtx);
            return;
        }
    }
    // Make sure target player is alive; auto-pick another if not
    if (playerInCage < 0 || playerInCage >= sharedState->num_players
        || !sharedState->players[playerInCage].alive) {
        playerInCage = nextAlivePlayer(playerInCage, +1);
        if (playerInCage < 0 || !sharedState->players[playerInCage].alive) {
            pthread_mutex_unlock(&sharedState->mtx);
            return;
        }
    }

    // Override the scheduler's active_id so the correct enemy acts
    sharedState->active_team = TEAM_NPC;
    sharedState->active_id   = enemyInCage;
    // Confirm the target is the currently selected/visible player
    sharedState->selected_player_id = playerInCage;

    PendingAction pa{};
    pa.action      = act;
    pa.actor_team  = TEAM_NPC;
    pa.actor_id    = enemyInCage;
    pa.target_team = TEAM_PLAYER;
    pa.target_id   = playerInCage;  // always hits the selected player
    pa.ready = 1;
    sharedState->pending = pa;

    // Flip to player's turn
    sharedState->turn_order = 0;
    localTurnOrder = 0;

    // Trigger both fighters' attack animations
    enemyFighters[enemyInCage].triggerAttack();
    playerFighters[playerInCage].triggerAttack();

    pthread_cond_broadcast(&sharedState->action_cv);
    pthread_mutex_unlock(&sharedState->mtx);
}

void Visualizer::handleMouseClick(float x, float y) {
    // No particle effects in plain white UI
    (void)x;
    (void)y;
}

// update logic

void Visualizer::update(float dt) {
    if (isIntroActive) {
        if (!introFrames.empty()) {
            introFrameTimer += dt;
            float frameDuration = 1.f / 30.f; // 30 FPS for intro
            // Drain the accumulated timer, advancing one frame at a time
            while (introFrameTimer >= frameDuration && isIntroActive) {
                introFrameTimer -= frameDuration;
                introFrameIndex++;
                if (introFrameIndex >= static_cast<int>(introFrames.size())) {
                    isIntroActive = false; // Intro finished — keep last frame visible
                } else {
                    introSprite.setTexture(introFrames[introFrameIndex]);
                }
            }
        } else {
            isIntroActive = false;
        }
        return; // Skip game update during intro
    }

    // ── Advance background frame ─────────────────────────────────────────────
    if (!bgFrames.empty()) {
        bgFrameTimer += dt;
        float frameDuration = 1.f / BG_FPS;
        if (bgFrameTimer >= frameDuration) {
            bgFrameTimer -= frameDuration;
            bgFrameIndex = (bgFrameIndex + 1) % static_cast<int>(bgFrames.size());
            bgSprite.setTexture(bgFrames[bgFrameIndex]);
        }
    }

    // ── Update fighter sprite animations ─────────────────────────────────────
    for (int i = 0; i < MAX_PLAYERS; ++i) playerFighters[i].update(dt);
    for (int i = 0; i < 4; ++i) enemyFighters[i].update(dt);

    if (!sharedState) return;

    // ── Sync localTurnOrder from the authoritative shared state ──────────────
    pthread_mutex_lock(&sharedState->mtx);
    localTurnOrder = sharedState->turn_order;

    // ── Track which fighters are in the cage ─────────────────────────────────
    // Only auto-update playerInCage/enemyInCage when the scheduler assigns a
    // NEW character that the user hasn't explicitly selected yet.
    // This preserves keyboard switching while still picking a valid live fighter.
    if (sharedState->active_team == TEAM_PLAYER && sharedState->active_id >= 0
        && sharedState->active_id < MAX_PLAYERS) {
        // Only override if the user's selection is now dead
        if (!sharedState->players[playerInCage].alive)
            playerInCage = sharedState->active_id;
    }
    if (sharedState->active_team == TEAM_NPC && sharedState->active_id >= 0
        && sharedState->active_id < 4) {
        // Only override if the user's selection is now dead
        if (!sharedState->npcs[enemyInCage].alive)
            enemyInCage = sharedState->active_id;
    }

    // ── Detect HP changes → trigger attack animations ────────────────────────
    for (int i = 0; i < sharedState->num_npcs && i < 4; ++i) {
        int curHP = sharedState->npcs[i].hp;
        if (lastHPNpcs[i] > 0 && curHP < lastHPNpcs[i] && sharedState->npcs[i].alive) {
            // NPC took damage → player is attacking
            if (playerInCage >= 0 && playerInCage < MAX_PLAYERS)
                playerFighters[playerInCage].triggerAttack();
        }
        lastHPNpcs[i] = curHP;
    }
    for (int i = 0; i < sharedState->num_players && i < MAX_PLAYERS; ++i) {
        int curHP = sharedState->players[i].hp;
        if (lastHPPlayers[i] > 0 && curHP < lastHPPlayers[i] && sharedState->players[i].alive) {
            // Player took damage → enemy is attacking
            if (enemyInCage >= 0 && enemyInCage < 4)
                enemyFighters[enemyInCage].triggerAttack();
        }
        lastHPPlayers[i] = curHP;
    }
    pthread_mutex_unlock(&sharedState->mtx);

    if (sliderCount > 0) {
        quantumTime = sliders[0]->getValue();
    }
    if (sliderCount > 1) {
        schedulingPriority = static_cast<int>(sliders[1]->getValue());
    }

    // update button colors to reflect current scheduling/deadlock mode
    for (int i = 0; i < 4; ++i) {
        if (schedulingBtns[i]) {
            bool isActive = (sharedState->scheduler_mode == static_cast<SchedulerMode>(i));
            schedulingBtns[i]->setColors(
                isActive ? sf::Color(180, 220, 180) : sf::Color(220, 220, 220),
                isActive ? sf::Color(160, 210, 160) : sf::Color(200, 200, 200),
                TEXT_PRIMARY);
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (deadlockBtns[i]) {
            bool isActive = (sharedState->deadlock_strategy == static_cast<DeadlockStrategy>(i));
            deadlockBtns[i]->setColors(
                isActive ? sf::Color(200, 180, 220) : sf::Color(220, 220, 220),
                isActive ? sf::Color(190, 170, 210) : sf::Color(200, 200, 200),
                TEXT_PRIMARY);
        }
    }

    updateComponents(dt);
    updateProcessBlocks();
    updateConnections();
}

void Visualizer::updateComponents(float dt) {
    (void)dt;
    if (!sharedState) return;
    int now = now_epoch();

    pthread_mutex_lock(&sharedState->mtx);

    for (int i = 0; i < sharedState->num_players && i < playerCardCount; ++i) {
        bool isActive = (sharedState->active_team == TEAM_PLAYER && sharedState->active_id == i);
        playerCards[i]->update(sharedState->players[i], isActive, now);
    }

    for (int i = 0; i < sharedState->num_npcs && i < npcCardCount; ++i) {
        bool isActive = (sharedState->active_team == TEAM_NPC && sharedState->active_id == i);
        npcCards[i]->update(sharedState->npcs[i], isActive, now);
    }

    for (int i = 0; i < 3; ++i) {
        artifactDisplays[i]->update(sharedState->artifacts[i], i, sharedState->eclipse_present);
    }

    bool isPolling = (sharedState->active_team != -1);
    arbiterNode->update(isPolling, sharedState->active_team, sharedState->active_id);

    pthread_mutex_unlock(&sharedState->mtx);
}

void Visualizer::updateProcessBlocks() {
    pthread_mutex_lock(&sharedState->mtx);

    int now = now_epoch();
    int blockIdx = 0;

    for (int i = 0; i < sharedState->num_players && blockIdx < processBlockCount; ++i) {
        auto& p = sharedState->players[i];
        if (!p.alive) continue;

        float progress = static_cast<float>(p.stamina) / max(1, p.max_stamina);
        int state = (sharedState->active_team == TEAM_PLAYER && sharedState->active_id == i) ? 0 :
                    (p.stunned_until_epoch > now) ? 2 : 1;

        processBlocks[blockIdx]->update(progress, state, "Player " + to_string(i));
        blockIdx++;
    }

    for (int i = 0; i < sharedState->num_npcs && blockIdx < processBlockCount; ++i) {
        auto& n = sharedState->npcs[i];
        if (!n.alive) continue;

        float progress = static_cast<float>(n.stamina) / max(1, n.max_stamina);
        int state = (sharedState->active_team == TEAM_NPC && sharedState->active_id == i) ? 0 :
                    (n.stunned_until_epoch > now) ? 2 : 1;

        processBlocks[blockIdx]->update(progress, state, "NPC " + to_string(i));
        blockIdx++;
    }

    // hide unused slots — don't show [EMPTY] for slots beyond actual process count
    int activeCount = blockIdx;
    for (; blockIdx < processBlockCount; ++blockIdx) {
        processBlocks[blockIdx]->update(0.f, 3, "");  // state 3 = hidden
    }
    (void)activeCount;

    pthread_mutex_unlock(&sharedState->mtx);

    for (int i = 0; i < hipSlotCount; ++i) {
        pthread_mutex_lock(&sharedState->mtx);
        bool hasActive = (sharedState->active_team == TEAM_PLAYER &&
                         sharedState->active_id == i && i < sharedState->num_players);
        if (hasActive && sharedState->players[i].alive) {
            hipSlots[i]->setOccupied(true, i, "Player " + to_string(i));
        } else {
            hipSlots[i]->setOccupied(false);
        }
        pthread_mutex_unlock(&sharedState->mtx);
    }
}

void Visualizer::updateConnections() {
    sf::Vector2f arbiterPos = arbiterNode->getPosition();

    // Connection from arbiter (header) to left panel (players area)
    connectionLines[0].setPoints(
        arbiterPos,
        sf::Vector2f(LEFT_PANEL_WIDTH - 10.f, HEADER_HEIGHT + 80.f));
    bool active = (sharedState->active_team == TEAM_PLAYER);
    connectionLines[0].setActive(active);
    connectionLines[0].setColor(active ? ACCENT_GREEN : sf::Color(180, 180, 180, 40));

    // Connection from arbiter (header) to center (NPCs area)
    connectionLines[1].setPoints(
        arbiterPos,
        sf::Vector2f(LEFT_PANEL_WIDTH + 20.f, HEADER_HEIGHT + 80.f));
    active = (sharedState->active_team == TEAM_NPC);
    connectionLines[1].setActive(active);
    connectionLines[1].setColor(active ? ACCENT_RED : sf::Color(180, 180, 180, 40));
}

// rendering

void Visualizer::render() {
    // Background will be drawn by renderBackground() (UFC stage video frames)
    window.clear(sf::Color(0, 0, 0, 255));

    if (isIntroActive) {
        if (!introFrames.empty()) {
            window.draw(introSprite);
        }
        window.display();
        return;
    }

    renderBackground();
    renderHeader();

    switch (currentMode) {
        case ViewMode::COMBAT:
            renderCombatView();
            break;
        case ViewMode::SCHEDULER:
            renderSchedulerView();
            break;
        case ViewMode::HYBRID:
            renderHybridView();
            break;
    }

    renderFooter();
    renderConnections();
    // No particles in plain UI

    window.display();
}

void Visualizer::renderBackground() {
    if (!bgFrames.empty()) {
        // Draw the UFC stage video frame as a fullscreen background
        window.draw(bgSprite);
    } else {
        // Fallback: solid dark colour if frames failed to load
        window.clear(sf::Color(20, 20, 30, 255));
    }
}

void Visualizer::renderHeader() {
    sf::RectangleShape header(sf::Vector2f(WINDOW_WIDTH, HEADER_HEIGHT));
    header.setFillColor(BACKGROUND_PANEL);
    header.setOutlineColor(BORDER_COLOR);
    header.setOutlineThickness(1.f);
    window.draw(header);

    // Title - left side
    sf::Text title;
    title.setFont(mainFont);
    title.setString("CHRONO RIFT");
    title.setCharacterSize(FONT_TITLE);
    title.setFillColor(ACCENT_BLUE);
    title.setStyle(sf::Text::Bold);
    title.setPosition(16.f, 8.f);
    window.draw(title);

    sf::Text subtitle;
    subtitle.setFont(mainFont);
    subtitle.setString("OS Process Scheduling Visualizer");
    subtitle.setCharacterSize(FONT_SMALL);
    subtitle.setFillColor(TEXT_SECONDARY);
    subtitle.setPosition(16.f, 42.f);
    window.draw(subtitle);

    // Arbiter node centered in header
    arbiterNode->draw(window);

    // Mode label - right of center
    sf::Text modeText;
    modeText.setFont(mainFont);
    string modeStr;
    switch (currentMode) {
        case ViewMode::COMBAT:    modeStr = "[1] COMBAT";    break;
        case ViewMode::SCHEDULER: modeStr = "[2] SCHEDULER"; break;
        case ViewMode::HYBRID:    modeStr = "[3] HYBRID";    break;
    }
    modeText.setString(modeStr);
    modeText.setCharacterSize(FONT_SUBTITLE);
    modeText.setFillColor(ACCENT_YELLOW);
    modeText.setPosition(WINDOW_WIDTH / 2.f + 50.f, 14.f);
    window.draw(modeText);

    // Status - far right
    sf::Text statusText;
    statusText.setFont(mainFont);
    statusText.setCharacterSize(FONT_SMALL);
    statusText.setFillColor(TEXT_SECONDARY);
    char buf[128];
    pthread_mutex_lock(&sharedState->mtx);
    int seed = sharedState->roll_seed;
    pthread_mutex_unlock(&sharedState->mtx);

    snprintf(buf, sizeof(buf), "SEED:%d | Q:%.1fs | P:%d", seed, quantumTime, schedulingPriority);
    statusText.setString(buf);
    statusText.setPosition(WINDOW_WIDTH - 280.f, 12.f);
    window.draw(statusText);

    // Panel separator lines (drawn on every frame below the header)
    sf::RectangleShape leftDiv(sf::Vector2f(1.f, WINDOW_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT));
    leftDiv.setPosition(LEFT_PANEL_WIDTH, HEADER_HEIGHT);
    leftDiv.setFillColor(BORDER_COLOR);
    window.draw(leftDiv);

    sf::RectangleShape rightDiv(sf::Vector2f(1.f, WINDOW_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT));
    rightDiv.setPosition(WINDOW_WIDTH - RIGHT_PANEL_WIDTH, HEADER_HEIGHT);
    rightDiv.setFillColor(BORDER_COLOR);
    window.draw(rightDiv);
}

void Visualizer::renderCombatView() {
    // ═══════════════════════════════════════════════════════════════════════
    //  UFC CAGE FIGHT VIEW
    //  Background video is already rendered.  We draw fighters + roster.
    // ═══════════════════════════════════════════════════════════════════════

    renderCageFighters();
    renderTeamRoster();
    renderOpponentRoster();
}

void Visualizer::renderSchedulerView() {
    // Left panel background
    sf::RectangleShape lPanelBg(sf::Vector2f(LEFT_PANEL_WIDTH, WINDOW_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT));
    lPanelBg.setPosition(0.f, HEADER_HEIGHT);
    lPanelBg.setFillColor(BACKGROUND_PANEL);
    window.draw(lPanelBg);

    sf::Text aspHeader;
    aspHeader.setFont(mainFont);
    aspHeader.setString("ASP - ACTIVE SCHEDULING PROCESSES");
    aspHeader.setCharacterSize(FONT_SUBTITLE);
    aspHeader.setFillColor(ACCENT_YELLOW);
    aspHeader.setStyle(sf::Text::Bold);
    aspHeader.setPosition(10.f, HEADER_HEIGHT + 4.f);
    window.draw(aspHeader);

    for (int i = 0; i < processBlockCount; ++i) {
        processBlocks[i]->draw(window);
    }

    // Right panel: controls + HIP slots
    sf::Text hipHeader;
    hipHeader.setFont(mainFont);
    hipHeader.setString("HIP - HARDWARE INTERFACE");
    hipHeader.setCharacterSize(FONT_SUBTITLE);
    hipHeader.setFillColor(ACCENT_BLUE);
    hipHeader.setStyle(sf::Text::Bold);
    hipHeader.setPosition(WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 10.f, HEADER_HEIGHT + 342.f);
    window.draw(hipHeader);

    for (int i = 0; i < hipSlotCount; ++i) {
        hipSlots[i]->draw(window);
    }

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i]->draw(window);
    }
    for (int i = 0; i < sliderCount; ++i) {
        sliders[i]->draw(window);
    }
    // Scheduling mode buttons
    for (int i = 0; i < 4; ++i) {
        if (schedulingBtns[i]) schedulingBtns[i]->draw(window);
    }
    // Deadlock strategy buttons
    for (int i = 0; i < 3; ++i) {
        if (deadlockBtns[i]) deadlockBtns[i]->draw(window);
    }
}

void Visualizer::renderHybridView() {
    // Left panel background
    sf::RectangleShape lPanelBg(sf::Vector2f(LEFT_PANEL_WIDTH, WINDOW_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT));
    lPanelBg.setPosition(0.f, HEADER_HEIGHT);
    lPanelBg.setFillColor(BACKGROUND_PANEL);
    window.draw(lPanelBg);

    // Left: process blocks (first 5)
    sf::Text aspHeader;
    aspHeader.setFont(mainFont);
    aspHeader.setString("ASP PROCESSES");
    aspHeader.setCharacterSize(FONT_SUBTITLE);
    aspHeader.setFillColor(ACCENT_YELLOW);
    aspHeader.setStyle(sf::Text::Bold);
    aspHeader.setPosition(10.f, HEADER_HEIGHT + 4.f);
    window.draw(aspHeader);

    int visibleBlocks = cr_min(5, processBlockCount);
    for (int i = 0; i < visibleBlocks; ++i) {
        processBlocks[i]->draw(window);
    }

    // Center: NPCs + Artifacts
    pthread_mutex_lock(&sharedState->mtx);
    sf::Text combatHeader;
    combatHeader.setFont(mainFont);
    combatHeader.setString("COMBAT FIELD - NPCS (" + to_string(sharedState->num_npcs) + ")");
    combatHeader.setCharacterSize(FONT_SUBTITLE);
    combatHeader.setFillColor(ACCENT_BLUE);
    combatHeader.setStyle(sf::Text::Bold);
    combatHeader.setPosition(LEFT_PANEL_WIDTH + 10.f, HEADER_HEIGHT + 4.f);
    window.draw(combatHeader);
    pthread_mutex_unlock(&sharedState->mtx);

    for (int i = 0; i < sharedState->num_npcs && i < npcCardCount; ++i) {
        npcCards[i]->draw(window);
    }

    for (int i = 0; i < 3; ++i) {
        artifactDisplays[i]->draw(window);
    }

    // Right: controls + HIP slots
    sf::Text hipHeader;
    hipHeader.setFont(mainFont);
    hipHeader.setString("HIP RESOURCES");
    hipHeader.setCharacterSize(FONT_SUBTITLE);
    hipHeader.setFillColor(ACCENT_BLUE);
    hipHeader.setStyle(sf::Text::Bold);
    hipHeader.setPosition(WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 10.f, HEADER_HEIGHT + 342.f);
    window.draw(hipHeader);

    for (int i = 0; i < hipSlotCount; ++i) {
        hipSlots[i]->draw(window);
    }

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i]->draw(window);
    }
    for (int i = 0; i < sliderCount; ++i) {
        sliders[i]->draw(window);
    }
    for (int i = 0; i < 4; ++i) {
        if (schedulingBtns[i]) schedulingBtns[i]->draw(window);
    }
    for (int i = 0; i < 3; ++i) {
        if (deadlockBtns[i]) deadlockBtns[i]->draw(window);
    }
}

void Visualizer::renderFooter() {
    // Footer: two lines of keyboard hints at the very bottom
    sf::Text hintText;
    hintText.setFont(mainFont);
    hintText.setCharacterSize(FONT_TINY);
    hintText.setFillColor(TEXT_DIM);

    // Line 1: View / system controls
    hintText.setString("[1]Combat [2]Scheduler [3]Hybrid | Space:Spawn | ESC:Exit");
    hintText.setPosition(WINDOW_WIDTH - 470.f, WINDOW_HEIGHT - 32.f);
    window.draw(hintText);

    // Line 2: Combat controls
    hintText.setString("Switch Player:W/A S/D  Switch Enemy:J/I L/K  | Player Attack: Q=Strike  E=Exhaust  | Enemy Attack: U=Strike  O=Exhaust");
    hintText.setPosition(10.f, WINDOW_HEIGHT - 16.f);
    window.draw(hintText);

    // Turn indicator (centered above footer)
    if (sharedState) {
        pthread_mutex_lock(&sharedState->mtx);
        int tOrder = sharedState->turn_order;
        pthread_mutex_unlock(&sharedState->mtx);

        sf::Text turnText;
        turnText.setFont(mainFont);
        turnText.setCharacterSize(FONT_SMALL);
        bool playerTurn = (tOrder == 0);
        turnText.setFillColor(playerTurn ? sf::Color(80, 220, 120, 255)
                                        : sf::Color(255, 100, 80, 255));
        turnText.setStyle(sf::Text::Bold);
        turnText.setString(playerTurn ? ">> YOUR TURN — Press Q (Strike) or E (Exhaust)"
                                      : ">> ENEMY TURN — Press U (Strike) or O (Exhaust)");
        sf::FloatRect tb = turnText.getLocalBounds();
        turnText.setPosition(WINDOW_WIDTH / 2.f - tb.width / 2.f, WINDOW_HEIGHT - 35.f);
        window.draw(turnText);
    }
}

void Visualizer::renderConnections() {
    for (int i = 0; i < 4; ++i) {
        connectionLines[i].draw(window);
    }
}

void Visualizer::renderGameOver() {
    pthread_mutex_lock(&sharedState->mtx);
    bool won = (sharedState->win == 1);
    int turns = sharedState->turn_seq;
    int kills = sharedState->kills;
    pthread_mutex_unlock(&sharedState->mtx);

    sf::RectangleShape overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    overlay.setFillColor(sf::Color(0, 0, 0, 120));
    window.draw(overlay);

    float boxW = 500.f;
    float boxH = 300.f;
    float boxX = (WINDOW_WIDTH - boxW) / 2.f;
    float boxY = (WINDOW_HEIGHT - boxH) / 2.f;

    sf::RectangleShape box(sf::Vector2f(boxW, boxH));
    box.setPosition(boxX, boxY);
    box.setFillColor(BACKGROUND_PANEL);
    box.setOutlineColor(won ? ACCENT_GREEN : ACCENT_RED);
    box.setOutlineThickness(2.f);
    window.draw(box);

    sf::Text title;
    title.setFont(mainFont);
    title.setString(won ? "[ VICTORY ]" : "[ DEFEAT ]");
    title.setCharacterSize(36);
    title.setFillColor(won ? ACCENT_GREEN : ACCENT_RED);
    title.setStyle(sf::Text::Bold);
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition(boxX + (boxW - tb.width) / 2.f, boxY + 30.f);
    window.draw(title);

    sf::Text stats;
    stats.setFont(mainFont);
    stats.setCharacterSize(FONT_SUBTITLE);
    stats.setFillColor(TEXT_PRIMARY);

    int survivors = 0;
    pthread_mutex_lock(&sharedState->mtx);
    for (int i = 0; i < sharedState->num_players; ++i) {
        if (sharedState->players[i].alive) survivors++;
    }
    pthread_mutex_unlock(&sharedState->mtx);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "Turns Played: %d\nNPCs Killed: %d\nSurvivors: %d\nThroughput: %.2f kills/s\nAvg Turnaround: %.2fs",
        turns, kills, survivors, cpuThroughput, avgTurnaroundTime);
    stats.setString(buf);
    stats.setPosition(boxX + 40.f, boxY + 100.f);
    window.draw(stats);

    sf::Text prompt;
    prompt.setFont(mainFont);
    prompt.setString("[ Press any key to exit ]");
    prompt.setCharacterSize(FONT_BODY);
    prompt.setFillColor(ACCENT_BLUE);
    sf::FloatRect pb = prompt.getLocalBounds();
    prompt.setPosition(boxX + (boxW - pb.width) / 2.f, boxY + boxH - 50.f);
    window.draw(prompt);

    window.display();

    bool waiting = true;
    while (waiting && window.isOpen()) {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::KeyPressed || ev.type == sf::Event::MouseButtonPressed) {
                waiting = false;
            }
            if (ev.type == sf::Event::Closed) {
                waiting = false;
                running = false;
            }
        }
        sf::sleep(sf::milliseconds(50));
    }
}

// button callbacks and misc actions

void Visualizer::spawnNewProcess() {
    // No particle effects in plain UI
    totalProcessesSpawned++;
}

void Visualizer::switchMode(ViewMode mode) {
    currentMode = mode;
    // No particle effects in plain UI
}

sf::Color Visualizer::getStateColor(int state) const {
    switch (state) {
        case 0: return STATE_RUNNING;
        case 1: return STATE_READY;
        case 2: return STATE_BLOCKED;
        default: return TEXT_DIM;
    }
}

string Visualizer::actionToString(ActionType action) const {
    switch (action) {
        case ACT_NONE: return "None";
        case ACT_STRIKE: return "Strike";
        case ACT_EXHAUST: return "Exhaust";
        case ACT_USE_WEAPON: return "Use Weapon";
        case ACT_SWAP_IN: return "Swap In";
        case ACT_HEAL: return "Heal";
        case ACT_SKIP: return "Skip";
        case ACT_ULTIMATE: return "Ultimate";
        case ACT_QUIT: return "Quit";
        default: return "Unknown";
    }
}

} // namespace ChronoRift

// ═══════════════════════════════════════════════════════════════════════════
//  UFC CAGE RENDERING METHODS  (appended outside namespace, re-opened)
// ═══════════════════════════════════════════════════════════════════════════
namespace ChronoRift {

// ── Render two fighters facing each other inside the cage ────────────────
void Visualizer::renderCageFighters() {
    if (!sharedState) return;

    pthread_mutex_lock(&sharedState->mtx);
    int pIdx = playerInCage;
    int eIdx = enemyInCage;
    int pHP = 0, pMaxHP = 1, eHP = 0, eMaxHP = 1;
    bool pAlive = false, eAlive = false;
    string pName = (pIdx >= 0 && pIdx < 4) ? PLAYER_NAMES[pIdx] : "???";
    string eName = (eIdx >= 0 && eIdx < 4) ? ENEMY_NAMES[eIdx]  : "???";

    if (pIdx >= 0 && pIdx < sharedState->num_players) {
        pHP    = sharedState->players[pIdx].hp;
        pMaxHP = max(1, sharedState->players[pIdx].max_hp);
        pAlive = sharedState->players[pIdx].alive;
    }
    if (eIdx >= 0 && eIdx < sharedState->num_npcs) {
        eHP    = sharedState->npcs[eIdx].hp;
        eMaxHP = max(1, sharedState->npcs[eIdx].max_hp);
        eAlive = sharedState->npcs[eIdx].alive;
    }
    pthread_mutex_unlock(&sharedState->mtx);

    // Fighter scale: fit sprite into ~420px height
    float targetH = 420.f;
    float pScale = 1.f, eScale = 1.f;
    if (playerFighters[pIdx].isLoaded() && playerFighters[pIdx].getActiveFrameHeight() > 0)
        pScale = targetH / static_cast<float>(playerFighters[pIdx].getActiveFrameHeight());
    if (enemyFighters[eIdx].isLoaded() && enemyFighters[eIdx].getActiveFrameHeight() > 0)
        eScale = targetH / static_cast<float>(enemyFighters[eIdx].getActiveFrameHeight());

    // Fighter positions
    float pX = 180.f;                              // player on left
    float eX = WINDOW_WIDTH - 600.f;               // enemy on right
    float fighterY = HEADER_HEIGHT + 240.f;         // top of sprite

    // When EITHER fighter is attacking, BOTH move to center of the cage
    bool anyAttacking = (playerFighters[pIdx].getState() == FighterSprite::ANIM_ATTACKING)
                     || (enemyFighters[eIdx].getState() == FighterSprite::ANIM_ATTACKING);
    float pOffset = anyAttacking ? 320.f : 0.f;
    float eOffset = anyAttacking ? -320.f : 0.f;

    // Draw player fighter (faces right)
    if (pAlive && playerFighters[pIdx].isLoaded()) {
        playerFighters[pIdx].draw(window, pX + pOffset, fighterY, pScale, false);
    }

    // Draw enemy fighter (faces left)
    if (eAlive && enemyFighters[eIdx].isLoaded()) {
        enemyFighters[eIdx].draw(window, eX + eOffset, fighterY, eScale, false);
    }

    // ── HP Bars ──────────────────────────────────────────────────────────
    float hpBarW = 280.f;
    float hpBarY = HEADER_HEIGHT + 90.f;

    // Player HP bar (left-aligned)
    renderFighterHP(100.f, hpBarY, hpBarW, pHP, pMaxHP, pName,
                    sf::Color(60, 180, 80, 255), false);

    // Enemy HP bar (right-aligned)
    renderFighterHP(WINDOW_WIDTH - 100.f - hpBarW, hpBarY, hpBarW, eHP, eMaxHP, eName,
                    sf::Color(200, 60, 60, 255), true);

    // ── VS Badge in center ──────────────────────────────────────────────
    sf::Text vsText;
    vsText.setFont(mainFont);
    vsText.setString("VS");
    vsText.setCharacterSize(42);
    vsText.setFillColor(sf::Color(255, 215, 0, 255));
    vsText.setStyle(sf::Text::Bold);
    sf::FloatRect vb = vsText.getLocalBounds();
    vsText.setPosition(WINDOW_WIDTH / 2.f - vb.width / 2.f,
                       HEADER_HEIGHT + 260.f);
    window.draw(vsText);

    // ── Artifacts row (compact, centered above roster) ──────────────────
    float artRowY = CAGE_BOTTOM - 62.f;
    float artW    = 100.f;
    float artGap  = 15.f;
    float artTotalW = 3.f * artW + 2.f * artGap;
    float artStartX = (WINDOW_WIDTH - artTotalW) / 2.f;

    sf::Text artLabel;
    artLabel.setFont(mainFont);
    artLabel.setString("ARTIFACTS");
    artLabel.setCharacterSize(FONT_SMALL);
    artLabel.setFillColor(sf::Color(200, 170, 255, 200));
    artLabel.setStyle(sf::Text::Bold);
    sf::FloatRect alb = artLabel.getLocalBounds();
    artLabel.setPosition(WINDOW_WIDTH / 2.f - alb.width / 2.f, artRowY - 18.f);
    window.draw(artLabel);

    for (int i = 0; i < 3; ++i) {
        artifactDisplays[i]->draw(window);
    }
}

// ── Render HP bar with name label ────────────────────────────────────────
void Visualizer::renderFighterHP(float x, float y, float width,
                                  int hp, int maxHp,
                                  const string& name, sf::Color barColor,
                                  bool /*alignRight*/) {
    // Name label above bar
    sf::Text nameText;
    nameText.setFont(mainFont);
    nameText.setString(name);
    nameText.setCharacterSize(FONT_SUBTITLE);
    nameText.setFillColor(sf::Color(255, 255, 255, 230));
    nameText.setStyle(sf::Text::Bold);
    nameText.setPosition(x, y - 2.f);
    window.draw(nameText);

    // Bar background
    float barH = 18.f;
    float barY = y + 20.f;
    sf::RectangleShape barBg(sf::Vector2f(width, barH));
    barBg.setPosition(x, barY);
    barBg.setFillColor(sf::Color(40, 40, 40, 180));
    barBg.setOutlineColor(sf::Color(100, 100, 100, 200));
    barBg.setOutlineThickness(1.f);
    window.draw(barBg);

    // Bar fill
    float ratio = static_cast<float>(hp) / static_cast<float>(max(1, maxHp));
    ratio = (ratio < 0.f) ? 0.f : (ratio > 1.f) ? 1.f : ratio;

    // Color shifts: green > yellow > red based on HP ratio
    sf::Color fillColor = barColor;
    if (ratio > 0.5f) fillColor = sf::Color(60, 180, 80, 255);
    else if (ratio > 0.2f) fillColor = sf::Color(220, 180, 30, 255);
    else fillColor = sf::Color(200, 50, 50, 255);

    sf::RectangleShape barFill(sf::Vector2f(width * ratio, barH));
    barFill.setPosition(x, barY);
    barFill.setFillColor(fillColor);
    window.draw(barFill);

    // HP text on bar
    char hpBuf[32];
    snprintf(hpBuf, sizeof(hpBuf), "%d / %d", hp, maxHp);
    sf::Text hpText;
    hpText.setFont(mainFont);
    hpText.setString(hpBuf);
    hpText.setCharacterSize(FONT_TINY);
    hpText.setFillColor(sf::Color(255, 255, 255, 220));
    hpText.setPosition(x + 6.f, barY + 1.f);
    window.draw(hpText);
}

// ── Team Roster (bottom-left) ────────────────────────────────────────────
void Visualizer::renderTeamRoster() {
    if (!sharedState) return;
    float rosterY = CAGE_BOTTOM;

    // Semi-transparent dark panel background
    sf::RectangleShape panel(sf::Vector2f(WINDOW_WIDTH / 2.f - 10.f, ROSTER_HEIGHT));
    panel.setPosition(0.f, rosterY);
    panel.setFillColor(sf::Color(10, 10, 30, 180));
    window.draw(panel);

    // "TEAM ROSTER" label
    sf::Text label;
    label.setFont(mainFont);
    label.setString("TEAM ROSTER");
    label.setCharacterSize(FONT_SUBTITLE);
    label.setFillColor(sf::Color(80, 200, 255, 255));
    label.setStyle(sf::Text::Bold);
    label.setPosition(20.f, rosterY + 6.f);
    window.draw(label);

    // Blue accent line under label
    sf::RectangleShape accentLine(sf::Vector2f(WINDOW_WIDTH / 2.f - 30.f, 2.f));
    accentLine.setPosition(15.f, rosterY + 28.f);
    accentLine.setFillColor(sf::Color(60, 160, 255, 180));
    window.draw(accentLine);

    // Portrait cards
    float cardW = 110.f;
    float cardH = 140.f;
    float cardGap = 15.f;
    float totalCardsW = 4.f * cardW + 3.f * cardGap;
    float startX = (WINDOW_WIDTH / 2.f - totalCardsW) / 2.f;
    float startY = rosterY + 38.f;

    pthread_mutex_lock(&sharedState->mtx);
    int numP = sharedState->num_players;

    for (int i = 0; i < 4 && i < numP; ++i) {
        float cx = startX + i * (cardW + cardGap);
        float cy = startY;
        bool isActive = (i == playerInCage);
        bool isAlive  = sharedState->players[i].alive;

        // Card background
        sf::RectangleShape card(sf::Vector2f(cardW, cardH));
        card.setPosition(cx, cy);

        if (!isAlive) {
            card.setFillColor(sf::Color(30, 30, 30, 150));
            card.setOutlineColor(sf::Color(80, 80, 80, 150));
        } else if (isActive) {
            card.setFillColor(sf::Color(20, 60, 100, 200));
            card.setOutlineColor(sf::Color(80, 200, 255, 255));
        } else {
            card.setFillColor(sf::Color(20, 30, 50, 180));
            card.setOutlineColor(sf::Color(60, 100, 160, 180));
        }
        card.setOutlineThickness(isActive ? 3.f : 1.f);
        window.draw(card);

        // Portrait: use first frame of standing spritesheet
        if (playerFighters[i].isLoaded()) {
            sf::Sprite portrait;
            portrait.setTexture(playerFighters[i].getStandingTexture());
            portrait.setTextureRect(playerFighters[i].getPortraitRect());
            float fw = static_cast<float>(playerFighters[i].getFrameWidth());
            float fh = static_cast<float>(playerFighters[i].getFrameHeight());
            float portraitH = cardH - 30.f;  // leave room for name
            float scale = portraitH / fh;
            float scaledW = fw * scale;

            // Center the portrait horizontally in the card
            float offsetX = (cardW - scaledW) / 2.f;
            portrait.setPosition(cx + offsetX, cy);
            portrait.setScale(scale, scale);
            if (!isAlive) portrait.setColor(sf::Color(100, 100, 100, 150));
            window.draw(portrait);
        }

        // Fighter name below portrait
        sf::Text nameText;
        nameText.setFont(mainFont);
        nameText.setString(PLAYER_NAMES[i]);
        nameText.setCharacterSize(FONT_SMALL);
        nameText.setFillColor(isAlive ? sf::Color(200, 230, 255, 255) : sf::Color(120, 120, 120, 180));
        nameText.setStyle(sf::Text::Bold);
        sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setPosition(cx + (cardW - nb.width) / 2.f, cy + cardH - 24.f);
        window.draw(nameText);

        // Small HP bar at bottom of card
        if (isAlive) {
            float barW = cardW - 10.f;
            float barH = 4.f;
            float barX = cx + 5.f;
            float barY2 = cy + cardH - 6.f;
            float hpRatio = static_cast<float>(sharedState->players[i].hp) /
                            static_cast<float>(max(1, sharedState->players[i].max_hp));

            sf::RectangleShape hpBg(sf::Vector2f(barW, barH));
            hpBg.setPosition(barX, barY2);
            hpBg.setFillColor(sf::Color(40, 40, 40, 180));
            window.draw(hpBg);

            sf::RectangleShape hpFill(sf::Vector2f(barW * hpRatio, barH));
            hpFill.setPosition(barX, barY2);
            hpFill.setFillColor(hpRatio > 0.5f ? sf::Color(60, 200, 80) :
                                hpRatio > 0.2f ? sf::Color(220, 180, 30) :
                                                  sf::Color(200, 50, 50));
            window.draw(hpFill);
        }

        // DEAD overlay
        if (!isAlive) {
            sf::Text deadText;
            deadText.setFont(mainFont);
            deadText.setString("DEAD");
            deadText.setCharacterSize(FONT_HEADER);
            deadText.setFillColor(sf::Color(200, 50, 50, 200));
            deadText.setStyle(sf::Text::Bold);
            sf::FloatRect db = deadText.getLocalBounds();
            deadText.setPosition(cx + (cardW - db.width) / 2.f,
                                 cy + (cardH - db.height) / 2.f);
            window.draw(deadText);
        }
    }

    pthread_mutex_unlock(&sharedState->mtx);
}

// ── Opponent Roster (bottom-right) ───────────────────────────────────────
void Visualizer::renderOpponentRoster() {
    if (!sharedState) return;
    float rosterY = CAGE_BOTTOM;
    float panelX = WINDOW_WIDTH / 2.f + 10.f;
    float panelW = WINDOW_WIDTH / 2.f - 10.f;

    // Semi-transparent dark panel background
    sf::RectangleShape panel(sf::Vector2f(panelW, ROSTER_HEIGHT));
    panel.setPosition(panelX, rosterY);
    panel.setFillColor(sf::Color(30, 10, 10, 180));
    window.draw(panel);

    // "OPPONENT ROSTER" label
    sf::Text label;
    label.setFont(mainFont);
    label.setString("OPPONENT ROSTER");
    label.setCharacterSize(FONT_SUBTITLE);
    label.setFillColor(sf::Color(255, 100, 80, 255));
    label.setStyle(sf::Text::Bold);
    label.setPosition(panelX + 20.f, rosterY + 6.f);
    window.draw(label);

    // Red accent line under label
    sf::RectangleShape accentLine(sf::Vector2f(panelW - 30.f, 2.f));
    accentLine.setPosition(panelX + 15.f, rosterY + 28.f);
    accentLine.setFillColor(sf::Color(255, 80, 60, 180));
    window.draw(accentLine);

    // Portrait cards
    float cardW = 110.f;
    float cardH = 140.f;
    float cardGap = 15.f;
    float totalCardsW = 4.f * cardW + 3.f * cardGap;
    float startX = panelX + (panelW - totalCardsW) / 2.f;
    float startY = rosterY + 38.f;

    pthread_mutex_lock(&sharedState->mtx);
    int numN = sharedState->num_npcs;

    for (int i = 0; i < 4 && i < numN; ++i) {
        float cx = startX + i * (cardW + cardGap);
        float cy = startY;
        bool isActive = (i == enemyInCage);
        bool isAlive  = sharedState->npcs[i].alive;

        // Card background
        sf::RectangleShape card(sf::Vector2f(cardW, cardH));
        card.setPosition(cx, cy);

        if (!isAlive) {
            card.setFillColor(sf::Color(30, 30, 30, 150));
            card.setOutlineColor(sf::Color(80, 80, 80, 150));
        } else if (isActive) {
            card.setFillColor(sf::Color(100, 20, 20, 200));
            card.setOutlineColor(sf::Color(255, 100, 80, 255));
        } else {
            card.setFillColor(sf::Color(50, 20, 20, 180));
            card.setOutlineColor(sf::Color(160, 60, 60, 180));
        }
        card.setOutlineThickness(isActive ? 3.f : 1.f);
        window.draw(card);

        // Portrait: use first frame of standing spritesheet
        if (enemyFighters[i].isLoaded()) {
            sf::Sprite portrait;
            portrait.setTexture(enemyFighters[i].getStandingTexture());
            portrait.setTextureRect(enemyFighters[i].getPortraitRect());
            float fw = static_cast<float>(enemyFighters[i].getFrameWidth());
            float fh = static_cast<float>(enemyFighters[i].getFrameHeight());
            float portraitH = cardH - 30.f;
            float scale = portraitH / fh;
            float scaledW = fw * scale;

            float offsetX = (cardW - scaledW) / 2.f;
            portrait.setPosition(cx + offsetX, cy);
            portrait.setScale(scale, scale);
            if (!isAlive) portrait.setColor(sf::Color(100, 100, 100, 150));
            window.draw(portrait);
        }

        // Fighter name below portrait
        sf::Text nameText;
        nameText.setFont(mainFont);
        nameText.setString(ENEMY_NAMES[i]);
        nameText.setCharacterSize(FONT_SMALL);
        nameText.setFillColor(isAlive ? sf::Color(255, 200, 200, 255) : sf::Color(120, 120, 120, 180));
        nameText.setStyle(sf::Text::Bold);
        sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setPosition(cx + (cardW - nb.width) / 2.f, cy + cardH - 24.f);
        window.draw(nameText);

        // Small HP bar at bottom of card
        if (isAlive) {
            float barW = cardW - 10.f;
            float barH = 4.f;
            float barX = cx + 5.f;
            float barY2 = cy + cardH - 6.f;
            float hpRatio = static_cast<float>(sharedState->npcs[i].hp) /
                            static_cast<float>(max(1, sharedState->npcs[i].max_hp));

            sf::RectangleShape hpBg(sf::Vector2f(barW, barH));
            hpBg.setPosition(barX, barY2);
            hpBg.setFillColor(sf::Color(40, 40, 40, 180));
            window.draw(hpBg);

            sf::RectangleShape hpFill(sf::Vector2f(barW * hpRatio, barH));
            hpFill.setPosition(barX, barY2);
            hpFill.setFillColor(hpRatio > 0.5f ? sf::Color(60, 200, 80) :
                                hpRatio > 0.2f ? sf::Color(220, 180, 30) :
                                                  sf::Color(200, 50, 50));
            window.draw(hpFill);
        }

        // DEAD overlay
        if (!isAlive) {
            sf::Text deadText;
            deadText.setFont(mainFont);
            deadText.setString("DEAD");
            deadText.setCharacterSize(FONT_HEADER);
            deadText.setFillColor(sf::Color(200, 50, 50, 200));
            deadText.setStyle(sf::Text::Bold);
            sf::FloatRect db = deadText.getLocalBounds();
            deadText.setPosition(cx + (cardW - db.width) / 2.f,
                                 cy + (cardH - db.height) / 2.f);
            window.draw(deadText);
        }
    }

    pthread_mutex_unlock(&sharedState->mtx);
}

} // namespace ChronoRift

