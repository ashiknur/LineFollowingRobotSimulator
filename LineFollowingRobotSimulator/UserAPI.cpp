#include "UserAPI.hpp"
#include "SharedState.hpp"
#include <thread>
#include <chrono>

SharedState* gShared = nullptr;   // definition\

std::vector<int> readSensor()
{
    std::lock_guard<std::mutex> lock(gShared->sensorMutex);
    return gShared->sensors;
}

void setMotorSpeed(float left, float right)
{
    gShared->leftMotor = left;
    gShared->rightMotor = right;
}

void delayMs(int ms)
{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(ms)
    );
}
