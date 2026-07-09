#pragma once
#include "LfrHostApi.h"

struct SharedState;

// Builds the C callback table the hot-reloaded user DLL uses to talk to the
// simulator. The callbacks operate on SharedState with the exe's own STL, so
// no C++ types ever cross the DLL boundary.
LfrHostApi makeHostApi(SharedState* shared);
