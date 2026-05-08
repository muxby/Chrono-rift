#include "UIComponents.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
// PROGRESS BAR
// ═══════════════════════════════════════════════════════════════════════════

ProgressBar::ProgressBar(float x, float y, float w, float h,
                         sf::Color fillColor, sf::Color bgColor)
    : maxWidth(w) {
    bgRect.setPosition(x, y);
    bgRect.setSize(sf::Vector2f(w, h));
    bgRect.setFillColor(bgColor);
    bgRect.setOutlineColor(BORDER_COLOR);
    bgRect.setOutlineThickness(1.f);

    fillRect.setPosition(x, y);
    fillRect.setSize(sf::Vector2f(0.f, h));
    fillRect.setFillColor(fillColor);
}

void ProgressBar::setProgress(float percent) {
    percent = std::max(0.f, std::min(1.f, percent));
    fillRect.setSize(sf::Vector2f(maxWidth * percent, fillRect.getSize().y));
}

void ProgressBar::setColors(sf::Color fill, sf::Color bg) {
    fillRect.setFillColor(fill);
    bgRect.setFillColor(bg);
}

void ProgressBar::draw(sf::RenderWindow& window) {
    window.draw(bgRect);
    window.draw(fillRect);
}

void ProgressBar::setPosition(float x, float y) {
    bgRect.setPosition(x, y);
    fillRect.setPosition(x, y);
}

sf::FloatRect ProgressBar::getBounds() const {
    return bgRect.getGlobalBounds();
}

// ═══════════════════════════════════════════════════════════════════════════
// NEON BUTTON
// ═══════════════════════════════════════════════════════════════════════════

Button::Button(float x, float y, float w, float h, const std::string& text,
               sf::Font& font, unsigned fontSize)
    : normalColor(NEON_BLUE), hoverColor(NEON_CYAN), textColor(sf::Color::White),
      hovered(false), callback(nullptr) {
    shape.setPosition(x, y);
    shape.setSize(sf::Vector2f(w, h));
    shape.setFillColor(normalColor);
    shape.setOutlineColor(NEON_CYAN);
    shape.setOutlineThickness(1.f);

    label.setFont(font);
    label.setString(text);
    label.setCharacterSize(fontSize);
    label.setFillColor(textColor);
    label.setStyle(sf::Text::Bold);

    // Center text
    sf::FloatRect textBounds = label.getLocalBounds();
    label.setPosition(
        x + (w - textBounds.width) / 2.f,
        y + (h - textBounds.height) / 2.f - 2.f
    );
}

void Button::draw(sf::RenderWindow& window) {
    shape.setFillColor(hovered ? hoverColor : normalColor);
    shape.setOutlineThickness(hovered ? 2.f : 1.f);
    window.draw(shape);
    window.draw(label);
}

bool Button::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool wasHovered = hovered;
    hovered = shape.getGlobalBounds().contains(mousePos);

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        if (hovered && callback) {
            callback();
            return true;
        }
    }
    return false;
}

void Button::setColors(sf::Color normal, sf::Color hover, sf::Color text) {
    normalColor = normal;
    hoverColor = hover;
    textColor = text;
    label.setFillColor(text);
}

void Button::setCallback(std::function<void()> cb) {
    callback = cb;
}

void Button::setText(const std::string& text) {
    label.setString(text);
    sf::FloatRect bounds = shape.getGlobalBounds();
    sf::FloatRect textBounds = label.getLocalBounds();
    label.setPosition(
        bounds.left + (bounds.width - textBounds.width) / 2.f,
        bounds.top + (bounds.height - textBounds.height) / 2.f - 2.f
    );
}

// ═══════════════════════════════════════════════════════════════════════════
// SLIDER CONTROL
// ═══════════════════════════════════════════════════════════════════════════

