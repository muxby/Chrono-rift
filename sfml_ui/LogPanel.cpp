#include "UIComponents.hpp"

namespace ChronoRift {

LogPanel::LogPanel(float px, float py, float w, float h, sf::Font& f)
    : x(px), y(py), width(w), height(h), font(f) {
    bg.setPosition(px, py);
    bg.setSize(sf::Vector2f(w, h));
    bg.setFillColor(BACKGROUND_CARD);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);

    headerText.setFont(font);
    headerText.setString("RECENT LOG");
    headerText.setCharacterSize(FONT_SUBTITLE);
    headerText.setFillColor(NEON_CYAN);
    headerText.setStyle(sf::Text::Bold);
    headerText.setPosition(px, py - 28.f);
}

void LogPanel::update(const SharedState* state) {
    logEntries.clear();

    int idx = state->log_head;
    int maxEntries = static_cast<int>((height - 10.f) / 18.f);

    for (int i = 0; i < maxEntries; ++i) {
        int pos = (idx - 1 - i + MAX_LOG) % MAX_LOG;
        sf::Text entry;
        entry.setFont(font);
        entry.setString("> " + std::string(state->logs[pos]));
        entry.setCharacterSize(FONT_SMALL);

        if (i < 3) {
            entry.setFillColor(TEXT_PRIMARY);
        } else {
            entry.setFillColor(TEXT_DIM);
        }

        entry.setPosition(x + 5.f, y + 5.f + i * 18.f);
        logEntries.push_back(entry);
    }
}

void LogPanel::draw(sf::RenderWindow& window) {
    window.draw(bg);
    window.draw(headerText);
    for (auto& entry : logEntries) {
        window.draw(entry);
    }
}

} // namespace ChronoRift
