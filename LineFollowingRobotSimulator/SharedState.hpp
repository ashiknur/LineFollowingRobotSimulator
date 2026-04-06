#pragma once
#include <vector>
#include <mutex>
#include <atomic>

struct SharedState
{
    std::vector<int>   sensors;

    std::atomic<float> leftMotor{ 0.f };
    std::atomic<float> rightMotor{ 0.f };

    std::atomic<bool>  running{ true };
    std::atomic<bool>  paused{ false };   // ← new: freeze robot physics

    std::mutex sensorMutex;
};