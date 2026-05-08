#pragma once

#include <SFML/Graphics.hpp>

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
// CHRONO RIFT — NEON DARK THEME
// ═══════════════════════════════════════════════════════════════════════════

// ── Base Colors ───────────────────────────────────────────────────────────
inline const sf::Color BACKGROUND_DARK     (10,  12,  18,  255);
inline const sf::Color BACKGROUND_PANEL    (18,  22,  32,  255);
inline const sf::Color BACKGROUND_CARD     (28,  34,  48,  255);
inline const sf::Color BACKGROUND_INPUT    (22,  26,  38,  255);

// ── Neon Accent Colors ────────────────────────────────────────────────────
inline const sf::Color NEON_CYAN           (0,   255, 255, 255);
inline const sf::Color NEON_BLUE           (30,  144, 255, 255);
inline const sf::Color NEON_PURPLE         (147, 0,   211, 255);
inline const sf::Color NEON_PINK           (255, 20,  147, 255);
inline const sf::Color NEON_GREEN          (57,  255, 20,  255);
inline const sf::Color NEON_YELLOW         (255, 215, 0,   255);
inline const sf::Color NEON_RED            (255, 50,  50,  255);
inline const sf::Color NEON_ORANGE         (255, 140, 0,   255);
inline const sf::Color NEON_WHITE          (240, 248, 255, 255);

// ── State Colors ──────────────────────────────────────────────────────────
inline const sf::Color STATE_RUNNING       (57,  255, 20,  255);   // Green
inline const sf::Color STATE_READY         (255, 215, 0,   255);   // Yellow
inline const sf::Color STATE_BLOCKED       (255, 50,  50,  255);   // Red
inline const sf::Color STATE_DEAD          (60,  60,  70,  255);   // Gray

// ── Bar Colors ────────────────────────────────────────────────────────────
inline const sf::Color HP_BAR_HIGH         (57,  255, 20,  255);   // >50%
inline const sf::Color HP_BAR_MED          (255, 215, 0,   255);   // 20-50%
inline const sf::Color HP_BAR_LOW          (255, 50,  50,  255);   // <20%
inline const sf::Color STAMINA_BAR         (0,   255, 255, 255);   // Cyan
inline const sf::Color PROGRESS_BAR        (30,  144, 255, 255);   // Blue

// ── Text Colors ───────────────────────────────────────────────────────────
inline const sf::Color TEXT_PRIMARY        (240, 248, 255, 255);
inline const sf::Color TEXT_SECONDARY      (150, 160, 180, 255);
inline const sf::Color TEXT_DIM            (100, 110, 130, 255);
inline const sf::Color TEXT_ACCENT         (0,   255, 255, 255);

// ── Border & Divider ──────────────────────────────────────────────────────
inline const sf::Color BORDER_COLOR        (40,  50,  70,  255);
inline const sf::Color BORDER_GLOW         (0,   200, 255, 100);

// ── Gantt Chart Colors ────────────────────────────────────────────────────
inline const sf::Color GANTT_PLAYER        (57,  255, 20,  180);
inline const sf::Color GANTT_NPC           (255, 50,  50,  180);
inline const sf::Color GANTT_BG            (15,  18,  25,  255);
inline const sf::Color GANTT_GRID          (40,  45,  60,  255);

// ── Layout Constants ──────────────────────────────────────────────────────
constexpr float PADDING_SMALL   = 5.f;
constexpr float PADDING_MEDIUM  = 10.f;
constexpr float PADDING_LARGE   = 20.f;
constexpr float CORNER_RADIUS   = 6.f;
constexpr float BORDER_THICK    = 1.5f;
constexpr float GLOW_THICK      = 3.f;

// ── Font Size Constants ───────────────────────────────────────────────────
constexpr unsigned FONT_TITLE    = 28;
constexpr unsigned FONT_HEADER   = 20;
constexpr unsigned FONT_SUBTITLE = 16;
constexpr unsigned FONT_BODY     = 14;
constexpr unsigned FONT_SMALL    = 12;
constexpr unsigned FONT_TINY     = 10;

} // namespace ChronoRift
