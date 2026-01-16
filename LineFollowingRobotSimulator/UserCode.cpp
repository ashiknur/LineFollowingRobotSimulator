#include "UserCode.hpp"

static float Kp = 1.0f;
static float Ki = 0.0f;
static float Kd = 0.2f;

static float integral = 0.f;
static float prevError = 0.f;

void userLoop(
    Robot& robot,
    const std::vector<int>& s,
    float dt
)
{
    float error = (s[2] ? 1.f : 0.f) - (s[0] ? 1.f : 0.f);

    integral += error * dt;
    float derivative = error - prevError;

    float correction = Kp * error + Ki * integral + Kd * derivative;
    prevError = error;

    robot.leftMotor.setSpeed(60.f - correction * 50);
    robot.rightMotor.setSpeed(60.f + correction * 50);
    
}
