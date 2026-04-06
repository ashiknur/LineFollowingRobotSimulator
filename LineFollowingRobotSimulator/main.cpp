#include <SFML/Graphics.hpp>
#include <thread>
#include<iostream>
#include "Canvas.hpp"
#include "Robot.hpp"
#include "SharedState.hpp"
#include "config.hpp"

void userThreadFunc(SharedState*);

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({ WIDTH, HEIGHT }),
        "Threaded Line Follower"
    );
    window.setFramerateLimit(60);

    Canvas canvas;
    Robot robot;
    SharedState shared;

    bool drawing = true;
    bool userThreadStarted = false;

    std::vector<sf::Vector2f> points;
    std::thread userThread;

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        while (auto e = window.pollEvent())
        {
            if (e->is<sf::Event::Closed>())
                window.close();

            // -------- DRAWING MODE --------
            if (drawing)
            {
                if (e->is<sf::Event::MouseButtonPressed>())
                {
                    auto m = e->getIf<sf::Event::MouseButtonPressed>();
                    if (m->button == sf::Mouse::Button::Left)
                        points.push_back({
                            (float)m->position.x,
                            (float)m->position.y
                            });
                }

                if (e->is<sf::Event::MouseMoved>() &&
                    sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
                {
                    auto m = e->getIf<sf::Event::MouseMoved>();
                    points.push_back({
                        (float)m->position.x,
                        (float)m->position.y
                        });
                }

                if (e->is<sf::Event::KeyPressed>() &&
                    e->getIf<sf::Event::KeyPressed>()->code ==
                    sf::Keyboard::Key::Enter)
                {
                    drawing = false;

                    // START USER THREAD ONCE
                    if (!userThreadStarted)
                    {
                        std::cout << "Starting user thread..." << std::endl;
                        userThreadStarted = true;
                        userThread = std::thread(userThreadFunc, &shared);
                        std::cout << "User thread started." << std::endl;
                    }
                }
            }
        }

        // -------- DRAW CANVAS --------
        canvas.handleDrawing(drawing, points);

        // -------- SIMULATION MODE --------
        if (!drawing)
        {
            // Update sensors for user thread

            {
                std::lock_guard<std::mutex> lock(shared.sensorMutex);
                auto sensorValues = robot.readSensors(canvas.getImage());
                shared.sensors = sensorValues;
            }
            // Apply motor state (latched, thread-safe)
            robot.leftMotor.setSpeed(shared.leftMotor.load());
            robot.rightMotor.setSpeed(shared.rightMotor.load());

            robot.update(dt, canvas.getImage());

        }
        else
        {
            // Drawing mode: robot must NOT move
            robot.leftMotor.setSpeed(0.f);
            robot.rightMotor.setSpeed(0.f);
        }

        // -------- RENDER --------
        window.clear(sf::Color::White);
        window.draw(sf::Sprite(canvas.getTexture()));
        robot.draw(window);
        window.display();
    }

    // -------- CLEAN SHUTDOWN --------
    shared.running = false;
    if (userThreadStarted)
        userThread.join();

    return 0;
}


