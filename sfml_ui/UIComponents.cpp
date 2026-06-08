#include "UIComponents.hpp"
#include "os_helpers.hpp"
#include <cmath>
#include <cstdio>
#include <iomanip>

using namespace std;

namespace ChronoRift {

// progress bar

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
    percent = cr_max(0.f, cr_min(1.f, percent));
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

// button

Button::Button(float x, float y, float w, float h, const string& text,
               sf::Font& font, unsigned fontSize)
    : normalColor(sf::Color(220, 220, 220, 255)),
      hoverColor(sf::Color(200, 200, 200, 255)),
      textColor(TEXT_PRIMARY),
      hovered(false), callback(nullptr), callbackWithTag(nullptr), callbackCtx(nullptr), tag(0) {
    shape.setPosition(x, y);
    shape.setSize(sf::Vector2f(w, h));
    shape.setFillColor(normalColor);
    shape.setOutlineColor(BORDER_COLOR);
    shape.setOutlineThickness(1.f);

    label.setFont(font);
    label.setString(text);
    label.setCharacterSize(fontSize);
    label.setFillColor(textColor);
    label.setStyle(sf::Text::Bold);

    sf::FloatRect textBounds = label.getLocalBounds();
    label.setPosition(
        x + (w - textBounds.width) / 2.f,
        y + (h - textBounds.height) / 2.f - 2.f
    );
}

void Button::draw(sf::RenderWindow& window) {
    shape.setFillColor(hovered ? hoverColor : normalColor);
    shape.setOutlineThickness(hovered ? 1.5f : 1.f);
    window.draw(shape);
    window.draw(label);
}

bool Button::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    hovered = shape.getGlobalBounds().contains(mousePos);

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        if (hovered) {
            if (callback) { callback(callbackCtx); return true; }
            if (callbackWithTag) { callbackWithTag(callbackCtx, tag); return true; }
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

void Button::setCallback(void (*cb)(void*), void* ctx) {
    callback = cb;
    callbackCtx = ctx;
}

void Button::setCallbackWithTag(void (*cb)(void*, int), void* ctx, int t) {
    callbackWithTag = cb;
    callbackCtx = ctx;
    tag = t;
}

void Button::setText(const string& text) {
    label.setString(text);
    sf::FloatRect bounds = shape.getGlobalBounds();
    sf::FloatRect textBounds = label.getLocalBounds();
    label.setPosition(
        bounds.left + (bounds.width - textBounds.width) / 2.f,
        bounds.top + (bounds.height - textBounds.height) / 2.f - 2.f
    );
}

// slider

Slider::Slider(float x, float y, float w, float minVal, float maxVal,
               float initialVal, const string& label, sf::Font& font)
    : minValue(minVal), maxValue(maxVal), currentValue(initialVal),
      trackX(x), trackY(y), trackWidth(w), dragging(false) {
    track.setPosition(x, y);
    track.setSize(sf::Vector2f(w, 6.f));
    track.setFillColor(BACKGROUND_CARD);
    track.setOutlineColor(BORDER_COLOR);
    track.setOutlineThickness(1.f);

    thumb.setRadius(8.f);
    thumb.setFillColor(ACCENT_BLUE);
    thumb.setOutlineColor(BORDER_COLOR);
    thumb.setOutlineThickness(1.f);
    thumb.setOrigin(8.f, 8.f);

    labelText.setFont(font);
    labelText.setString(label);
    labelText.setCharacterSize(FONT_SMALL);
    labelText.setFillColor(TEXT_SECONDARY);
    labelText.setPosition(x, y - 22.f);

    valueText.setFont(font);
    valueText.setCharacterSize(FONT_SMALL);
    valueText.setFillColor(TEXT_PRIMARY);
    updateThumbPosition();
}

void Slider::updateThumbPosition() {
    float t = (currentValue - minValue) / (maxValue - minValue);
    float thumbX = trackX + t * trackWidth;
    thumb.setPosition(thumbX, trackY + 3.f);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", currentValue);
    valueText.setString(buf);
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
        float t = cr_max(0.f, cr_min(1.f, (mousePos.x - trackX) / trackWidth));
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
    currentValue = cr_max(minValue, cr_min(maxValue, val));
    updateThumbPosition();
}

// character card - shows hp, stamina, inventory slots

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

    // No glow border - just a simple highlight when active
    glowBorder.setPosition(x, y);
    glowBorder.setSize(sf::Vector2f(w, h));
    glowBorder.setFillColor(sf::Color::Transparent);
    glowBorder.setOutlineColor(sf::Color::Transparent);
    glowBorder.setOutlineThickness(2.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_BODY);
    nameText.setFillColor(isPlayer ? ACCENT_GREEN : ACCENT_RED);
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
    statusText.setFillColor(ACCENT_YELLOW);
    statusText.setPosition(x + w - 70.f, y + 64.f);
}

void CharacterCard::update(const CharacterState& state, bool isActive, int currentEpoch) {
    active = isActive;
    string prefix = isPlayerType ? "P" : "N";
    nameText.setString(prefix + to_string(state.id));

    if (!state.alive) {
        nameText.setFillColor(TEXT_DIM);
        hpText.setString("DEAD");
        hpBar.setProgress(0.f);
        stBar.setProgress(0.f);
        dmgText.setString("DMG: " + to_string(state.dmg));
        statusText.setString("");
        cardBg.setFillColor(sf::Color(230, 230, 230, 255));
        return;
    }

    float hpPct = static_cast<float>(state.hp) / cr_max(1, state.max_hp);
    float stPct = static_cast<float>(state.stamina) / cr_max(1, state.max_stamina);

    hpBar.setProgress(hpPct);
    stBar.setProgress(stPct);

    if (hpPct > 0.5f) hpBar.setColors(HP_BAR_HIGH, BACKGROUND_CARD);
    else if (hpPct > 0.2f) hpBar.setColors(HP_BAR_MED, BACKGROUND_CARD);
    else hpBar.setColors(HP_BAR_LOW, BACKGROUND_CARD);

    hpText.setString("HP: " + to_string(state.hp) + "/" + to_string(state.max_hp));
    stText.setString("ST: " + to_string(state.stamina) + "/" + to_string(state.max_stamina));
    dmgText.setString("DMG: " + to_string(state.dmg) + " SPD: " + to_string(state.speed));

    if (state.stunned_until_epoch > currentEpoch) {
        int stunLeft = state.stunned_until_epoch - currentEpoch;
        statusText.setString("[STUN " + to_string(stunLeft) + "s]");
    } else {
        statusText.setString(isActive ? "<< ACTIVE" : "");
    }

    if (isActive) {
        glowBorder.setOutlineColor(ACCENT_BLUE);
        cardBg.setFillColor(sf::Color(235, 245, 255, 255));
    } else {
        glowBorder.setOutlineColor(sf::Color::Transparent);
        cardBg.setFillColor(BACKGROUND_CARD);
    }

    nameText.setFillColor(isPlayerType ? ACCENT_GREEN : ACCENT_RED);
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

// artifact display

ArtifactDisplay::ArtifactDisplay(float x, float y, float w, sf::Font& font)
    : artifactIndex(0) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, 50.f));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    icon.setRadius(10.f);
    icon.setPosition(x + 8.f, y + 15.f);
    icon.setFillColor(ACCENT_PURPLE);
    icon.setOutlineColor(BORDER_COLOR);
    icon.setOutlineThickness(1.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_BODY);
    nameText.setFillColor(ACCENT_PURPLE);
    nameText.setPosition(x + 30.f, y + 4.f);

    statusText.setFont(font);
    statusText.setCharacterSize(FONT_SMALL);
    statusText.setFillColor(TEXT_SECONDARY);
    statusText.setPosition(x + 30.f, y + 26.f);

    waitingText.setFont(font);
    waitingText.setCharacterSize(FONT_TINY);
    waitingText.setFillColor(ACCENT_YELLOW);
    waitingText.setPosition(x + w - 80.f, y + 30.f);
}