Slider::Slider(float x, float y, float w, float minVal, float maxVal,
               float initialVal, const std::string& label, sf::Font& font)
    : minValue(minVal), maxValue(maxVal), currentValue(initialVal),
      trackX(x), trackY(y), trackWidth(w), dragging(false) {
    track.setPosition(x, y);
    track.setSize(sf::Vector2f(w, 6.f));
    track.setFillColor(BACKGROUND_CARD);
    track.setOutlineColor(BORDER_COLOR);
    track.setOutlineThickness(1.f);

    thumb.setRadius(10.f);
    thumb.setFillColor(NEON_CYAN);
    thumb.setOutlineColor(NEON_WHITE);
    thumb.setOutlineThickness(1.f);
    thumb.setOrigin(10.f, 10.f);

    labelText.setFont(font);
    labelText.setString(label);
    labelText.setCharacterSize(FONT_SMALL);
    labelText.setFillColor(TEXT_SECONDARY);
    labelText.setPosition(x, y - 22.f);

    valueText.setFont(font);
    valueText.setCharacterSize(FONT_SMALL);
    valueText.setFillColor(TEXT_ACCENT);
    updateThumbPosition();
}

void Slider::updateThumbPosition() {
    float t = (currentValue - minValue) / (maxValue - minValue);
    float thumbX = trackX + t * trackWidth;
    thumb.setPosition(thumbX, trackY + 3.f);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << currentValue;
    valueText.setString(ss.str());
    valueText.setPosition(trackX + trackWidth + 10.f, trackY - 5.f);
}

void Slider::draw(sf::RenderWindow& window) {
    window.draw(track);
    window.draw(thumb);
    window.draw(labelText);
    window.draw(valueText);
}

bool Slider::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        if (thumb.getGlobalBounds().contains(mousePos) ||
            track.getGlobalBounds().contains(mousePos)) {
            dragging = true;
        }
    }
    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
        dragging = false;
    }
    if (event.type == sf::Event::MouseMoved && dragging) {
        float t = std::max(0.f, std::min(1.f, (mousePos.x - trackX) / trackWidth));
        currentValue = minValue + t * (maxValue - minValue);
        updateThumbPosition();
        return true;
    }
    return false;
}

float Slider::getValue() const {
    return currentValue;
}

void Slider::setValue(float val) {
    currentValue = std::max(minValue, std::min(maxValue, val));
    updateThumbPosition();
}

// ═══════════════════════════════════════════════════════════════════════════
// CHARACTER CARD
// ═══════════════════════════════════════════════════════════════════════════

CharacterCard::CharacterCard(float x, float y, float w, float h,
                             sf::Font& font, bool isPlayer)
    : hpBar(x + 70.f, y + 28.f, w - 80.f, 12.f, HP_BAR_HIGH),
      stBar(x + 70.f, y + 48.f, w - 80.f, 8.f, STAMINA_BAR),
      isPlayerType(isPlayer), active(false) {
    cardBg.setPosition(x, y);
    cardBg.setSize(sf::Vector2f(w, h));
    cardBg.setFillColor(BACKGROUND_CARD);
    cardBg.setOutlineColor(BORDER_COLOR);
    cardBg.setOutlineThickness(1.f);

    glowBorder.setPosition(x - 2.f, y - 2.f);
    glowBorder.setSize(sf::Vector2f(w + 4.f, h + 4.f));
    glowBorder.setFillColor(sf::Color::Transparent);
    glowBorder.setOutlineColor(sf::Color::Transparent);
    glowBorder.setOutlineThickness(2.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_BODY);
    nameText.setFillColor(isPlayer ? STATE_RUNNING : STATE_BLOCKED);
    nameText.setPosition(x + 8.f, y + 4.f);

    hpText.setFont(font);
    hpText.setCharacterSize(FONT_TINY);
    hpText.setFillColor(TEXT_SECONDARY);
    hpText.setPosition(x + 8.f, y + 26.f);

    stText.setFont(font);
    stText.setCharacterSize(FONT_TINY);
    stText.setFillColor(TEXT_SECONDARY);
    stText.setPosition(x + 8.f, y + 46.f);

    dmgText.setFont(font);
    dmgText.setCharacterSize(FONT_TINY);
    dmgText.setFillColor(TEXT_DIM);
    dmgText.setPosition(x + 8.f, y + 64.f);

    statusText.setFont(font);
    statusText.setCharacterSize(FONT_TINY);
    statusText.setFillColor(NEON_YELLOW);
    statusText.setPosition(x + w - 70.f, y + 64.f);
}

