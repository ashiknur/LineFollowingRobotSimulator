#include "Robot.hpp"
#include "Math.hpp"
#include "config.hpp"

Robot::Robot()
    : position(100.f, 100.f),
    angle(0.f),
    sensors(30.f, 10.f, 3),
    texture(),                  // texture constructed first
    sprite(texture)             // sprite constructed with texture
{
    if (!texture.loadFromFile("lfr body.png"))
    {
        throw std::runtime_error("Failed to load robot texture");
    }
    sprite.setTexture(texture, true);
    // Robot
    sprite.setTextureRect(
        sf::IntRect(
            sf::Vector2i(0, 0),
            sf::Vector2i(220, 220) // robot img dimension
        )
    );
    sprite.setOrigin(sf::Vector2f{ 110.f, 110.f }); // middle of robot img
    sprite.setScale(sf::Vector2f{ 0.25f, 0.25f });
}


std::vector<int> Robot::readSensors(const sf::Image& img)
{
    return sensors.readSensors(img, position, angle);
}

void Robot::update(float dt, const sf::Image& img)
{
    float avg = (leftMotor.getSpeed() + rightMotor.getSpeed()) * 0.5f;
    float rot = (rightMotor.getSpeed() - leftMotor.getSpeed()) * 0.05f;

    angle += rot * dt * (180.f / 3.1415926f);

    position.x += avg * std::cos(deg2rad(angle)) * dt;
    position.y += avg * std::sin(deg2rad(angle)) * dt;

    position.x = std::clamp(position.x, 0.f, (float)WIDTH);
    position.y = std::clamp(position.y, 0.f, (float)HEIGHT);
}

void Robot::draw(sf::RenderWindow& window)
{
    sprite.setPosition(position);
    sprite.setRotation(sf::degrees(angle + 90));
    window.draw(sprite);

    for (auto& s : sensors.getPositions())
    {
        sf::CircleShape c(3.f);
        c.setOrigin({ 3.f, 3.f });
        c.setPosition(s);
        c.setFillColor(sf::Color::Green);
        window.draw(c);
    }
}
