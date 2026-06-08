#pragma once

#include "UITheme.hpp"
#include "../shared.hpp"
#include <SFML/Graphics.hpp>
#include <string>

namespace ChronoRift {

// ---
// UI COMPONENTS — Reusable SFML UI Elements
// ---

// ── Progress Bar ──────────────────────────────────────────────────────────
class ProgressBar {
public:
    ProgressBar(float x, float y, float width, float height,
                sf::Color fillColor, sf::Color bgColor = BACKGROUND_CARD);
    void setProgress(float percent); // 0.0 to 1.0
    void setColors(sf::Color fill, sf::Color bg);
    void draw(sf::RenderWindow& window);
    void setPosition(float x, float y);
    sf::FloatRect getBounds() const;

private:
    sf::RectangleShape bgRect;
    sf::RectangleShape fillRect;
    float maxWidth;
};

// ── Neon Button ───────────────────────────────────────────────────────────
class Button {
public:
    Button(float x, float y, float width, float height, const std::string& text,
           sf::Font& font, unsigned fontSize = FONT_BODY);
    void draw(sf::RenderWindow& window);
    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void setColors(sf::Color normal, sf::Color hover, sf::Color text);
    void setCallback(void (*cb)(void*), void* ctx);
    void setCallbackWithTag(void (*cb)(void*, int), void* ctx, int tag);
    void setText(const std::string& text);
    bool isHovered() const { return hovered; }
    int getTag() const { return tag; }

private:
    sf::RectangleShape shape;
    sf::Text label;
    sf::Color normalColor;
    sf::Color hoverColor;
    sf::Color textColor;
    bool hovered;
    void (*callback)(void*);
    void (*callbackWithTag)(void*, int);
    void* callbackCtx;
    int tag;
};

// ── Slider Control ────────────────────────────────────────────────────────
class Slider {
public:
    Slider(float x, float y, float width, float minVal, float maxVal,
           float initialVal, const std::string& label, sf::Font& font);
    void draw(sf::RenderWindow& window);
    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    float getValue() const;
    void setValue(float val);

private:
    sf::RectangleShape track;
    sf::CircleShape thumb;
    sf::Text labelText;
    sf::Text valueText;
    float minValue;
    float maxValue;
    float currentValue;
    float trackX, trackY, trackWidth;
    bool dragging;
    void updateThumbPosition();
};

// ── Character Card ────────────────────────────────────────────────────────
class CharacterCard {
public:
    CharacterCard(float x, float y, float width, float height,
                  sf::Font& font, bool isPlayer);
    void update(const CharacterState& state, bool isActive, int currentEpoch);
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;

private:
    sf::RectangleShape cardBg;
    sf::RectangleShape glowBorder;
    sf::Text nameText;
    sf::Text hpText;
    sf::Text stText;
    sf::Text dmgText;
    sf::Text statusText;
    ProgressBar hpBar;
    ProgressBar stBar;
    bool isPlayerType;
    bool active;
    void drawInventory(sf::RenderWindow& window, float x, float y, const CharacterState& state);
};

// ── Artifact Display ──────────────────────────────────────────────────────
class ArtifactDisplay {
public:
    ArtifactDisplay(float x, float y, float width, sf::Font& font);
    void update(const ArtifactState& state, int index, bool eclipsePresent);
    void draw(sf::RenderWindow& window);

private:
    sf::RectangleShape bg;
    sf::Text nameText;
    sf::Text statusText;
    sf::Text waitingText;
    sf::CircleShape icon;
    int artifactIndex;
};

// ── Log Panel ─────────────────────────────────────────────────────────────
static constexpr int MAX_LOG_ENTRIES = 20;

class LogPanel {
public:
    LogPanel(float x, float y, float width, float height, sf::Font& font);
    void update(const SharedState* state);
    void draw(sf::RenderWindow& window);

private:
    sf::RectangleShape bg;
    sf::Text headerText;
    sf::Text logEntries[MAX_LOG_ENTRIES];
    sf::Font& font;
    int logCount;
    float x, y, width, height;
};

// ── Metrics Dashboard ─────────────────────────────────────────────────────
class MetricsDashboard {
public:
    MetricsDashboard(float x, float y, float width, sf::Font& font);
    void update(float avgWait, float avgTurnaround, float throughput,
                int turns, int kills, int totalProcesses);
    void draw(sf::RenderWindow& window);

private:
    sf::RectangleShape bg;
    sf::Text headerText;
    sf::Text waitText;
    sf::Text turnaroundText;
    sf::Text throughputText;
    sf::Text turnsText;
    sf::Text killsText;
};

// ── Gantt Chart (using sf::VertexArray) ───────────────────────────────────
static constexpr int MAX_GANTT_ENTRIES = 256;

class GanttChart {
public:
    GanttChart(float x, float y, float width, float height, sf::Font& font);
    void addEntry(int entityId, TeamType team, float startTime, float duration);
    void update(float currentTime);
    void clear();
    void draw(sf::RenderWindow& window);

private:
    struct GanttEntry {
        int entityId;
        TeamType team;
        float startTime;
        float duration;
    };