void CharacterCard::update(const CharacterState& state, bool isActive, int currentEpoch) {
    active = isActive;
    std::string prefix = isPlayerType ? "P" : "N";
    nameText.setString(prefix + std::to_string(state.id));

    if (!state.alive) {
        nameText.setFillColor(TEXT_DIM);
        hpText.setString("DEAD");
        hpBar.setProgress(0.f);
        stBar.setProgress(0.f);
        dmgText.setString("DMG: " + std::to_string(state.dmg));
        statusText.setString("");
        cardBg.setFillColor(sf::Color(25, 25, 30, 255));
        return;
    }

    float hpPct = static_cast<float>(state.hp) / std::max(1, state.max_hp);
    float stPct = static_cast<float>(state.stamina) / std::max(1, state.max_stamina);

    hpBar.setProgress(hpPct);
    stBar.setProgress(stPct);

    // Color-code HP bar
    if (hpPct > 0.5f) hpBar.setColors(HP_BAR_HIGH, BACKGROUND_CARD);
    else if (hpPct > 0.2f) hpBar.setColors(HP_BAR_MED, BACKGROUND_CARD);
    else hpBar.setColors(HP_BAR_LOW, BACKGROUND_CARD);

    hpText.setString("HP: " + std::to_string(state.hp) + "/" + std::to_string(state.max_hp));
    stText.setString("ST: " + std::to_string(state.stamina) + "/" + std::to_string(state.max_stamina));
    dmgText.setString("DMG: " + std::to_string(state.dmg) + " SPD: " + std::to_string(state.speed));

    // Status
    if (state.stunned_until_epoch > currentEpoch) {
        int stunLeft = state.stunned_until_epoch - currentEpoch;
        statusText.setString("[STUN " + std::to_string(stunLeft) + "s]");
    } else {
        statusText.setString(isActive ? "<< ACTIVE" : "");
    }

    // Active glow
    if (isActive) {
        glowBorder.setOutlineColor(NEON_YELLOW);
        cardBg.setFillColor(sf::Color(35, 40, 55, 255));
    } else {
        glowBorder.setOutlineColor(sf::Color::Transparent);
        cardBg.setFillColor(BACKGROUND_CARD);
    }

    nameText.setFillColor(isPlayerType ? NEON_GREEN : NEON_RED);
}

void CharacterCard::draw(sf::RenderWindow& window) {
    window.draw(cardBg);
    if (active) window.draw(glowBorder);
    window.draw(nameText);
    window.draw(hpText);
    window.draw(stText);
    hpBar.draw(window);
    stBar.draw(window);
    window.draw(dmgText);
    window.draw(statusText);
}

sf::FloatRect CharacterCard::getBounds() const {
    return cardBg.getGlobalBounds();
}

// ═══════════════════════════════════════════════════════════════════════════
// ARTIFACT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════

ArtifactDisplay::ArtifactDisplay(float x, float y, float w, sf::Font& font)
    : artifactIndex(0) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, 50.f));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    icon.setRadius(12.f);
    icon.setPosition(x + 8.f, y + 13.f);
    icon.setFillColor(NEON_PURPLE);
    icon.setOutlineColor(NEON_PINK);
    icon.setOutlineThickness(1.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_BODY);
    nameText.setFillColor(NEON_PURPLE);
    nameText.setPosition(x + 35.f, y + 4.f);

    statusText.setFont(font);
    statusText.setCharacterSize(FONT_SMALL);
    statusText.setFillColor(TEXT_SECONDARY);
    statusText.setPosition(x + 35.f, y + 26.f);

    waitingText.setFont(font);
    waitingText.setCharacterSize(FONT_TINY);
    waitingText.setFillColor(NEON_YELLOW);
    waitingText.setPosition(x + w - 80.f, y + 30.f);
}

