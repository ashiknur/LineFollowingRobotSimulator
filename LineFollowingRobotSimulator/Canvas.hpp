#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class Canvas
{
public:
    Canvas();

    void handleDrawing(
        bool drawing,
        const std::vector<sf::Vector2f>& points
    );

    sf::Image getImage() const;
    const sf::Texture& getTexture() const;

private:
    sf::RenderTexture canvas;
};
