#include "HostApi.hpp"
#include "SharedState.hpp"

#include <mutex>

static int hostReadSensors(void* ctx, int* out, int maxCount)
{
    auto* s = static_cast<SharedState*>(ctx);
    std::lock_guard<std::mutex> lock(s->sensorMutex);

    int n = static_cast<int>(s->sensors.size());
    if (n > maxCount) n = maxCount;
    for (int i = 0; i < n; ++i)
        out[i] = s->sensors[i];
    return n;
}

static void hostSetMotorSpeed(void* ctx, float left, float right)
{
    auto* s = static_cast<SharedState*>(ctx);
    s->leftMotor  = left;
    s->rightMotor = right;
}

LfrHostApi makeHostApi(SharedState* shared)
{
    LfrHostApi api{};
    api.ctx           = shared;
    api.readSensors   = hostReadSensors;
    api.setMotorSpeed = hostSetMotorSpeed;
    return api;
}