void ArtifactDisplay::update(const ArtifactState& state, int index, bool eclipsePresent) {
    artifactIndex = index;
    const char* names[] = {"Solar Core", "Lunar Blade", "Eclipse Relic"};
    nameText.setString(names[index]);

    if (index == 2 && !eclipsePresent) {
        statusText.setString("[Hidden]");
        statusText.setFillColor(TEXT_DIM);
        icon.setFillColor(sf::Color(40, 40, 50, 255));
    } else if (state.holder_team == -1) {
        statusText.setString("[Free]");
        statusText.setFillColor(NEON_YELLOW);
        icon.setFillColor(NEON_PURPLE);
    } else {
        std::string team = (state.holder_team == TEAM_PLAYER) ? "P" : "N";
        statusText.setString(team + std::to_string(state.holder_id));
        statusText.setFillColor(state.holder_team == TEAM_PLAYER ? NEON_GREEN : NEON_RED);
        icon.setFillColor(state.holder_team == TEAM_PLAYER ? NEON_GREEN : NEON_RED);
    }

    if (state.waiting_count > 0) {
        waitingText.setString(std::to_string(state.waiting_count) + " waiting");
    } else {
        waitingText.setString("");
    }
}

void ArtifactDisplay::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(icon);
    window.draw(nameText);
    window.draw(statusText);
    window.draw(waitingText);
}

// ═══════════════════════════════════════════════════════════════════════════
// METRICS DASHBOARD
// ═══════════════════════════════════════════════════════════════════════════

MetricsDashboard::MetricsDashboard(float x, float y, float w, sf::Font& font) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, 140.f));
    bg.setFillColor(BACKGROUND_PANEL);
    bg.setOutlineColor(NEON_CYAN);
    bg.setOutlineThickness(1.f);

    headerText.setFont(font);
    headerText.setString("SYSTEM METRICS");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(NEON_CYAN);
    headerText.setStyle(sf::Text::Bold);
    headerText.setPosition(x + 10.f, y + 8.f);

    waitText.setFont(font);
    waitText.setCharacterSize(FONT_BODY);
    waitText.setFillColor(TEXT_PRIMARY);
    waitText.setPosition(x + 10.f, y + 38.f);

    turnaroundText.setFont(font);
    turnaroundText.setCharacterSize(FONT_BODY);
    turnaroundText.setFillColor(TEXT_PRIMARY);
    turnaroundText.setPosition(x + 10.f, y + 60.f);

    throughputText.setFont(font);
    throughputText.setCharacterSize(FONT_BODY);
    throughputText.setFillColor(TEXT_PRIMARY);
    throughputText.setPosition(x + 10.f, y + 82.f);

    turnsText.setFont(font);
    turnsText.setCharacterSize(FONT_BODY);
    turnsText.setFillColor(TEXT_PRIMARY);
    turnsText.setPosition(x + 10.f, y + 104.f);

    killsText.setFont(font);
    killsText.setCharacterSize(FONT_BODY);
    killsText.setFillColor(NEON_GREEN);
    killsText.setPosition(x + w / 2.f, y + 104.f);
}

void MetricsDashboard::update(float avgWait, float avgTurnaround, float throughput,
                               int turns, int kills, int totalProcesses) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "Avg Wait Time: " << avgWait << "s";
    waitText.setString(ss.str());
    ss.str("");

    ss << "Avg Turnaround: " << avgTurnaround << "s";
    turnaroundText.setString(ss.str());
    ss.str("");

    ss << "Throughput: " << throughput << " proc/s";
    throughputText.setString(ss.str());
    ss.str("");

    turnsText.setString("Turns: " + std::to_string(turns));
    killsText.setString("Kills: " + std::to_string(kills) + "/10");
}

