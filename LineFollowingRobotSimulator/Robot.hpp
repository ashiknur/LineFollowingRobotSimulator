#pragma once
#include <SFML/Graphics.hpp>
#include "Sensor.hpp"
#include "Motor.hpp"

class Robot
{
public:
    Robot();

    void update(float dt, const sf::Image& img);
    void draw(sf::RenderWindow& window);

    std::vector<int> readSensors(const sf::Image& img);

    Motor leftMotor;
    Motor rightMotor;

private:
    sf::Vector2f position;
    float angle;

    Sensor sensors;

    sf::Texture texture;
    sf::Sprite sprite;
};
