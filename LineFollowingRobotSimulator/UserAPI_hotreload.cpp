// UserAPI_hotreload.cpp
//
// Compiled ONLY into the hot-reload shared library (guarded by LFR_HOTRELOAD).
// Provides readSensor() / setMotorSpeed() / delayMs() for UserCode.cpp,
// forwarding through a SharedState* injected at load time.

#ifdef LFR_HOTRELOAD

#include "UserAPI.hpp"
#include "SharedState.hpp"
#include <thread>
#include <chrono>

#if defined(_WIN32)
#  define LFR_EXPORT __declspec(dllexport)
#else
#  define LFR_EXPORT
#endif

// Local pointer – set by lfr_injectShared() which is called from
// Usercodeadapter.cpp right after the library is loaded.
static SharedState* gSharedLocal = nullptr;

extern "C"
{
    // Must be visible to Usercodeadapter.cpp (also in this shared lib)
    LFR_EXPORT void lfr_injectShared(void* ptr)
    {
        gSharedLocal = static_cast<SharedState*>(ptr);
    }
}

// ── UserAPI implementation for the hot-reload DLL ───────────────────────────

std::vector<int> readSensor()
{
    if (!gSharedLocal) return {};
    std::lock_guard<std::mutex> lock(gSharedLocal->sensorMutex);
    return gSharedLocal->sensors;
}

void setMotorSpeed(float left, float right)
{
    if (!gSharedLocal) return;
    gSharedLocal->leftMotor = left;
    gSharedLocal->rightMotor = right;
}

void delayMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif // LFR_HOTRELOAD