void MetricsDashboard::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(headerText);
    window.draw(waitText);
    window.draw(turnaroundText);
    window.draw(throughputText);
    window.draw(turnsText);
    window.draw(killsText);
}

// ═══════════════════════════════════════════════════════════════════════════
// GANTT CHART (using sf::VertexArray)
// ═══════════════════════════════════════════════════════════════════════════

GanttChart::GanttChart(float x, float y, float w, float h, sf::Font& font)
    : x(x), y(y), width(w), height(h), font(font), currentTime(0.f), timeWindow(30.f) {
    headerText.setFont(font);
    headerText.setString("GANTT CHART — Process Scheduling Timeline");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(NEON_CYAN);
    headerText.setStyle(sf::Text::Bold);
    headerText.setPosition(x, y - 30.f);

    timeText.setFont(font);
    timeText.setCharacterSize(FONT_SMALL);
    timeText.setFillColor(TEXT_SECONDARY);
    timeText.setPosition(x + width - 100.f, y - 30.f);
}

void GanttChart::addEntry(int entityId, TeamType team, float startTime, float duration) {
    entries.push_back({entityId, team, startTime, duration});
}

void GanttChart::update(float time) {
    currentTime = time;

    // Remove old entries outside the time window
    auto it = entries.begin();
    while (it != entries.end()) {
        if (it->startTime + it->duration < currentTime - timeWindow) {
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
}

void GanttChart::clear() {
    entries.clear();
}

void GanttChart::drawGrid(sf::RenderWindow& window) {
    sf::VertexArray grid(sf::Lines);

    // Background
    sf::RectangleShape bg(sf::Vector2f(width, height));
    bg.setPosition(x, y);
    bg.setFillColor(GANTT_BG);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    // Vertical time grid lines
    int numCols = 10;
    for (int i = 0; i <= numCols; ++i) {
        float gx = x + (width / numCols) * i;
        grid.append(sf::Vertex(sf::Vector2f(gx, y), GANTT_GRID));
        grid.append(sf::Vertex(sf::Vector2f(gx, y + height), GANTT_GRID));
    }

    // Horizontal lane dividers
    int maxEntities = std::max(MAX_PLAYERS, MAX_NPCS);
    float laneHeight = height / (maxEntities + 1);
    for (int i = 0; i <= maxEntities + 1; ++i) {
        float gy = y + laneHeight * i;
        grid.append(sf::Vertex(sf::Vector2f(x, gy), GANTT_GRID));
        grid.append(sf::Vertex(sf::Vector2f(x + width, gy), GANTT_GRID));
    }

    window.draw(grid);

    // Time labels
    for (int i = 0; i <= numCols; ++i) {
        float t = currentTime - timeWindow + (timeWindow / numCols) * i;
        sf::Text lbl;
        lbl.setFont(font);
        lbl.setCharacterSize(FONT_TINY);
        lbl.setFillColor(TEXT_DIM);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0) << t << "s";
        lbl.setString(ss.str());
        lbl.setPosition(x + (width / numCols) * i + 2.f, y + height + 2.f);
        window.draw(lbl);
    }
}

void GanttChart::drawEntries(sf::RenderWindow& window) {
    float laneHeight = height / (MAX_NPCS + 1);

    for (const auto& entry : entries) {
        float relStart = entry.startTime - (currentTime - timeWindow);
        float px = x + (relStart / timeWindow) * width;
        float pw = (entry.duration / timeWindow) * width;
        float py = y + entry.entityId * laneHeight + 2.f;
        float ph = laneHeight - 4.f;

        if (px + pw < x || px > x + width) continue;

        px = std::max(x, px);
        pw = std::min(pw, (x + width) - px);

        sf::RectangleShape bar(sf::Vector2f(pw, ph));
        bar.setPosition(px, py);
        bar.setFillColor(entry.team == TEAM_PLAYER ? GANTT_PLAYER : GANTT_NPC);
        bar.setOutlineColor(entry.team == TEAM_PLAYER ? NEON_GREEN : NEON_RED);
        bar.setOutlineThickness(0.5f);
        window.draw(bar);

        // Entity label
        if (pw > 20.f) {
            sf::Text lbl;
            lbl.setFont(font);
            lbl.setCharacterSize(FONT_TINY);
            lbl.setFillColor(sf::Color::White);
            std::string prefix = entry.team == TEAM_PLAYER ? "P" : "N";
            lbl.setString(prefix + std::to_string(entry.entityId));
            lbl.setPosition(px + 3.f, py + 1.f);
            window.draw(lbl);
        }
    }
}

void GanttChart::draw(sf::RenderWindow& window) {
    std::ostringstream ss;
    ss << "T: " << std::fixed << std::setprecision(1) << currentTime << "s";
    timeText.setString(ss.str());

    window.draw(headerText);
    window.draw(timeText);
    drawGrid(window);
    drawEntries(window);
}

// ═══════════════════════════════════════════════════════════════════════════
// ARBITER NODE VISUALIZATION
// ═══════════════════════════════════════════════════════════════════════════

ArbiterNode::ArbiterNode(float x, float y, float r, sf::Font& font)
    : x(x), y(y), radius(r), pulsePhase(0.f), polling(false), activeTeam(-1), activeId(-1) {
    outerRing.setRadius(r + 8.f);
    outerRing.setOrigin(r + 8.f, r + 8.f);
    outerRing.setPosition(x, y);
    outerRing.setFillColor(sf::Color::Transparent);
    outerRing.setOutlineColor(NEON_CYAN);
    outerRing.setOutlineThickness(2.f);

    innerCircle.setRadius(r);
    innerCircle.setOrigin(r, r);
    innerCircle.setPosition(x, y);
    innerCircle.setFillColor(BACKGROUND_PANEL);
    innerCircle.setOutlineColor(NEON_BLUE);
    innerCircle.setOutlineThickness(2.f);

    labelText.setFont(font);
    labelText.setString("ARBITER");
    labelText.setCharacterSize(FONT_SMALL);
    labelText.setFillColor(NEON_CYAN);
    labelText.setStyle(sf::Text::Bold);
    sf::FloatRect tb = labelText.getLocalBounds();
    labelText.setOrigin(tb.width / 2.f, tb.height / 2.f);
    labelText.setPosition(x, y - 8.f);

    statusText.setFont(font);
    statusText.setCharacterSize(FONT_TINY);
    statusText.setFillColor(TEXT_SECONDARY);
    statusText.setPosition(x - 25.f, y + 8.f);
}

void ArbiterNode::update(bool isPolling, int team, int id) {
    polling = isPolling;
    activeTeam = team;
    activeId = id;
    pulsePhase += 0.05f;

    if (polling) {
        float pulse = 0.5f + 0.5f * std::sin(pulsePhase * 3.f);
        sf::Uint8 alpha = static_cast<sf::Uint8>(100 + 155 * pulse);
        outerRing.setOutlineColor(sf::Color(0, 255, 255, alpha));
        outerRing.setOutlineThickness(2.f + pulse * 2.f);
    } else {
        outerRing.setOutlineColor(sf::Color(0, 200, 255, 100));
        outerRing.setOutlineThickness(2.f);
    }

    if (activeTeam == TEAM_PLAYER) {
        statusText.setString("P" + std::to_string(activeId));
        statusText.setFillColor(NEON_GREEN);
        innerCircle.setOutlineColor(NEON_GREEN);
    } else if (activeTeam == TEAM_NPC) {
        statusText.setString("N" + std::to_string(activeId));
        statusText.setFillColor(NEON_RED);
        innerCircle.setOutlineColor(NEON_RED);
    } else {
        statusText.setString("IDLE");
        statusText.setFillColor(TEXT_DIM);
        innerCircle.setOutlineColor(NEON_BLUE);
    }
}

void ArbiterNode::draw(sf::RenderWindow& window) {
    window.draw(outerRing);
    window.draw(innerCircle);
    window.draw(labelText);
    window.draw(statusText);
}

sf::Vector2f ArbiterNode::getPosition() const {
    return sf::Vector2f(x, y);
}

// ═══════════════════════════════════════════════════════════════════════════
// PROCESS BLOCK (ASP visualization)
// ═══════════════════════════════════════════════════════════════════════════

ProcessBlock::ProcessBlock(float x, float y, float w, float h,
                           int pid, sf::Font& font)
    : processId(pid),
      progressBar(x + 5.f, y + h - 18.f, w - 10.f, 10.f, PROGRESS_BAR),
      currentState(1) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    glow.setPosition(x - 2.f, y - 2.f);
    glow.setSize(sf::Vector2f(w + 4.f, h + 4.f));
    glow.setFillColor(sf::Color::Transparent);
    glow.setOutlineColor(sf::Color::Transparent);
    glow.setOutlineThickness(2.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_SMALL);
    nameText.setFillColor(TEXT_PRIMARY);
    nameText.setPosition(x + 8.f, y + 4.f);

    pidText.setFont(font);
    pidText.setCharacterSize(FONT_TINY);
    pidText.setFillColor(TEXT_DIM);
    pidText.setPosition(x + w - 35.f, y + 4.f);
    pidText.setString("PID:" + std::to_string(pid));

    stateText.setFont(font);
    stateText.setCharacterSize(FONT_TINY);
    stateText.setFillColor(STATE_READY);
    stateText.setPosition(x + 8.f, y + 22.f);
}