void ArtifactDisplay::update(const ArtifactState& state, int index, bool eclipsePresent) {
    artifactIndex = index;
    const char* names[] = {"Solar Core", "Lunar Blade", "Eclipse Relic"};
    nameText.setString(names[index]);

    if (index == 2 && !eclipsePresent) {
        statusText.setString("[Hidden]");
        statusText.setFillColor(TEXT_DIM);
        icon.setFillColor(sf::Color(180, 180, 180, 255));
    } else if (state.holder_team == -1) {
        statusText.setString("[Free]");
        statusText.setFillColor(ACCENT_YELLOW);
        icon.setFillColor(ACCENT_PURPLE);
    } else {
        string team = (state.holder_team == TEAM_PLAYER) ? "P" : "N";
        statusText.setString(team + to_string(state.holder_id));
        statusText.setFillColor(state.holder_team == TEAM_PLAYER ? ACCENT_GREEN : ACCENT_RED);
        icon.setFillColor(state.holder_team == TEAM_PLAYER ? ACCENT_GREEN : ACCENT_RED);
    }

    if (state.waiting_count > 0) {
        waitingText.setString(to_string(state.waiting_count) + " waiting");
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

// metrics dashboard - shows scheduling stats

MetricsDashboard::MetricsDashboard(float x, float y, float w, sf::Font& font) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, 140.f));
    bg.setFillColor(BACKGROUND_PANEL);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    headerText.setFont(font);
    headerText.setString("SYSTEM METRICS");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(ACCENT_BLUE);
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
    killsText.setFillColor(ACCENT_GREEN);
    killsText.setPosition(x + w / 2.f, y + 104.f);
}

