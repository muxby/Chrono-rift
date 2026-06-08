#pragma once

#include <SFML/Graphics.hpp>

namespace ChronoRift {

// ---
// CHRONO RIFT - PLAIN WHITE THEME (Clean, readable, no fancy effects)
// ---

// -- Base Colors --
inline const sf::Color BACKGROUND_DARK     (245, 245, 245, 255);   // Light gray background
inline const sf::Color BACKGROUND_PANEL    (255, 255, 255, 255);   // White panels
inline const sf::Color BACKGROUND_CARD     (250, 250, 250, 255);   // Slightly off-white cards
inline const sf::Color BACKGROUND_INPUT    (240, 240, 240, 255);   // Input field bg

// -- Accent Colors (muted, professional) --
inline const sf::Color ACCENT_BLUE         (50,  100, 180, 255);   // Primary blue
inline const sf::Color ACCENT_CYAN         (0,   120, 160, 255);   // Secondary cyan
inline const sf::Color ACCENT_GREEN        (40,  150, 60,  255);   // Success green
inline const sf::Color ACCENT_RED          (180, 50,  50,  255);   // Danger red
inline const sf::Color ACCENT_PURPLE       (120, 60,  140, 255);   // Accent purple
inline const sf::Color ACCENT_YELLOW       (180, 150, 30,  255);   // Warning yellow
inline const sf::Color ACCENT_ORANGE       (180, 100, 30,  255);   // Orange accent
inline const sf::Color ACCENT_WHITE        (255, 255, 255, 255);   // White

// For backward compatibility - map neon names to muted accents
inline const sf::Color NEON_BLUE           (50,  100, 180, 255);
inline const sf::Color NEON_CYAN           (0,   120, 160, 255);
inline const sf::Color NEON_PURPLE         (120, 60,  140, 255);
inline const sf::Color NEON_PINK           (180, 60,  120, 255);
inline const sf::Color NEON_GREEN          (40,  150, 60,  255);
inline const sf::Color NEON_YELLOW         (180, 150, 30,  255);
inline const sf::Color NEON_RED            (180, 50,  50,  255);
inline const sf::Color NEON_ORANGE         (180, 100, 30,  255);
inline const sf::Color NEON_WHITE          (255, 255, 255, 255);

// -- State Colors --
inline const sf::Color STATE_RUNNING       (40,  150, 60,  255);   // Green
inline const sf::Color STATE_READY         (180, 150, 30,  255);   // Yellow
inline const sf::Color STATE_BLOCKED       (180, 50,  50,  255);   // Red
inline const sf::Color STATE_DEAD          (120, 120, 120, 255);   // Gray

// -- Bar Colors --
inline const sf::Color HP_BAR_HIGH         (40,  150, 60,  255);   // >50%
inline const sf::Color HP_BAR_MED          (180, 150, 30,  255);   // 20-50%
inline const sf::Color HP_BAR_LOW          (180, 50,  50,  255);   // <20%
inline const sf::Color STAMINA_BAR         (0,   120, 160, 255);   // Cyan
inline const sf::Color PROGRESS_BAR        (50,  100, 180, 255);   // Blue

// -- Text Colors --
inline const sf::Color TEXT_PRIMARY        (30,  30,  30,  255);   // Dark text
inline const sf::Color TEXT_SECONDARY      (80,  80,  80,  255);   // Secondary
inline const sf::Color TEXT_DIM            (140, 140, 140, 255);   // Dim text
inline const sf::Color TEXT_ACCENT         (0,   120, 160, 255);   // Accent text

// -- Border & Divider --
inline const sf::Color BORDER_COLOR        (180, 180, 180, 255);   // Light gray border
inline const sf::Color BORDER_GLOW         (200, 200, 200, 255);   // Subtle glow (no actual glow)

// -- Gantt Chart Colors --
inline const sf::Color GANTT_PLAYER        (40,  150, 60,  180);   // Green
inline const sf::Color GANTT_NPC           (180, 50,  50,  180);   // Red
inline const sf::Color GANTT_BG            (255, 255, 255, 255);   // White
inline const sf::Color GANTT_GRID          (220, 220, 220, 255);   // Light gray grid

// -- Layout Constants --
constexpr float PADDING_SMALL   = 5.f;
constexpr float PADDING_MEDIUM  = 10.f;
constexpr float PADDING_LARGE   = 20.f;
constexpr float CORNER_RADIUS   = 4.f;
constexpr float BORDER_THICK    = 1.f;
constexpr float GLOW_THICK      = 1.f;    // No glow - just thin border

// -- Font Size Constants --
constexpr unsigned FONT_TITLE    = 24;
constexpr unsigned FONT_HEADER   = 18;
constexpr unsigned FONT_SUBTITLE = 14;
constexpr unsigned FONT_BODY     = 12;
constexpr unsigned FONT_SMALL    = 11;
constexpr unsigned FONT_TINY     = 10;

} // namespace ChronoRift
