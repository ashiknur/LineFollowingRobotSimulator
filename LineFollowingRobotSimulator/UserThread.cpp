#include "SharedState.hpp"
#include "UserAPI.hpp"

void userMain();

void userThreadFunc(SharedState* shared)
{
    gShared = shared;
    userMain();
}
