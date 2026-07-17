#include "Robot.hpp"
#include "Math.hpp"
#include "config.hpp"
#include "Paths.hpp"

Robot::Robot()
    : position(100.f, 100.f),
    angle(0.f),
    sensors(30.f, 10.f, 3),
    texture(),
    sprite(texture)
{
    // Look next to the exe first (installed app), then the cwd (VS debugger)
    if (!texture.loadFromFile(paths::exeDir() + "/LFR body.png") &&
        !texture.loadFromFile("LFR body.png"))
        throw std::runtime_error("Failed to load robot texture");

    sprite.setTexture(texture, true);
    sprite.setTextureRect(
        sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(220, 220))
    );
    sprite.setOrigin(sf::Vector2f{ 110.f, 110.f });
    sprite.setScale(sf::Vector2f{ 0.25f, 0.25f });
}

std::vector<int> Robot::readSensors(const sf::Image& img)
{
    return sensors.readSensors(img, position, angle);
}

void Robot::setSensorCount(int n)
{
    sensors.setCount(std::clamp(n, 2, 15));
}

int Robot::getSensorCount() const
{
    return sensors.getCount();
}

void Robot::update(float dt, const sf::Image& img)
{
    // Differential-drive kinematics. Wheel speeds are px/s; the robot's
    // center moves with the average speed and turns with
    //   omega = (vL - vR) / WHEEL_BASE   [rad/s]
    // This makes the turning center land where physics says it should:
    //   vR = 0        -> pivot exactly on the right wheel
    //   vL = -vR      -> spin in place about the robot's center
    float vL = leftMotor.getSpeed();
    float vR = rightMotor.getSpeed();

    float v = 0.5f * (vL + vR);
    float omega = (vL - vR) / WHEEL_BASE;

    // Midpoint integration: advance along the heading at the middle of this
    // step's rotation, so arcs (and wheel pivots) stay on the true circle
    // instead of drifting outward.
    float dAngle = omega * dt;
    float midHeading = deg2rad(angle) + dAngle * 0.5f;

    position.x += v * std::cos(midHeading) * dt;
    position.y += v * std::sin(midHeading) * dt;
    angle += rad2deg(dAngle);

    position.x = std::clamp(position.x, 0.f, (float)WIDTH);
    position.y = std::clamp(position.y, 0.f, (float)HEIGHT);
}

// Now accepts sf::RenderTarget& (works for both Window and RenderTexture)
void Robot::draw(sf::RenderTarget& target)
{
    sprite.setPosition(position);
    sprite.setRotation(sf::degrees(angle + 90.f));
    target.draw(sprite);

    for (auto& s : sensors.getPositions())
    {
        sf::CircleShape c(3.f);
        c.setOrigin({ 3.f, 3.f });
        c.setPosition(s);
        c.setFillColor(sf::Color::Green);
        target.draw(c);
    }
}