#pragma once

class Motor
{
public:
    void setSpeed(float s);
    float getSpeed() const;

private:
    float speed = 0.f;
};
