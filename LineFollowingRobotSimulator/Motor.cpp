#include "Motor.hpp"
#include <algorithm>

void Motor::setSpeed(float s)
{
    speed = s;
}

float Motor::getSpeed() const
{
    return speed;
}
