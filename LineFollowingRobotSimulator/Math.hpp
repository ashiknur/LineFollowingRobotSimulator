#pragma once
#include <cmath>

inline float deg2rad(float d)
{
    return d * 3.1415926f / 180.f;
}

inline float rad2deg(float r)
{
    return r * 180.f / 3.1415926f;
}
