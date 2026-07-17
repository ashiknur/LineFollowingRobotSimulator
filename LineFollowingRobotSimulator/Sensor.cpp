#include "Sensor.hpp"
#include "config.hpp"
#include "Math.hpp"
#include <algorithm>

Sensor::Sensor(float offset, float spacing, int count)
    : sensorOffset(offset), sensorSpacing(spacing), sensorCount(count)
{}

int Sensor::readGray(const sf::Image& img, sf::Vector2i p)
{
    p.x = std::clamp(p.x, 0, (int)WIDTH - 1);
    p.y = std::clamp(p.y, 0, (int)HEIGHT - 1);

    sf::Color c = img.getPixel(
        sf::Vector2u{ (unsigned)p.x, (unsigned)p.y }
    );
    return (c.r + c.g + c.b) / 3;
}

std::vector<int> Sensor::readSensors(
    const sf::Image& img,
    sf::Vector2f robotPos,
    float robotAngle
)
{
    positions.clear();
    std::vector<int> digital;

    float forward = deg2rad(robotAngle);
    float perp = forward + 3.1415926f / 2.f;

    // Center the array on the robot's forward axis. (The old
    // "for i in [-count/2, count/2]" loop emitted count+1 sensors for even
    // counts.) s[0] stays the leftmost sensor.
    for (int i = 0; i < sensorCount; ++i)
    {
        float lateral = (i - (sensorCount - 1) * 0.5f) * sensorSpacing;

        sf::Vector2f pos{
            robotPos.x + std::cos(forward) * sensorOffset +
            std::cos(perp) * lateral,

            robotPos.y + std::sin(forward) * sensorOffset +
            std::sin(perp) * lateral
        };

        positions.push_back(pos);

        int gray = readGray(img, sf::Vector2i{
            (int)pos.x, (int)pos.y
            });

        digital.push_back(gray < THRESHOLD ? 1 : 0);
    }

    return digital;
}

const std::vector<sf::Vector2f>& Sensor::getPositions() const
{
    return positions;
}
