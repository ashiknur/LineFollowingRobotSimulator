// UserAPI_hotreload.cpp
//
// Compiled ONLY into the hot-reload shared library (guarded by LFR_HOTRELOAD).
// Provides readSensor() / setMotorSpeed() / delayMs() for UserCode.cpp,
// forwarding through the C callback table (LfrHostApi) injected at load time.
//
// The DLL may be built by a different compiler than the main exe, so it must
// never touch the exe's C++ objects directly — everything goes through the
// plain C function pointers in LfrHostApi. The std::vector returned by
// readSensor() is created here, with THIS compiler's STL, and never crosses
// the DLL boundary.

#ifdef LFR_HOTRELOAD

#include "UserAPI.hpp"
#include "LfrHostApi.h"
#include <thread>
#include <chrono>

#if defined(_WIN32)
#  define LFR_EXPORT __declspec(dllexport)
#else
#  define LFR_EXPORT
#endif

// Local copy of the host callback table – set by lfr_injectHostApi() which is
// called from Usercodeadapter.cpp right after the library is loaded.
static LfrHostApi gHost{};

extern "C"
{
    // Must be visible to Usercodeadapter.cpp (also in this shared lib)
    LFR_EXPORT void lfr_injectHostApi(const LfrHostApi* api)
    {
        if (api) gHost = *api;
    }
}

// ── UserAPI implementation for the hot-reload DLL ───────────────────────────

std::vector<int> readSensor()
{
    if (!gHost.readSensors) return {};
    int buf[64];
    int n = gHost.readSensors(gHost.ctx, buf, 64);
    if (n < 0) n = 0;
    return std::vector<int>(buf, buf + n);
}

void setMotorSpeed(float left, float right)
{
    if (gHost.setMotorSpeed)
        gHost.setMotorSpeed(gHost.ctx, left, right);
}

void delayMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif // LFR_HOTRELOAD
