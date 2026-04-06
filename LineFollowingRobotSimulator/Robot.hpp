#pragma once
#include <SFML/Graphics.hpp>
#include <stdexcept>
#include "Motor.hpp"
#include "Sensor.hpp"

class Robot
{
public:
    Robot();

    Motor leftMotor;
    Motor rightMotor;

    std::vector<int> readSensors(const sf::Image& img);
    void update(float dt, const sf::Image& img);

    // Accepts sf::RenderTarget so it works for both
    // sf::RenderWindow (original) and sf::RenderTexture (scene compositor).
    void draw(sf::RenderTarget& target);

    sf::Vector2f getPosition() const { return position; }
    float        getAngle()    const { return angle; }

private:
    sf::Vector2f position;
    float        angle;

    Sensor        sensors;
    sf::Texture   texture;
    sf::Sprite    sprite;
};