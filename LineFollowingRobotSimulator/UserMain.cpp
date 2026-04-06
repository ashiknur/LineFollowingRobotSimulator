#include "UserAPI.hpp"
#include "UserCode.hpp"
#include "SharedState.hpp"


void userMain()
{
    setup();
    while (true)
    {
        loop();
    }
}
