#include "SharedState.hpp"
#include "UserAPI.hpp"
#include "HotReload.hpp"

// Forward declaration – defined in UserMain.cpp
void userMain(SharedState* shared, HotReload* hotreload);

void userThreadFunc(SharedState* shared, HotReload* hotreload)
{
    // gShared is still needed by the static UserAPI.cpp (non-hotreload path
    // used elsewhere, e.g. for any future non-DLL callers).
    gShared = shared;

    userMain(shared, hotreload);
}