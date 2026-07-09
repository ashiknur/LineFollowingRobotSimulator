// LfrHostApi.h
//
// The ONLY thing shared between the main exe and the hot-reloaded user DLL.
//
// This header is deliberately pure C: the exe is built with MSVC while the
// user DLL may be built by a completely different compiler (the bundled
// MinGW/Clang toolchain). C++ types (std::vector, std::mutex, ...) are NOT
// ABI-compatible across those compilers, so nothing C++ may ever cross this
// boundary — only plain data and C function pointers.
//
// The exe fills in this table (see HostApi.cpp) and passes it to the DLL via
// the exported lfr_setHostApi() right after LoadLibrary.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LfrHostApi
{
    // Opaque pointer owned by the exe; passed back to every callback.
    void* ctx;

    // Fills out[0..maxCount) with the current sensor readings.
    // Returns the number of values actually written.
    int  (*readSensors)(void* ctx, int* out, int maxCount);

    void (*setMotorSpeed)(void* ctx, float left, float right);
} LfrHostApi;

#ifdef __cplusplus
} // extern "C"
#endif