void ProcessBlock::update(float progress, int state, const std::string& name) {
    currentState = state;
    progressBar.setProgress(progress);
    nameText.setString(name);

    sf::Color stateColor;
    std::string stateStr;
    switch (state) {
        case 0: stateColor = STATE_RUNNING; stateStr = "RUNNING"; break;
        case 1: stateColor = STATE_READY; stateStr = "READY"; break;
        case 2: stateColor = STATE_BLOCKED; stateStr = "BLOCKED"; break;
        default: stateColor = TEXT_DIM; stateStr = "UNKNOWN"; break;
    }

    stateText.setString(stateStr);
    stateText.setFillColor(stateColor);
    bg.setOutlineColor(stateColor);

    if (state == 0) {
        glow.setOutlineColor(sf::Color(stateColor.r, stateColor.g, stateColor.b, 100));
    } else {
        glow.setOutlineColor(sf::Color::Transparent);
    }
}

void ProcessBlock::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(glow);
    window.draw(nameText);
    window.draw(pidText);
    window.draw(stateText);
    progressBar.draw(window);
}

void ProcessBlock::setPosition(float x, float y) {
    bg.setPosition(x, y);
    glow.setPosition(x - 2.f, y - 2.f);
    nameText.setPosition(x + 8.f, y + 4.f);
    pidText.setPosition(x + bg.getSize().x - 35.f, y + 4.f);
    stateText.setPosition(x + 8.f, y + 22.f);
    progressBar.setPosition(x + 5.f, y + bg.getSize().y - 18.f);
}

