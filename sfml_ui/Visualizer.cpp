#include "Visualizer.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

Visualizer::Visualizer()
    : window(sf::VideoMode(static_cast<unsigned>(WINDOW_WIDTH),
                           static_cast<unsigned>(WINDOW_HEIGHT)),
             "Chrono Rift — OS Process Visualizer", sf::Style::Close),
      running(false), sharedState(nullptr), currentMode(ViewMode::HYBRID),
      simulationTime(0.f), avgWaitingTime(0.f), avgTurnaroundTime(0.f),
      cpuThroughput(0.f), totalProcessesSpawned(0), lastTurnSeq(0), lastKills(0),
      quantumTime(2.0f), schedulingPriority(5) {}

Visualizer::~Visualizer() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

bool Visualizer::initialize() {
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    if (!loadFonts()) {
        std::cerr << "Failed to load fonts!" << std::endl;
        return false;
    }

    initComponents();
    running = true;
    return true;
}

bool Visualizer::loadFonts() {
    // Try multiple system font locations
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
        // macOS paths
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        // Windows paths (for WSL/cross-platform)
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
        std::cerr << "WARNING: Could not load font. Text rendering will fail." << std::endl;
        // Create a minimal fallback
        return false;
    }

    // Try to load monospace font for metrics
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
        monoFont = mainFont; // fallback
    }

    return true;
}

void Visualizer::initComponents() {
    initPlayerCards();
    initNpcCards();
    initArtifacts();
    initControls();
    initSchedulerView();

    // Log panel
    logPanel = std::make_unique<LogPanel>(
        20.f, WINDOW_HEIGHT - FOOTER_HEIGHT + 10.f,
        LEFT_PANEL_WIDTH - 40.f, FOOTER_HEIGHT - 40.f, mainFont);

    // Metrics dashboard
    metricsDashboard = std::make_unique<MetricsDashboard>(
        WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 20.f, 80.f,
        RIGHT_PANEL_WIDTH - 40.f, mainFont);

    // Gantt chart
    ganttChart = std::make_unique<GanttChart>(
        LEFT_PANEL_WIDTH + 20.f, WINDOW_HEIGHT - FOOTER_HEIGHT + 20.f,
        WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH - 40.f,
        FOOTER_HEIGHT - 60.f, mainFont);

    // Arbiter node (center top)
    arbiterNode = std::make_unique<ArbiterNode>(
        WINDOW_WIDTH / 2.f, HEADER_HEIGHT + 60.f, 40.f, mainFont);

    // Connection lines from arbiter to panels
    connectionLines.resize(4);
}

void Visualizer::initPlayerCards() {
    float cardW = (WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH - 60.f) / 2.f;
    float cardH = 85.f;
    float startX = LEFT_PANEL_WIDTH + 20.f;
    float startY = HEADER_HEIGHT + 130.f;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        auto card = std::make_unique<CharacterCard>(
            startX, startY + i * (cardH + 8.f), cardW, cardH, mainFont, true);
        playerCards.push_back(std::move(card));
    }
}

void Visualizer::initNpcCards() {
    float cardW = (WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH - 60.f) / 2.f;
    float cardH = 85.f;
    float startX = LEFT_PANEL_WIDTH + cardW + 40.f;
    float startY = HEADER_HEIGHT + 130.f;

    for (int i = 0; i < MAX_NPCS; ++i) {
        auto card = std::make_unique<CharacterCard>(
            startX, startY + i * (cardH + 8.f), cardW, cardH, mainFont, false);
        npcCards.push_back(std::move(card));
    }
}

void Visualizer::initArtifacts() {
    float artW = (WINDOW_WIDTH - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH - 80.f) / 3.f;
    float startX = LEFT_PANEL_WIDTH + 20.f;
    float startY = HEADER_HEIGHT + 500.f;

    for (int i = 0; i < 3; ++i) {
        auto art = std::make_unique<ArtifactDisplay>(
            startX + i * (artW + 10.f), startY, artW, mainFont);
        artifactDisplays.push_back(std::move(art));
    }
}

