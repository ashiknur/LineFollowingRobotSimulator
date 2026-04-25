// Usercodeadapter.cpp
//
// Compiled ONLY into the hot-reload shared library (usercode_hot_N.dll/.so).
// The main exe must NOT define LFR_HOTRELOAD, so this entire file becomes
// empty when Visual Studio compiles it as part of the main project.
//
// HotReload::compile() passes -DLFR_HOTRELOAD on its compiler command line
// when building the shared lib, which activates this file.

#ifdef LFR_HOTRELOAD

#include "UserCode.hpp"   // void setup(); void loop();
#include "UserAPI.hpp"

// On Windows the DLL symbols must be explicitly exported; on Linux/macOS
// all symbols are visible by default when building with -shared.
#if defined(_WIN32)
#  define LFR_EXPORT __declspec(dllexport)
#else
#  define LFR_EXPORT
#endif

// Declared in UserAPI_hotreload.cpp (compiled into the same shared lib)
extern "C" void lfr_injectShared(void* ptr);

extern "C"
{
    // Called by HotReload::load() right after dlopen/LoadLibrary to hand
    // over the SharedState* so the UserAPI functions can reach it.
    LFR_EXPORT void lfr_setShared(void* ptr)
    {
        lfr_injectShared(ptr);
    }

    LFR_EXPORT void lfr_setup() { setup(); }
    LFR_EXPORT void lfr_loop() { loop(); }
}

#endif // LFR_HOTRELOAD