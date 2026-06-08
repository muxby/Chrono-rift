#pragma once

// ═══════════════════════════════════════════════════════════════════════════
//  SpriteAnimation — Spritesheet-based fighter animation system
//  Loads 5×5 grid spritesheets and cycles frames for standing / attack
// ═══════════════════════════════════════════════════════════════════════════

#include <SFML/Graphics.hpp>
#include <string>

namespace ChronoRift {

// ─────────────────────────────────────────────────────────────────────────
//  SpriteSheet — loads a single spritesheet image, divides into grid
// ─────────────────────────────────────────────────────────────────────────
class SpriteSheet {
public:
    SpriteSheet();

    // Load spritesheet PNG.  cols/rows define the grid layout (default 5×5).
    bool loadFromFile(const std::string& path, int cols = 5, int rows = 5);

    // Get the texture sub-rect for frame at the given index (0-based, row-major).
    sf::IntRect getFrameRect(int frameIndex) const;

    int          getTotalFrames()  const { return totalFrames_; }
    sf::Texture& getTexture()           { return texture_; }
    const sf::Texture& getTexture() const { return texture_; }
    int          getFrameWidth()   const { return frameWidth_; }
    int          getFrameHeight()  const { return frameHeight_; }
    bool         isLoaded()        const { return loaded_; }

private:
    sf::Texture texture_;
    int  cols_, rows_;
    int  frameWidth_, frameHeight_;
    int  totalFrames_;
    bool loaded_;
};

// ─────────────────────────────────────────────────────────────────────────
//  FighterSprite — animated fighter with standing + attack spritesheets
// ─────────────────────────────────────────────────────────────────────────
class FighterSprite {
public:
    enum AnimState { ANIM_STANDING = 0, ANIM_ATTACKING = 1 };

    FighterSprite();

    bool loadStanding(const std::string& path);
    bool loadAttack(const std::string& path);

    // Advance animation timer by dt seconds
    void update(float dt);

    // Draw the current frame at (x, y) with the given scale.
    // If flipH is true the sprite is mirrored horizontally (for enemies).
    void draw(sf::RenderWindow& window, float x, float y,
              float scale, bool flipH = false);

    // Trigger the attack animation — plays all frames once, then reverts.
    void triggerAttack();

    void      setFPS(float fps)    { animFPS_ = fps; }
    AnimState getState()     const { return currentState_; }
    bool      isLoaded()     const { return standing_.isLoaded(); }

    // Portrait helpers — first frame of standing sheet
    sf::IntRect   getPortraitRect()  const;
    sf::Texture&  getStandingTexture();
    int           getFrameWidth()  const;
    int           getFrameHeight() const;
    int           getActiveFrameWidth()  const;
    int           getActiveFrameHeight() const;

private:
    SpriteSheet standing_;
    SpriteSheet attack_;
    sf::Sprite  sprite_;

    AnimState currentState_;
    int       currentFrame_;
    float     frameTimer_;
    float     animFPS_;
};

} // namespace ChronoRift
