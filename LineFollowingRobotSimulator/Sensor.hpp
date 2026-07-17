#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class Sensor
{
public:
    Sensor(float offset, float spacing, int count);

    std::vector<int> readSensors(
        const sf::Image& img,
        sf::Vector2f robotPos,
        float robotAngle
    );

    const std::vector<sf::Vector2f>& getPositions() const;

    void setCount(int n) { sensorCount = n; }
    int  getCount() const { return sensorCount; }

    void  setSpacing(float s) { sensorSpacing = s; }
    float getSpacing() const { return sensorSpacing; }

private:
    int sensorCount;
    float sensorOffset;
    float sensorSpacing;

    std::vector<sf::Vector2f> positions;

    int readGray(const sf::Image& img, sf::Vector2i p);
};
