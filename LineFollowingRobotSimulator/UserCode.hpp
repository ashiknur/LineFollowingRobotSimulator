#pragma once
#include <vector>
#include "Robot.hpp"

void userLoop(
    Robot& robot,
    const std::vector<int>& sensors,
    float dt
);