sf::FloatRect ProcessBlock::getBounds() const {
    return bg.getGlobalBounds();
}

// ═══════════════════════════════════════════════════════════════════════════
// HIP RESOURCE SLOT
// ═══════════════════════════════════════════════════════════════════════════

HIPSlot::HIPSlot(float x, float y, float w, float h,
                 int sid, const std::string& type, sf::Font& font)
    : slotId(sid), resourceType(type), occupied(false) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    glow.setPosition(x - 2.f, y - 2.f);
    glow.setSize(sf::Vector2f(w + 4.f, h + 4.f));
    glow.setFillColor(sf::Color::Transparent);
    glow.setOutlineColor(sf::Color::Transparent);
    glow.setOutlineThickness(2.f);

    idText.setFont(font);
    idText.setCharacterSize(FONT_SMALL);
    idText.setFillColor(TEXT_SECONDARY);
    idText.setPosition(x + 8.f, y + 4.f);
    idText.setString("CPU " + std::to_string(sid));

    typeText.setFont(font);
    typeText.setCharacterSize(FONT_TINY);
    typeText.setFillColor(TEXT_DIM);
    typeText.setPosition(x + 8.f, y + 22.f);
    typeText.setString(type);

    occupantText.setFont(font);
    occupantText.setCharacterSize(FONT_TINY);
    occupantText.setFillColor(NEON_GREEN);
    occupantText.setPosition(x + 8.f, y + 38.f);
    occupantText.setString("[ IDLE ]");
}

