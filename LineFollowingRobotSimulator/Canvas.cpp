#include "Canvas.hpp"
#include "config.hpp"
#include <cmath>

Canvas::Canvas()
    : canvas(sf::Vector2u{ WIDTH, HEIGHT })
{
    canvas.clear(sf::Color::White);
    canvas.display();
}

void Canvas::handleDrawing(
    bool drawing,
    const std::vector<sf::Vector2f>& points
)
{
    if (!drawing || points.size() < 2)
        return;

    sf::CircleShape start(LINE_WIDTH / 2.f);
    start.setOrigin(sf::Vector2f{ LINE_WIDTH / 2.f, LINE_WIDTH / 2.f });
    start.setFillColor(sf::Color::Black);
    start.setPosition(points[0]);
    canvas.draw(start);

    for (size_t i = 1; i < points.size(); ++i)
    {
        sf::Vector2f p0 = points[i - 1];
        sf::Vector2f p1 = points[i];

        sf::Vector2f d = p1 - p0;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len == 0) continue;

        sf::Vector2f u = d / len;
        sf::Vector2f n(-u.y, u.x);

        sf::Vertex quad[4];
        quad[0].position = p0 + n * (LINE_WIDTH / 2.f);
        quad[1].position = p1 + n * (LINE_WIDTH / 2.f);
        quad[2].position = p1 - n * (LINE_WIDTH / 2.f);
        quad[3].position = p0 - n * (LINE_WIDTH / 2.f);

        for (auto& v : quad) v.color = sf::Color::Black;
        canvas.draw(quad, 4, sf::PrimitiveType::TriangleFan);

        sf::CircleShape joint(LINE_WIDTH / 2.f);
        joint.setOrigin(sf::Vector2f{ LINE_WIDTH / 2.f, LINE_WIDTH / 2.f });
        joint.setFillColor(sf::Color::Black);
        joint.setPosition(p1);
        canvas.draw(joint);
    }

    canvas.display();
}

sf::Image Canvas::getImage() const
{
    return canvas.getTexture().copyToImage();
}

const sf::Texture& Canvas::getTexture() const
{
    return canvas.getTexture();
}