void Visualizer::initControls() {
    float btnX = WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 30.f;
    float btnY = 240.f;

    // Spawn Process button
    auto spawnBtn = std::make_unique<Button>(
        btnX, btnY, 130.f, 36.f, "+ PROCESS", mainFont, FONT_SMALL);
    spawnBtn->setColors(NEON_GREEN, sf::Color(100, 255, 100, 255), sf::Color::Black);
    spawnBtn->setCallback([this]() { spawnNewProcess(); });
    buttons.push_back(std::move(spawnBtn));

    // Mode switch buttons
    auto combatBtn = std::make_unique<Button>(
        btnX + 140.f, btnY, 80.f, 36.f, "COMBAT", mainFont, FONT_SMALL);
    combatBtn->setColors(NEON_BLUE, NEON_CYAN, sf::Color::White);
    combatBtn->setCallback([this]() { switchMode(ViewMode::COMBAT); });
    buttons.push_back(std::move(combatBtn));

    // Quantum slider
    auto qSlider = std::make_unique<Slider>(
        btnX, btnY + 55.f, 200.f, 0.5f, 10.f, quantumTime,
        "Quantum Time (s)", mainFont);
    sliders.push_back(std::move(qSlider));

    // Priority slider
    auto pSlider = std::make_unique<Slider>(
        btnX, btnY + 105.f, 200.f, 1.f, 10.f, static_cast<float>(schedulingPriority),
        "Priority Level", mainFont);
    sliders.push_back(std::move(pSlider));
}