void MetricsDashboard::update(float avgWait, float avgTurnaround, float throughput,
                               int turns, int kills, int totalProcesses) {
    (void)totalProcesses;
    char buf[64];

    snprintf(buf, sizeof(buf), "Avg Wait Time: %.2fs", avgWait);
    waitText.setString(buf);

    snprintf(buf, sizeof(buf), "Avg Turnaround: %.2fs", avgTurnaround);
    turnaroundText.setString(buf);

    snprintf(buf, sizeof(buf), "Throughput: %.2f proc/s", throughput);
    throughputText.setString(buf);

    turnsText.setString("Turns: " + to_string(turns));
    killsText.setString("Kills: " + to_string(kills) + "/10");
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

// gantt chart for turn timeline

GanttChart::GanttChart(float x, float y, float w, float h, sf::Font& font)
    : x(x), y(y), width(w), height(h), font(font), entryCount(0),
      currentTime(0.f), timeWindow(30.f) {
    headerText.setFont(font);
    headerText.setString("GANTT CHART - Process Scheduling Timeline");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(ACCENT_BLUE);
    headerText.setStyle(sf::Text::Bold);
    headerText.setPosition(x, y - 30.f);

    timeText.setFont(font);
    timeText.setCharacterSize(FONT_SMALL);
    timeText.setFillColor(TEXT_SECONDARY);
    timeText.setPosition(x + width - 100.f, y - 30.f);
}

void GanttChart::addEntry(int entityId, TeamType team, float startTime, float duration) {
    if (entryCount < MAX_GANTT_ENTRIES) {
        entries[entryCount++] = {entityId, team, startTime, duration};
    }
}

void GanttChart::update(float time) {
    currentTime = time;

    int writeIdx = 0;
    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].startTime + entries[i].duration >= currentTime - timeWindow) {
            entries[writeIdx++] = entries[i];
        }
    }
    entryCount = writeIdx;
}

void GanttChart::clear() {
    entryCount = 0;
}

void GanttChart::drawGrid(sf::RenderWindow& window) {
    sf::VertexArray grid(sf::Lines);

    sf::RectangleShape bg(sf::Vector2f(width, height));
    bg.setPosition(x, y);
    bg.setFillColor(GANTT_BG);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    int numCols = 10;
    for (int i = 0; i <= numCols; ++i) {
        float gx = x + (width / numCols) * i;
        grid.append(sf::Vertex(sf::Vector2f(gx, y), GANTT_GRID));
        grid.append(sf::Vertex(sf::Vector2f(gx, y + height), GANTT_GRID));
    }

    int maxEntities = cr_max(MAX_PLAYERS, MAX_NPCS);
    float laneHeight = height / (maxEntities + 1);
    for (int i = 0; i <= maxEntities + 1; ++i) {
        float gy = y + laneHeight * i;
        grid.append(sf::Vertex(sf::Vector2f(x, gy), GANTT_GRID));
        grid.append(sf::Vertex(sf::Vector2f(x + width, gy), GANTT_GRID));
    }

    window.draw(grid);

    for (int i = 0; i <= numCols; ++i) {
        float t = currentTime - timeWindow + (timeWindow / numCols) * i;
        sf::Text lbl;
        lbl.setFont(font);
        lbl.setCharacterSize(FONT_TINY);
        lbl.setFillColor(TEXT_DIM);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fs", t);
        lbl.setString(buf);
        lbl.setPosition(x + (width / numCols) * i + 2.f, y + height + 2.f);
        window.draw(lbl);
    }
}