void HIPSlot::setOccupied(bool occ, int processId, const std::string& processName) {
    occupied = occ;
    if (occupied) {
        occupantText.setString("P" + std::to_string(processId) + ": " + processName);
        occupantText.setFillColor(NEON_GREEN);
        bg.setOutlineColor(NEON_GREEN);
        glow.setOutlineColor(sf::Color(57, 255, 20, 80));
    } else {
        occupantText.setString("[ IDLE ]");
        occupantText.setFillColor(TEXT_DIM);
        bg.setOutlineColor(BORDER_COLOR);
        glow.setOutlineColor(sf::Color::Transparent);
    }
}

void HIPSlot::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(glow);
    window.draw(idText);
    window.draw(typeText);
    window.draw(occupantText);
}

sf::FloatRect HIPSlot::getBounds() const {
    return bg.getGlobalBounds();
}

// ═══════════════════════════════════════════════════════════════════════════
// PARTICLE SYSTEM
// ═══════════════════════════════════════════════════════════════════════════

ParticleSystem::ParticleSystem() {}

void ParticleSystem::emit(float x, float y, sf::Color color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.shape.setRadius(2.f + static_cast<float>(rand() % 3));
        p.shape.setFillColor(color);
        p.shape.setPosition(x, y);
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
        float speed = 20.f + static_cast<float>(rand() % 80);
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        p.lifetime = 0.5f + static_cast<float>(rand() % 100) / 200.f;
        p.maxLifetime = p.lifetime;
        particles.push_back(p);
    }
}

void ParticleSystem::update(float dt) {
    auto it = particles.begin();
    while (it != particles.end()) {
        it->shape.move(it->velocity * dt);
        it->velocity *= 0.98f; // friction
        it->lifetime -= dt;
        float alpha = it->lifetime / it->maxLifetime;
        sf::Color c = it->shape.getFillColor();
        c.a = static_cast<sf::Uint8>(255 * alpha);
        it->shape.setFillColor(c);
        if (it->lifetime <= 0) {
            it = particles.erase(it);
        } else {
            ++it;
        }
    }
}

void ParticleSystem::draw(sf::RenderWindow& window) {
    for (auto& p : particles) {
        window.draw(p.shape);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONNECTION LINE
// ═══════════════════════════════════════════════════════════════════════════

ConnectionLine::ConnectionLine() : isActive(false) {
    line.setPrimitiveType(sf::Lines);
    line.resize(2);
    color = sf::Color(0, 255, 255, 80);
}

void ConnectionLine::setPoints(sf::Vector2f from, sf::Vector2f to) {
    line[0].position = from;
    line[1].position = to;
    line[0].color = color;
    line[1].color = color;
}

void ConnectionLine::setActive(bool active) {
    isActive = active;
    color = active ? sf::Color(0, 255, 255, 180) : sf::Color(0, 255, 255, 40);
    line[0].color = color;
    line[1].color = color;
}

void ConnectionLine::setColor(sf::Color c) {
    color = c;
    line[0].color = color;
    line[1].color = color;
}

void ConnectionLine::draw(sf::RenderWindow& window) {
    if (isActive) {
        window.draw(line);
    }
}

} // namespace ChronoRift
