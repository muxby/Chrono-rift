#include "UIComponents.hpp"

using namespace std;

namespace ChronoRift {

LogPanel::LogPanel(float px, float py, float w, float h, sf::Font& f)
    : x(px), y(py), width(w), height(h), font(f), logCount(0) {
    bg.setPosition(px, py);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    headerText.setFont(font);
    headerText.setString("RECENT LOG");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(TEXT_PRIMARY);
    headerText.setStyle(sf::Text::Bold);
    headerText.setPosition(px, py - 28.f);
}

void LogPanel::update(const SharedState* state) {
    logCount = 0;

    int idx = state->log_head;
    int maxEntries = static_cast<int>((height - 10.f) / 18.f);
    if (maxEntries > MAX_LOG_ENTRIES) maxEntries = MAX_LOG_ENTRIES;

    for (int i = 0; i < maxEntries; ++i) {
        int pos = (idx - 1 - i + MAX_LOG) % MAX_LOG;
        logEntries[logCount].setFont(font);
        logEntries[logCount].setString("> " + string(state->logs[pos]));
        logEntries[logCount].setCharacterSize(FONT_SMALL);

        // Use readable colors for white background
        if (i < 3) {
            logEntries[logCount].setFillColor(TEXT_PRIMARY);
        } else {
            logEntries[logCount].setFillColor(TEXT_DIM);
        }

        logEntries[logCount].setPosition(x + 5.f, y + 5.f + i * 18.f);
        logCount++;
    }
}

void LogPanel::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(headerText);
    for (int i = 0; i < logCount; ++i) {
        window.draw(logEntries[i]);
    }
}

} // namespace ChronoRift