void GanttChart::drawEntries(sf::RenderWindow& window) {
    float laneHeight = height / (MAX_NPCS + 1);

    for (int i = 0; i < entryCount; ++i) {
        const GanttEntry& entry = entries[i];
        float relStart = entry.startTime - (currentTime - timeWindow);
        float px = x + (relStart / timeWindow) * width;
        float pw = (entry.duration / timeWindow) * width;
        float py = y + entry.entityId * laneHeight + 2.f;
        float ph = laneHeight - 4.f;

        if (px + pw < x || px > x + width) continue;

        px = cr_max(x, px);
        pw = cr_min(pw, (x + width) - px);

        sf::RectangleShape bar(sf::Vector2f(pw, ph));
        bar.setPosition(px, py);
        bar.setFillColor(entry.team == TEAM_PLAYER ? GANTT_PLAYER : GANTT_NPC);
        bar.setOutlineColor(entry.team == TEAM_PLAYER ? ACCENT_GREEN : ACCENT_RED);
        bar.setOutlineThickness(0.5f);
        window.draw(bar);

        if (pw > 20.f) {
            sf::Text lbl;
            lbl.setFont(font);
            lbl.setCharacterSize(FONT_TINY);
            lbl.setFillColor(sf::Color::White);
            string prefix = entry.team == TEAM_PLAYER ? "P" : "N";
            lbl.setString(prefix + to_string(entry.entityId));
            lbl.setPosition(px + 3.f, py + 1.f);
            window.draw(lbl);
        }
    }
}

void GanttChart::draw(sf::RenderWindow& window) {
    char buf[32];
    snprintf(buf, sizeof(buf), "T: %.1fs", currentTime);
    timeText.setString(buf);

    window.draw(headerText);
    window.draw(timeText);
    drawGrid(window);
    drawEntries(window);
}

// arbiter node - the circle in the header showing whos active

