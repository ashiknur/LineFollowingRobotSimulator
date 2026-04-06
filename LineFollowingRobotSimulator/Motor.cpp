#include "Motor.hpp"
#include <algorithm>

void Motor::setSpeed(float s)
{
    speed = std::clamp(s, -255.f, 255.f);
}

float Motor::getSpeed() const
{
    return speed;
}
