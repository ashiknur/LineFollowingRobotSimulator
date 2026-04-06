#include "UserCode.hpp"
#include "UserAPI.hpp"
#include <iostream>


// PID constants
static float Kp = 1.0f;
static float Ki = 0.0f;
static float Kd = 0.2f;

// PID state
static float integral = 0.f;
static float prevError = 0.f;

// Control loop interval (ms)
static constexpr float LOOP_DELAY_MS = 200.f; // 50 Hz
static constexpr float BASE_SPEED = 60.f;

void setup()
{
    // Initialization code here
    std::cout << "Line Follower Robot Initialized" << std::endl;
}

void loop()
{
    // Main control loop code here
    auto s = readSensor();

    //std::cout << "Motor Started " << s.size() << std::endl;
    //if (s.size() >= 3)
    //{
        // Line-following error (right - left)
    float error = (s[2] ? 1.f : 0.f) - (s[0] ? 1.f : 0.f);

    // PID math (dt derived from delay)
    float dt = LOOP_DELAY_MS / 1000.f;
    integral += error * dt;
    float derivative = error - prevError;

    float correction =
        Kp * error +
        Ki * integral +
        Kd * derivative;

    prevError = error;

    float left = BASE_SPEED - correction * 50.f;
    float right = BASE_SPEED + correction * 50.f;
    left = std::clamp(left, 10.f, 255.f);
    right = std::clamp(right, 10.f, 255.f);
    setMotorSpeed(left, right);
    //}
    //else
    //{
        // Safety stop if sensors invalid
        //setMotorSpeed(0.f, 0.f);
    //}
    // Non-blocking for simulation (robot keeps moving)
    //delayMs((int)LOOP_DELAY_MS);

}