ArbiterNode::ArbiterNode(float x, float y, float r, sf::Font& font)
    : x(x), y(y), radius(r), pulsePhase(0.f), polling(false), activeTeam(-1), activeId(-1) {
    outerRing.setRadius(r + 4.f);
    outerRing.setOrigin(r + 4.f, r + 4.f);
    outerRing.setPosition(x, y);
    outerRing.setFillColor(sf::Color::Transparent);
    outerRing.setOutlineColor(ACCENT_BLUE);
    outerRing.setOutlineThickness(1.5f);

    innerCircle.setRadius(r);
    innerCircle.setOrigin(r, r);
    innerCircle.setPosition(x, y);
    innerCircle.setFillColor(BACKGROUND_PANEL);
    innerCircle.setOutlineColor(ACCENT_BLUE);
    innerCircle.setOutlineThickness(1.5f);

    labelText.setFont(font);
    labelText.setString("ARBITER");
    labelText.setCharacterSize(FONT_SMALL);
    labelText.setFillColor(ACCENT_BLUE);
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

    if (polling) {
        outerRing.setOutlineColor(ACCENT_BLUE);
    } else {
        outerRing.setOutlineColor(BORDER_COLOR);
    }

    if (activeTeam == TEAM_PLAYER) {
        statusText.setString("P" + to_string(activeId));
        statusText.setFillColor(ACCENT_GREEN);
        innerCircle.setOutlineColor(ACCENT_GREEN);
    } else if (activeTeam == TEAM_NPC) {
        statusText.setString("N" + to_string(activeId));
        statusText.setFillColor(ACCENT_RED);
        innerCircle.setOutlineColor(ACCENT_RED);
    } else {
        statusText.setString("IDLE");
        statusText.setFillColor(TEXT_DIM);
        innerCircle.setOutlineColor(ACCENT_BLUE);
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

// process block - one per player/npc in the scheduler view

ProcessBlock::ProcessBlock(float x, float y, float w, float h,
                           int pid, sf::Font& font)
    : processId(pid),
      progressBar(x + 5.f, y + h - 18.f, w - 10.f, 10.f, PROGRESS_BAR),
      currentState(3) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    glow.setPosition(x, y);
    glow.setSize(sf::Vector2f(w, h));
    glow.setFillColor(sf::Color::Transparent);
    glow.setOutlineColor(sf::Color::Transparent);
    glow.setOutlineThickness(1.f);

    nameText.setFont(font);
    nameText.setCharacterSize(FONT_SMALL);
    nameText.setFillColor(TEXT_PRIMARY);
    nameText.setPosition(x + 8.f, y + 4.f);

    pidText.setFont(font);
    pidText.setCharacterSize(FONT_TINY);
    pidText.setFillColor(TEXT_DIM);
    pidText.setPosition(x + w - 35.f, y + 4.f);
    pidText.setString("PID:" + to_string(pid));

    stateText.setFont(font);
    stateText.setCharacterSize(FONT_TINY);
    stateText.setFillColor(STATE_READY);
    stateText.setPosition(x + 8.f, y + 22.f);
}

void ProcessBlock::update(float progress, int state, const string& name) {
    currentState = state;
    if (state == 3) return;  // hidden — skip all visual updates
    progressBar.setProgress(progress);
    nameText.setString(name);

    sf::Color stateColor;
    string stateStr;
    switch (state) {
        case 0: stateColor = STATE_RUNNING; stateStr = "RUNNING"; break;
        case 1: stateColor = STATE_READY; stateStr = "READY"; break;
        case 2: stateColor = STATE_BLOCKED; stateStr = "BLOCKED"; break;
        default: stateColor = TEXT_DIM; stateStr = "READY"; break;
    }

    stateText.setString(stateStr);
    stateText.setFillColor(stateColor);
    bg.setOutlineColor(stateColor);
    glow.setOutlineColor(sf::Color::Transparent);
}

void ProcessBlock::draw(sf::RenderWindow& window) {
    if (currentState == 3) return;  // hidden — don't draw
    window.draw(bg);
    window.draw(glow);
    window.draw(nameText);
    window.draw(pidText);
    window.draw(stateText);
    progressBar.draw(window);
}

void ProcessBlock::setPosition(float x, float y) {
    bg.setPosition(x, y);
    glow.setPosition(x, y);
    nameText.setPosition(x + 8.f, y + 4.f);
    pidText.setPosition(x + bg.getSize().x - 35.f, y + 4.f);
    stateText.setPosition(x + 8.f, y + 22.f);
    progressBar.setPosition(x + 5.f, y + bg.getSize().y - 18.f);
}

sf::FloatRect ProcessBlock::getBounds() const {
    return bg.getGlobalBounds();
}

// hip slot - shows which player thread is running

HIPSlot::HIPSlot(float x, float y, float w, float h,
                 int sid, const string& type, sf::Font& font)
    : slotId(sid), resourceType(type), occupied(false) {
    bg.setPosition(x, y);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    glow.setPosition(x, y);
    glow.setSize(sf::Vector2f(w, h));
    glow.setFillColor(sf::Color::Transparent);
    glow.setOutlineColor(sf::Color::Transparent);
    glow.setOutlineThickness(1.f);

    idText.setFont(font);
    idText.setCharacterSize(FONT_SMALL);
    idText.setFillColor(TEXT_SECONDARY);
    idText.setPosition(x + 8.f, y + 4.f);
    idText.setString("CPU " + to_string(sid));

    typeText.setFont(font);
    typeText.setCharacterSize(FONT_TINY);
    typeText.setFillColor(TEXT_DIM);
    typeText.setPosition(x + 8.f, y + 22.f);
    typeText.setString(type);

    occupantText.setFont(font);
    occupantText.setCharacterSize(FONT_TINY);
    occupantText.setFillColor(ACCENT_GREEN);
    occupantText.setPosition(x + 8.f, y + 38.f);
    occupantText.setString("[ IDLE ]");
}

void HIPSlot::setOccupied(bool occ, int processId, const string& processName) {
    occupied = occ;
    if (occupied) {
        occupantText.setString("P" + to_string(processId) + ": " + processName);
        occupantText.setFillColor(ACCENT_GREEN);
        bg.setOutlineColor(ACCENT_GREEN);
        glow.setOutlineColor(sf::Color::Transparent);
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

// particle system - disabled, keeping the stubs so it still compiles

ParticleSystem::ParticleSystem() : particleCount(0) {}

void ParticleSystem::emit(float x, float y, sf::Color color, int count) {
    // Particles disabled - plain white UI has no effects
    (void)x;
    (void)y;
    (void)color;
    (void)count;
}

void ParticleSystem::update(float dt) {
    (void)dt;
    // No particles to update
}

void ParticleSystem::draw(sf::RenderWindow& window) {
    (void)window;
    // No particles to draw
}

// connection line between arbiter and active process

ConnectionLine::ConnectionLine() : isActive(false) {
    line.setPrimitiveType(sf::Lines);
    line.resize(2);
    color = sf::Color(180, 180, 180, 60);
}

void ConnectionLine::setPoints(sf::Vector2f from, sf::Vector2f to) {
    line[0].position = from;
    line[1].position = to;
    line[0].color = color;
    line[1].color = color;
}

void ConnectionLine::setActive(bool active) {
    isActive = active;
    color = active ? sf::Color(100, 100, 100, 120) : sf::Color(180, 180, 180, 40);
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