void Visualizer::initSchedulerView() {
    // ASP Process blocks (left side)
    float blockW = LEFT_PANEL_WIDTH - 40.f;
    float blockH = 50.f;
    float startX = 20.f;
    float startY = HEADER_HEIGHT + 80.f;

    for (int i = 0; i < 6; ++i) {
        auto block = std::make_unique<ProcessBlock>(
            startX, startY + i * (blockH + 8.f), blockW, blockH, i + 100, mainFont);
        processBlocks.push_back(std::move(block));
    }

    // HIP CPU slots (right side)
    float slotW = RIGHT_PANEL_WIDTH - 40.f;
    float slotH = 55.f;
    float slotX = WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 20.f;
    float slotY = 420.f;

    for (int i = 0; i < 4; ++i) {
        auto slot = std::make_unique<HIPSlot>(
            slotX, slotY + i * (slotH + 8.f), slotW, slotH, i, "Core", mainFont);
        hipSlots.push_back(std::move(slot));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════

void Visualizer::run(SharedState* state) {
    sharedState = state;
    gameClock.restart();

    while (running && window.isOpen()) {
        float dt = deltaClock.restart().asSeconds();
        simulationTime = gameClock.getElapsedTime().asSeconds();

        handleEvents();
        update(dt);
        render();

        // Check for game over
        if (sharedState && !sharedState->running) {
            renderGameOver();
            sf::sleep(sf::seconds(3));
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

// ═══════════════════════════════════════════════════════════════════════════
// EVENT HANDLING
// ═══════════════════════════════════════════════════════════════════════════

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

        // Pass events to UI components
        for (auto& btn : buttons) {
            btn->handleEvent(event, window);
        }
        for (auto& slider : sliders) {
            slider->handleEvent(event, window);
        }

        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            handleMouseClick(event.mouseButton.x, event.mouseButton.y);
        }
    }
}

void Visualizer::handleKeyPress(sf::Keyboard::Key key) {
    switch (key) {
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
        default:
            break;
    }
}

void Visualizer::handleMouseClick(float x, float y) {
    // Spawn particles at click location
    particles.emit(x, y, NEON_CYAN, 8);
}

// ═══════════════════════════════════════════════════════════════════════════
// UPDATE
// ═══════════════════════════════════════════════════════════════════════════

void Visualizer::update(float dt) {
    particles.update(dt);

    if (!sharedState) return;

    // Update quantum from slider
    if (!sliders.empty()) {
        quantumTime = sliders[0]->getValue();
    }
    if (sliders.size() > 1) {
        schedulingPriority = static_cast<int>(sliders[1]->getValue());
    }

    updateComponents(dt);
    updateMetrics();
    updateGanttChart();
    updateProcessBlocks();
    updateConnections();
}

void Visualizer::updateComponents(float dt) {
    int now = now_epoch();

    pthread_mutex_lock(&sharedState->mtx);

    // Update player cards
    for (int i = 0; i < sharedState->num_players && i < static_cast<int>(playerCards.size()); ++i) {
        bool isActive = (sharedState->active_team == TEAM_PLAYER && sharedState->active_id == i);
        playerCards[i]->update(sharedState->players[i], isActive, now);
    }

    // Update NPC cards
    for (int i = 0; i < sharedState->num_npcs && i < static_cast<int>(npcCards.size()); ++i) {
        bool isActive = (sharedState->active_team == TEAM_NPC && sharedState->active_id == i);
        npcCards[i]->update(sharedState->npcs[i], isActive, now);
    }

    // Update artifacts
    for (int i = 0; i < 3; ++i) {
        artifactDisplays[i]->update(sharedState->artifacts[i], i, sharedState->eclipse_present);
    }

    // Update log panel
    logPanel->update(sharedState);

    // Update arbiter node
    bool isPolling = (sharedState->active_team != -1);
    arbiterNode->update(isPolling, sharedState->active_team, sharedState->active_id);

    pthread_mutex_unlock(&sharedState->mtx);
}

void Visualizer::updateMetrics() {
    float elapsed = metricsClock.getElapsedTime().asSeconds();
    if (elapsed < 0.5f) return;
    metricsClock.restart();

    pthread_mutex_lock(&sharedState->mtx);

    int turns = sharedState->turn_seq;
    int kills = sharedState->kills;

    // Calculate waiting time approximation
    int waitingPlayers = 0;
    int waitingNPCs = 0;
    for (int i = 0; i < sharedState->num_players; ++i) {
        if (sharedState->players[i].alive &&
            sharedState->players[i].stamina < sharedState->players[i].max_stamina) {
            waitingPlayers++;
        }
    }
    for (int i = 0; i < sharedState->num_npcs; ++i) {
        if (sharedState->npcs[i].alive &&
            sharedState->npcs[i].stamina < sharedState->npcs[i].max_stamina) {
            waitingNPCs++;
        }
    }

    avgWaitingTime = (waitingPlayers + waitingNPCs) * quantumTime / std::max(1, sharedState->num_players + sharedState->num_npcs);
    avgTurnaroundTime = static_cast<float>(turns) * quantumTime / std::max(1, kills);
    cpuThroughput = static_cast<float>(kills) / std::max(1.f, simulationTime);

    metricsDashboard->update(avgWaitingTime, avgTurnaroundTime, cpuThroughput,
                              turns, kills,
                              sharedState->num_players + sharedState->num_npcs);

    pthread_mutex_unlock(&sharedState->mtx);
}

void Visualizer::updateGanttChart() {
    pthread_mutex_lock(&sharedState->mtx);

    // Add Gantt entries for active entity
    if (sharedState->turn_seq != lastTurnSeq) {
        float dur = quantumTime;
        ganttChart->addEntry(sharedState->active_id, static_cast<TeamType>(sharedState->active_team),
                             simulationTime - dur, dur);
        lastTurnSeq = sharedState->turn_seq;
    }

    ganttChart->update(simulationTime);

    pthread_mutex_unlock(&sharedState->mtx);
}

void Visualizer::updateProcessBlocks() {
    pthread_mutex_lock(&sharedState->mtx);

    int now = now_epoch();

    // Map characters to process blocks
    int blockIdx = 0;

    // Player processes
    for (int i = 0; i < sharedState->num_players && blockIdx < static_cast<int>(processBlocks.size()); ++i) {
        auto& p = sharedState->players[i];
        if (!p.alive) continue;

        float progress = static_cast<float>(p.stamina) / std::max(1, p.max_stamina);
        int state = (sharedState->active_team == TEAM_PLAYER && sharedState->active_id == i) ? 0 :
                    (p.stunned_until_epoch > now) ? 2 : 1;

        processBlocks[blockIdx]->update(progress, state, "Player " + std::to_string(i));
        blockIdx++;
    }

    // NPC processes
    for (int i = 0; i < sharedState->num_npcs && blockIdx < static_cast<int>(processBlocks.size()); ++i) {
        auto& n = sharedState->npcs[i];
        if (!n.alive) continue;

        float progress = static_cast<float>(n.stamina) / std::max(1, n.max_stamina);
        int state = (sharedState->active_team == TEAM_NPC && sharedState->active_id == i) ? 0 :
                    (n.stunned_until_epoch > now) ? 2 : 1;

        processBlocks[blockIdx]->update(progress, state, "NPC " + std::to_string(i));
        blockIdx++;
    }

    // Hide unused blocks
    for (; blockIdx < static_cast<int>(processBlocks.size()); ++blockIdx) {
        processBlocks[blockIdx]->update(0.f, 1, "[EMPTY]");
    }

    pthread_mutex_unlock(&sharedState->mtx);

    // Update HIP slots based on active assignments
    for (int i = 0; i < static_cast<int>(hipSlots.size()); ++i) {
        pthread_mutex_lock(&sharedState->mtx);
        bool hasActive = (sharedState->active_team == TEAM_PLAYER &&
                         sharedState->active_id == i && i < sharedState->num_players);
        if (hasActive && sharedState->players[i].alive) {
            hipSlots[i]->setOccupied(true, i, "Player " + std::to_string(i));
        } else {
            hipSlots[i]->setOccupied(false);
        }
        pthread_mutex_unlock(&sharedState->mtx);
    }
}

void Visualizer::updateConnections() {
    sf::Vector2f arbiterPos = arbiterNode->getPosition();

    // Connection to player panel
    if (connectionLines.size() > 0) {
        connectionLines[0].setPoints(
            arbiterPos,
            sf::Vector2f(LEFT_PANEL_WIDTH + 20.f, HEADER_HEIGHT + 170.f));
        bool active = (sharedState->active_team == TEAM_PLAYER);
        connectionLines[0].setActive(active);
        connectionLines[0].setColor(active ? NEON_GREEN : sf::Color(0, 255, 255, 40));
    }

    // Connection to NPC panel
    if (connectionLines.size() > 1) {
        connectionLines[1].setPoints(
            arbiterPos,
            sf::Vector2f(WINDOW_WIDTH / 2.f + 20.f, HEADER_HEIGHT + 170.f));
        bool active = (sharedState->active_team == TEAM_NPC);
        connectionLines[1].setActive(active);
        connectionLines[1].setColor(active ? NEON_RED : sf::Color(0, 255, 255, 40));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// RENDERING
// ═══════════════════════════════════════════════════════════════════════════

void Visualizer::render() {
    window.clear(BACKGROUND_DARK);

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
    particles.draw(window);

    window.display();
}

void Visualizer::renderBackground() {
    // Subtle grid pattern
    sf::VertexArray grid(sf::Lines);
    for (float x = 0; x < WINDOW_WIDTH; x += 40.f) {
        grid.append(sf::Vertex(sf::Vector2f(x, 0.f), sf::Color(30, 35, 50, 60)));
        grid.append(sf::Vertex(sf::Vector2f(x, WINDOW_HEIGHT), sf::Color(30, 35, 50, 60)));
    }
    for (float y = 0; y < WINDOW_HEIGHT; y += 40.f) {
        grid.append(sf::Vertex(sf::Vector2f(0.f, y), sf::Color(30, 35, 50, 60)));
        grid.append(sf::Vertex(sf::Vector2f(WINDOW_WIDTH, y), sf::Color(30, 35, 50, 60)));
    }
    window.draw(grid);
}

void Visualizer::renderHeader() {
    // Header background
    sf::RectangleShape header(sf::Vector2f(WINDOW_WIDTH, HEADER_HEIGHT));
    header.setFillColor(BACKGROUND_PANEL);
    header.setOutlineColor(NEON_CYAN);
    header.setOutlineThickness(1.f);
    window.draw(header);

    // Title
    sf::Text title;
    title.setFont(mainFont);
    title.setString("CHRONO RIFT");
    title.setCharacterSize(FONT_TITLE);
    title.setFillColor(NEON_CYAN);
    title.setStyle(sf::Text::Bold);
    title.setPosition(20.f, 12.f);
    window.draw(title);

    // Subtitle
    sf::Text subtitle;
    subtitle.setFont(mainFont);
    subtitle.setString("OS Process Scheduling Visualizer");
    subtitle.setCharacterSize(FONT_SMALL);
    subtitle.setFillColor(TEXT_SECONDARY);
    subtitle.setPosition(20.f, 42.f);
    window.draw(subtitle);

    // Mode indicator
    sf::Text modeText;
    modeText.setFont(mainFont);
    std::string modeStr;
    switch (currentMode) {
        case ViewMode::COMBAT: modeStr = "[1] COMBAT"; break;
        case ViewMode::SCHEDULER: modeStr = "[2] SCHEDULER"; break;
        case ViewMode::HYBRID: modeStr = "[3] HYBRID"; break;
    }
    modeText.setString(modeStr);
    modeText.setCharacterSize(FONT_SUBTITLE);
    modeText.setFillColor(NEON_YELLOW);
    modeText.setPosition(WINDOW_WIDTH / 2.f - 80.f, 18.f);
    window.draw(modeText);

    // Status indicators
    sf::Text statusText;
    statusText.setFont(mainFont);
    statusText.setCharacterSize(FONT_SMALL);
    statusText.setFillColor(TEXT_SECONDARY);
    std::ostringstream ss;
    ss << "Q:" << quantumTime << "s | P:" << schedulingPriority;
    statusText.setString(ss.str());
    statusText.setPosition(WINDOW_WIDTH - 200.f, 20.f);
    window.draw(statusText);
}

void Visualizer::renderCombatView() {
    pthread_mutex_lock(&sharedState->mtx);

    // Player panel header
    sf::Text pHeader;
    pHeader.setFont(mainFont);
    pHeader.setString("PLAYERS (" + std::to_string(sharedState->num_players) + ")");
    pHeader.setCharacterSize(FONT_HEADER);
    pHeader.setFillColor(NEON_GREEN);
    pHeader.setStyle(sf::Text::Bold);
    pHeader.setPosition(LEFT_PANEL_WIDTH + 20.f, HEADER_HEIGHT + 95.f);
    window.draw(pHeader);

    // NPC panel header
    sf::Text nHeader;
    nHeader.setFont(mainFont);
    nHeader.setString("NPCS (" + std::to_string(sharedState->num_npcs) + ")");
    nHeader.setCharacterSize(FONT_HEADER);
    nHeader.setFillColor(NEON_RED);
    nHeader.setStyle(sf::Text::Bold);
    nHeader.setPosition(WINDOW_WIDTH / 2.f + 20.f, HEADER_HEIGHT + 95.f);
    window.draw(nHeader);

    pthread_mutex_unlock(&sharedState->mtx);

    // Draw character cards
    for (int i = 0; i < sharedState->num_players && i < static_cast<int>(playerCards.size()); ++i) {
        playerCards[i]->draw(window);
    }
    for (int i = 0; i < sharedState->num_npcs && i < static_cast<int>(npcCards.size()); ++i) {
        npcCards[i]->draw(window);
    }

    // Draw artifacts
    for (auto& art : artifactDisplays) {
        art->draw(window);
    }

    // Draw arbiter
    arbiterNode->draw(window);

    // Draw metrics
    metricsDashboard->draw(window);
}

void Visualizer::renderSchedulerView() {
    // Left panel - ASP Process blocks
    sf::Text aspHeader;
    aspHeader.setFont(mainFont);
    aspHeader.setString("ASP — ACTIVE SCHEDULING PROCESSES");
    aspHeader.setCharacterSize(FONT_HEADER);
    aspHeader.setFillColor(NEON_YELLOW);
    aspHeader.setStyle(sf::Text::Bold);
    aspHeader.setPosition(20.f, HEADER_HEIGHT + 75.f);
    window.draw(aspHeader);

    for (auto& block : processBlocks) {
        block->draw(window);
    }

    // Center - Arbiter
    arbiterNode->draw(window);

    // Right panel - HIP Resource slots
    sf::Text hipHeader;
    hipHeader.setFont(mainFont);
    hipHeader.setString("HIP — HARDWARE INTERFACE");
    hipHeader.setCharacterSize(FONT_HEADER);
    hipHeader.setFillColor(NEON_BLUE);
    hipHeader.setStyle(sf::Text::Bold);
    hipHeader.setPosition(WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 20.f, 380.f);
    window.draw(hipHeader);

    for (auto& slot : hipSlots) {
        slot->draw(window);
    }

    // Draw controls
    for (auto& btn : buttons) {
        btn->draw(window);
    }
    for (auto& slider : sliders) {
        slider->draw(window);
    }

    // Draw metrics
    metricsDashboard->draw(window);
}

void Visualizer::renderHybridView() {
    // Combines both combat and scheduler views
    // Left: ASP process blocks (scheduler view)
    // Center: Character cards + Arbiter (combat view)
    // Right: HIP slots + Metrics + Controls

    // ── Left Panel: ASP Process Blocks ──
    sf::Text aspHeader;
    aspHeader.setFont(mainFont);
    aspHeader.setString("ASP PROCESSES");
    aspHeader.setCharacterSize(FONT_SUBTITLE);
    aspHeader.setFillColor(NEON_YELLOW);
    aspHeader.setStyle(sf::Text::Bold);
    aspHeader.setPosition(20.f, HEADER_HEIGHT + 10.f);
    window.draw(aspHeader);

    int visibleBlocks = std::min(5, static_cast<int>(processBlocks.size()));
    for (int i = 0; i < visibleBlocks; ++i) {
        processBlocks[i]->draw(window);
    }

    // ── Center: Character Cards ──
    pthread_mutex_lock(&sharedState->mtx);

    sf::Text combatHeader;
    combatHeader.setFont(mainFont);
    combatHeader.setString("COMBAT FIELD");
    combatHeader.setCharacterSize(FONT_SUBTITLE);
    combatHeader.setFillColor(NEON_CYAN);
    combatHeader.setStyle(sf::Text::Bold);
    combatHeader.setPosition(LEFT_PANEL_WIDTH + 20.f, HEADER_HEIGHT + 10.f);
    window.draw(combatHeader);

    pthread_mutex_unlock(&sharedState->mtx);

    for (int i = 0; i < sharedState->num_players && i < static_cast<int>(playerCards.size()); ++i) {
        playerCards[i]->draw(window);
    }
    for (int i = 0; i < sharedState->num_npcs && i < static_cast<int>(npcCards.size()); ++i) {
        npcCards[i]->draw(window);
    }

    // ── Artifacts ──
    for (auto& art : artifactDisplays) {
        art->draw(window);
    }

    // ── Center: Arbiter ──
    arbiterNode->draw(window);

    // ── Right Panel: HIP + Controls + Metrics ──
    sf::Text hipHeader;
    hipHeader.setFont(mainFont);
    hipHeader.setString("HIP RESOURCES");
    hipHeader.setCharacterSize(FONT_SUBTITLE);
    hipHeader.setFillColor(NEON_BLUE);
    hipHeader.setStyle(sf::Text::Bold);
    hipHeader.setPosition(WINDOW_WIDTH - RIGHT_PANEL_WIDTH + 20.f, HEADER_HEIGHT + 10.f);
    window.draw(hipHeader);

    for (auto& slot : hipSlots) {
        slot->draw(window);
    }

    // Controls
    for (auto& btn : buttons) {
        btn->draw(window);
    }
    for (auto& slider : sliders) {
        slider->draw(window);
    }

    // Metrics
    metricsDashboard->draw(window);
}

void Visualizer::renderFooter() {
    // Footer background
    sf::RectangleShape footer(sf::Vector2f(WINDOW_WIDTH, FOOTER_HEIGHT));
    footer.setPosition(0.f, WINDOW_HEIGHT - FOOTER_HEIGHT);
    footer.setFillColor(BACKGROUND_PANEL);
    footer.setOutlineColor(BORDER_COLOR);
    footer.setOutlineThickness(1.f);
    window.draw(footer);

    // Gantt chart
    ganttChart->draw(window);

    // Log panel
    logPanel->draw(window);

    // Controls hint
    sf::Text hintText;
    hintText.setFont(mainFont);
    hintText.setString("[1]Combat [2]Scheduler [3]Hybrid | Space:Spawn | ESC:Exit");
    hintText.setCharacterSize(FONT_TINY);
    hintText.setFillColor(TEXT_DIM);
    hintText.setPosition(WINDOW_WIDTH - 450.f, WINDOW_HEIGHT - 18.f);
    window.draw(hintText);
}

void Visualizer::renderConnections() {
    for (auto& line : connectionLines) {
        line.draw(window);
    }
}

void Visualizer::renderGameOver() {
    pthread_mutex_lock(&sharedState->mtx);
    bool won = (sharedState->win == 1);
    int turns = sharedState->turn_seq;
    int kills = sharedState->kills;
    pthread_mutex_unlock(&sharedState->mtx);

    // Dim overlay
    sf::RectangleShape overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(overlay);

    // Result box
    float boxW = 500.f;
    float boxH = 300.f;
    float boxX = (WINDOW_WIDTH - boxW) / 2.f;
    float boxY = (WINDOW_HEIGHT - boxH) / 2.f;

    sf::RectangleShape box(sf::Vector2f(boxW, boxH));
    box.setPosition(boxX, boxY);
    box.setFillColor(BACKGROUND_PANEL);
    box.setOutlineColor(won ? NEON_GREEN : NEON_RED);
    box.setOutlineThickness(3.f);
    window.draw(box);

    // Title
    sf::Text title;
    title.setFont(mainFont);
    title.setString(won ? "[ VICTORY ]" : "[ DEFEAT ]");
    title.setCharacterSize(36);
    title.setFillColor(won ? NEON_GREEN : NEON_RED);
    title.setStyle(sf::Text::Bold);
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition(boxX + (boxW - tb.width) / 2.f, boxY + 30.f);
    window.draw(title);

    // Stats
    sf::Text stats;
    stats.setFont(mainFont);
    stats.setCharacterSize(FONT_SUBTITLE);
    stats.setFillColor(TEXT_PRIMARY);

    std::ostringstream ss;
    ss << "Turns Played: " << turns << "\n";
    ss << "NPCs Killed: " << kills << " / 10\n";

    int survivors = 0;
    pthread_mutex_lock(&sharedState->mtx);
    for (int i = 0; i < sharedState->num_players; ++i) {
        if (sharedState->players[i].alive) survivors++;
    }
    pthread_mutex_unlock(&sharedState->mtx);

    ss << "Survivors: " << survivors << "\n";
    ss << "Throughput: " << std::fixed << std::setprecision(2) << cpuThroughput << " kills/s\n";
    ss << "Avg Turnaround: " << std::fixed << std::setprecision(2) << avgTurnaroundTime << "s";

    stats.setString(ss.str());
    stats.setPosition(boxX + 40.f, boxY + 100.f);
    window.draw(stats);

    // Prompt
    sf::Text prompt;
    prompt.setFont(mainFont);
    prompt.setString("[ Press any key to exit ]");
    prompt.setCharacterSize(FONT_BODY);
    prompt.setFillColor(NEON_CYAN);
    sf::FloatRect pb = prompt.getLocalBounds();
    prompt.setPosition(boxX + (boxW - pb.width) / 2.f, boxY + boxH - 50.f);
    window.draw(prompt);

    window.display();

    // Wait for key
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

// ═══════════════════════════════════════════════════════════════════════════
// ACTIONS
// ═══════════════════════════════════════════════════════════════════════════

void Visualizer::spawnNewProcess() {
    particles.emit(100.f, 200.f, NEON_GREEN, 20);
    totalProcessesSpawned++;
}

void Visualizer::switchMode(ViewMode mode) {
    currentMode = mode;
    particles.emit(WINDOW_WIDTH / 2.f, HEADER_HEIGHT + 50.f, NEON_CYAN, 15);
}

sf::Color Visualizer::getStateColor(int state) const {
    switch (state) {
        case 0: return STATE_RUNNING;
        case 1: return STATE_READY;
        case 2: return STATE_BLOCKED;
        default: return TEXT_DIM;
    }
}

std::string Visualizer::actionToString(ActionType action) const {
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
