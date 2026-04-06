#include "Robot.hpp"
#include "Math.hpp"
#include "config.hpp"

Robot::Robot()
    : position(100.f, 100.f),
    angle(0.f),
    sensors(30.f, 10.f, 3),
    texture(),
    sprite(texture)
{
    if (!texture.loadFromFile("lfr body.png"))
        throw std::runtime_error("Failed to load robot texture");

    sprite.setTexture(texture, true);
    sprite.setTextureRect(
        sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(220, 220))
    );
    sprite.setOrigin(sf::Vector2f{ 110.f, 110.f });
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

// Now accepts sf::RenderTarget& (works for both Window and RenderTexture)
void Robot::draw(sf::RenderTarget& target)
{
    sprite.setPosition(position);
    sprite.setRotation(sf::degrees(angle + 90.f));
    target.draw(sprite);

    for (auto& s : sensors.getPositions())
    {
        sf::CircleShape c(3.f);
        c.setOrigin({ 3.f, 3.f });
        c.setPosition(s);
        c.setFillColor(sf::Color::Green);
        target.draw(c);
    }
}