#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

// ---------------------------------------------------------------------------
// Drawing tools
// ---------------------------------------------------------------------------
enum class DrawTool
{
    Freehand,
    Eraser,
    StraightLine,
    FilledRect,
    CircleBorder
};

// ---------------------------------------------------------------------------
class Canvas
{
public:
    Canvas();

    // ── Tool state (write from UI) ──────────────────────────────────────────
    DrawTool  tool = DrawTool::Freehand;
    sf::Color penColor = sf::Color::Black;
    float     brushSize = 10.f;

    // ── Mouse events – coords in canvas-texture space ───────────────────────
    void mousePressed(sf::Vector2f pos);
    void mouseMoved(sf::Vector2f pos);
    void mouseReleased(sf::Vector2f pos);

    // ── Accessors ───────────────────────────────────────────────────────────
    sf::Image          getImage()            const;
    const sf::Texture& getCommittedTexture() const;
    sf::Vector2u       getSize()             const;

    void clear();   // reset to white

    // ── Preview helpers (used by AppUI DrawList overlay) ────────────────────
    bool         isDragging()   const { return dragging_; }
    sf::Vector2f getDragStart() const { return dragStart_; }
    sf::Vector2f getDragCur()   const { return dragCur_; }

private:
    sf::RenderTexture rt_;

    bool         dragging_ = false;
    sf::Vector2f dragStart_;
    sf::Vector2f dragCur_;

    std::vector<sf::Vector2f> stroke_;  // freehand accumulated points

    // ── Internal draw helpers ───────────────────────────────────────────────
    void drawThickSegment(sf::Vector2f p0, sf::Vector2f p1, sf::Color col);
    void drawFilledRect(sf::Vector2f p0, sf::Vector2f p1, sf::Color col);
    void drawCircleBorder(sf::Vector2f center, float radius, sf::Color col);
};