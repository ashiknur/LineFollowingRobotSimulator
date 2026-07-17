#include "Canvas.hpp"
#include "config.hpp"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
Canvas::Canvas()
    : rt_(sf::Vector2u{ WIDTH, HEIGHT })
{
    rt_.clear(sf::Color::White);
    rt_.display();
}

// ---------------------------------------------------------------------------
void Canvas::clear()
{
    pushUndoSnapshot();
    rt_.clear(sf::Color::White);
    rt_.display();
    stroke_.clear();
    dragging_ = false;
}

// ---------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------
bool Canvas::saveTo(const std::string& path) const
{
    return rt_.getTexture().copyToImage().saveToFile(path);
}

bool Canvas::loadFrom(const std::string& path)
{
    sf::Image img;
    if (!img.loadFromFile(path))
        return false;

    sf::Texture t;
    if (!t.loadFromImage(img))
        return false;

    pushUndoSnapshot();   // loading a track is undoable

    sf::Sprite s(t);
    sf::Vector2u ts = t.getSize();
    sf::Vector2u cs = rt_.getSize();
    s.setScale({ (float)cs.x / (float)ts.x, (float)cs.y / (float)ts.y });

    rt_.clear(sf::Color::White);
    rt_.draw(s);
    rt_.display();
    return true;
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------
void Canvas::pushUndoSnapshot()
{
    constexpr size_t MAX_UNDO = 20;   // ~2 MB per 700x700 snapshot
    undoStack_.push_back(getImage());
    if (undoStack_.size() > MAX_UNDO)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void Canvas::restoreImage(const sf::Image& img)
{
    sf::Texture t;
    if (!t.loadFromImage(img)) return;
    rt_.clear(sf::Color::White);
    rt_.draw(sf::Sprite(t));
    rt_.display();
}

void Canvas::undo()
{
    if (undoStack_.empty()) return;
    redoStack_.push_back(getImage());
    restoreImage(undoStack_.back());
    undoStack_.pop_back();
}

void Canvas::redo()
{
    if (redoStack_.empty()) return;
    undoStack_.push_back(getImage());
    restoreImage(redoStack_.back());
    redoStack_.pop_back();
}

// ---------------------------------------------------------------------------
void Canvas::mousePressed(sf::Vector2f pos)
{
    pushUndoSnapshot();   // one undo step per stroke/shape

    dragging_ = true;
    dragStart_ = pos;
    dragCur_ = pos;
    stroke_.clear();
    stroke_.push_back(pos);

    // Freehand / eraser: start dot
    if (tool == DrawTool::Freehand || tool == DrawTool::Eraser)
    {
        sf::Color col = (tool == DrawTool::Eraser) ? eraserColor() : penColor;
        sf::CircleShape dot(brushSize / 2.f);
        dot.setOrigin({ brushSize / 2.f, brushSize / 2.f });
        dot.setFillColor(col);
        dot.setPosition(pos);
        rt_.draw(dot);
        rt_.display();
    }
}

// ---------------------------------------------------------------------------
void Canvas::mouseMoved(sf::Vector2f pos)
{
    if (!dragging_) return;
    dragCur_ = pos;

    if (tool == DrawTool::Freehand || tool == DrawTool::Eraser)
    {
        sf::Color col = (tool == DrawTool::Eraser) ? eraserColor() : penColor;
        if (!stroke_.empty())
            drawThickSegment(stroke_.back(), pos, col);
        stroke_.push_back(pos);
        rt_.display();
    }
    // For StraightLine / FilledRect / CircleBorder the preview is rendered
    // by AppUI's DrawList overlay; nothing is committed yet.
}

// ---------------------------------------------------------------------------
void Canvas::mouseReleased(sf::Vector2f pos)
{
    if (!dragging_) return;
    dragCur_ = pos;
    dragging_ = false;

    switch (tool)
    {
    case DrawTool::Freehand:
    case DrawTool::Eraser:
        // Already drawn incrementally – nothing to commit.
        break;

    case DrawTool::StraightLine:
        drawThickSegment(dragStart_, pos, penColor);
        break;

    case DrawTool::FilledRect:
        drawFilledRect(dragStart_, pos, penColor);
        break;

    case DrawTool::CircleBorder: {
        float dx = pos.x - dragStart_.x;
        float dy = pos.y - dragStart_.y;
        float r = std::sqrt(dx * dx + dy * dy);
        drawCircleBorder(dragStart_, r, penColor);
        break;
    }
    }

    rt_.display();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
void Canvas::drawThickSegment(sf::Vector2f p0, sf::Vector2f p1, sf::Color col)
{
    sf::Vector2f d = p1 - p0;
    float        len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.5f) return;

    sf::Vector2f u(d / len);
    sf::Vector2f n(-u.y, u.x);
    float hw = brushSize / 2.f;

    sf::Vertex quad[4];
    quad[0].position = p0 + n * hw;
    quad[1].position = p1 + n * hw;
    quad[2].position = p1 - n * hw;
    quad[3].position = p0 - n * hw;
    for (auto& v : quad) v.color = col;
    rt_.draw(quad, 4, sf::PrimitiveType::TriangleFan);

    // Round cap at p1
    sf::CircleShape cap(hw);
    cap.setOrigin({ hw, hw });
    cap.setFillColor(col);
    cap.setPosition(p1);
    rt_.draw(cap);
}

void Canvas::drawFilledRect(sf::Vector2f p0, sf::Vector2f p1, sf::Color col)
{
    float x = std::min(p0.x, p1.x);
    float y = std::min(p0.y, p1.y);
    float w = std::abs(p1.x - p0.x);
    float h = std::abs(p1.y - p0.y);

    sf::RectangleShape r({ w, h });
    r.setPosition({ x, y });
    r.setFillColor(col);
    r.setOutlineThickness(0.f);
    rt_.draw(r);
}

void Canvas::drawCircleBorder(sf::Vector2f center, float radius, sf::Color col)
{
    sf::CircleShape c(radius, 64);
    c.setOrigin({ radius, radius });
    c.setPosition(center);
    c.setFillColor(sf::Color::Transparent);
    c.setOutlineColor(col);
    c.setOutlineThickness(brushSize);
    rt_.draw(c);
}

// ---------------------------------------------------------------------------
sf::Image Canvas::getImage() const
{
    return rt_.getTexture().copyToImage();
}

const sf::Texture& Canvas::getCommittedTexture() const
{
    return rt_.getTexture();
}

sf::Vector2u Canvas::getSize() const
{
    return rt_.getSize();
}