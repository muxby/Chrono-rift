#include "SpriteAnimation.hpp"
#include <iostream>

using namespace std;

namespace ChronoRift {

// ═══════════════════════════════════════════════════════════════════════════
//  SpriteSheet
// ═══════════════════════════════════════════════════════════════════════════

SpriteSheet::SpriteSheet()
    : cols_(5), rows_(5), frameWidth_(0), frameHeight_(0),
      totalFrames_(25), loaded_(false) {}

bool SpriteSheet::loadFromFile(const string& path, int cols, int rows) {
    cols_ = cols;
    rows_ = rows;
    totalFrames_ = cols_ * rows_;
    loaded_ = texture_.loadFromFile(path);

    if (loaded_) {
        texture_.setSmooth(true);
        sf::Vector2u sz = texture_.getSize();
        frameWidth_  = static_cast<int>(sz.x) / cols_;
        frameHeight_ = static_cast<int>(sz.y) / rows_;
        cerr << "[SPRITE] Loaded " << path
             << " (" << sz.x << "x" << sz.y
             << ", frame=" << frameWidth_ << "x" << frameHeight_ << ")" << endl;
    } else {
        cerr << "[SPRITE] FAILED to load: " << path << endl;
    }
    return loaded_;
}

sf::IntRect SpriteSheet::getFrameRect(int frameIndex) const {
    if (!loaded_ || frameIndex < 0) return sf::IntRect(0, 0, 0, 0);
    frameIndex = frameIndex % totalFrames_;
    int col = frameIndex % cols_;
    int row = frameIndex / cols_;
    return sf::IntRect(col * frameWidth_, row * frameHeight_,
                       frameWidth_, frameHeight_);
}

// ═══════════════════════════════════════════════════════════════════════════
//  FighterSprite
// ═══════════════════════════════════════════════════════════════════════════

FighterSprite::FighterSprite()
    : currentState_(ANIM_STANDING), currentFrame_(0),
      frameTimer_(0.f), animFPS_(12.f) {}

bool FighterSprite::loadStanding(const string& path) {
    return standing_.loadFromFile(path);
}

bool FighterSprite::loadAttack(const string& path) {
    return attack_.loadFromFile(path);
}

void FighterSprite::update(float dt) {
    if (!standing_.isLoaded()) return;

    frameTimer_ += dt;
    float frameDuration = 1.f / animFPS_;

    while (frameTimer_ >= frameDuration) {
        frameTimer_ -= frameDuration;
        currentFrame_++;

        if (currentState_ == ANIM_ATTACKING && attack_.isLoaded()) {
            if (currentFrame_ >= attack_.getTotalFrames()) {
                // Attack animation finished → revert to standing
                currentState_ = ANIM_STANDING;
                currentFrame_ = 0;
            }
        } else {
            currentFrame_ = currentFrame_ % standing_.getTotalFrames();
        }
    }
}

void FighterSprite::draw(sf::RenderWindow& window, float x, float y,
                         float scale, bool flipH) {
    if (!standing_.isLoaded()) return;

    SpriteSheet& sheet = (currentState_ == ANIM_ATTACKING && attack_.isLoaded())
                         ? attack_ : standing_;

    sprite_.setTexture(sheet.getTexture());
    sprite_.setTextureRect(sheet.getFrameRect(currentFrame_));

    if (flipH) {
        sprite_.setOrigin(static_cast<float>(sheet.getFrameWidth()), 0.f);
        sprite_.setScale(-scale, scale);
    } else {
        sprite_.setOrigin(0.f, 0.f);
        sprite_.setScale(scale, scale);
    }

    sprite_.setPosition(x, y);
    window.draw(sprite_);
}

void FighterSprite::triggerAttack() {
    if (attack_.isLoaded()) {
        currentState_ = ANIM_ATTACKING;
        currentFrame_ = 0;
        frameTimer_   = 0.f;
    }
}

sf::IntRect FighterSprite::getPortraitRect() const {
    return standing_.getFrameRect(0);
}

sf::Texture& FighterSprite::getStandingTexture() {
    return standing_.getTexture();
}

int FighterSprite::getFrameWidth() const {
    return standing_.getFrameWidth();
}

int FighterSprite::getFrameHeight() const {
    return standing_.getFrameHeight();
}

int FighterSprite::getActiveFrameWidth() const {
    if (currentState_ == ANIM_ATTACKING && attack_.isLoaded()) {
        return attack_.getFrameWidth();
    }
    return standing_.getFrameWidth();
}

int FighterSprite::getActiveFrameHeight() const {
    if (currentState_ == ANIM_ATTACKING && attack_.isLoaded()) {
        return attack_.getFrameHeight();
    }
    return standing_.getFrameHeight();
}

} // namespace ChronoRift
