#include "SharedState.hpp"
#include "UserAPI.hpp"
#include "HotReload.hpp"

// Forward declaration – defined in UserMain.cpp
void userMain(SharedState* shared, HotReload* hotreload);

void userThreadFunc(SharedState* shared, HotReload* hotreload)
{
    userMain(shared, hotreload);
}