    float x, y, width, height;
    sf::Font& font;
    sf::Text headerText;
    sf::Text timeText;
    GanttEntry entries[MAX_GANTT_ENTRIES];
    int entryCount;
    float currentTime;
    float timeWindow;

    void drawGrid(sf::RenderWindow& window);
    void drawEntries(sf::RenderWindow& window);
};

// ── Arbiter Node Visualization ────────────────────────────────────────────
class ArbiterNode {
public:
    ArbiterNode(float x, float y, float radius, sf::Font& font);
    void update(bool isPolling, int activeTeam, int activeId);
    void draw(sf::RenderWindow& window);
    sf::Vector2f getPosition() const;
    float getRadius() const { return radius; }

private:
    float x, y, radius;
    sf::CircleShape outerRing;
    sf::CircleShape innerCircle;
    sf::Text labelText;
    sf::Text statusText;
    float pulsePhase;
    bool polling;
    int activeTeam;
    int activeId;
};

// ── Process Block (ASP visualization) ─────────────────────────────────────
class ProcessBlock {
public:
    ProcessBlock(float x, float y, float width, float height,
                 int pid, sf::Font& font);
    void update(float progress, int state, const std::string& name);
    void draw(sf::RenderWindow& window);
    void setPosition(float x, float y);
    sf::FloatRect getBounds() const;
    int getPid() const { return processId; }

private:
    int processId;
    sf::RectangleShape bg;
    sf::RectangleShape glow;
    ProgressBar progressBar;
    sf::Text nameText;
    sf::Text pidText;
    sf::Text stateText;
    int currentState;
};

// ── HIP Resource Slot ─────────────────────────────────────────────────────
class HIPSlot {
public:
    HIPSlot(float x, float y, float width, float height,
            int slotId, const std::string& type, sf::Font& font);
    void setOccupied(bool occ, int processId = -1, const std::string& processName = "");
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;
    int getSlotId() const { return slotId; }
    bool isOccupied() const { return occupied; }

private:
    int slotId;
    std::string resourceType;
    sf::RectangleShape bg;
    sf::RectangleShape glow;
    sf::Text idText;
    sf::Text typeText;
    sf::Text occupantText;
    bool occupied;
};

// ── Particle System for Visual Effects ────────────────────────────────────
static constexpr int MAX_PARTICLES = 512;

class ParticleSystem {
public:
    ParticleSystem();
    void emit(float x, float y, sf::Color color, int count = 10);
    void update(float dt);
    void draw(sf::RenderWindow& window);

private:
    struct Particle {
        sf::CircleShape shape;
        sf::Vector2f velocity;
        float lifetime;
        float maxLifetime;
    };
    Particle particles[MAX_PARTICLES];
    int particleCount;
};

// ── Connection Line (for showing Arbiter polling) ─────────────────────────
class ConnectionLine {
public:
    ConnectionLine();
    void setPoints(sf::Vector2f from, sf::Vector2f to);
    void setActive(bool active);
    void setColor(sf::Color color);
    void draw(sf::RenderWindow& window);

private:
    sf::VertexArray line;
    sf::Color color;
    bool isActive;
};

} // namespace ChronoRift
