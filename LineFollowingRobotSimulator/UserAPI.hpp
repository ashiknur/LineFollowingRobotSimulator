#pragma once
#include <vector>

struct SharedState;   // forward declaration

extern SharedState* gShared;

std::vector<int> readSensor();
void setMotorSpeed(float left, float right);
void delayMs(int ms);
