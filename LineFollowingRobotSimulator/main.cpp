#include <SFML/Graphics.hpp>
#include "Canvas.hpp"
#include "Robot.hpp"
#include "UserCode.hpp"
#include "config.hpp"

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({ WIDTH, HEIGHT }),
        "PID Line Follower Simulator"
    );
    window.setFramerateLimit(60);

    Canvas canvas;
    Robot robot;

    bool drawing = true;
    std::vector<sf::Vector2f> points;
    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        while (auto e = window.pollEvent())
        {
            if (e->is<sf::Event::Closed>())
                window.close();

            if (drawing)
            {
                if (e->is<sf::Event::MouseButtonPressed>())
                {
                    auto m = e->getIf<sf::Event::MouseButtonPressed>();
                    if (m->button == sf::Mouse::Button::Left)
                        points.push_back(
                            { (float)m->position.x, (float)m->position.y }
                        );
                }

                if (e->is<sf::Event::MouseMoved>() &&
                    sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
                {
                    auto m = e->getIf<sf::Event::MouseMoved>();
                    points.push_back(
                        { (float)m->position.x, (float)m->position.y }
                    );
                }

                if (e->is<sf::Event::KeyPressed>() &&
                    e->getIf<sf::Event::KeyPressed>()->code ==
                    sf::Keyboard::Key::Enter)
                {
                    drawing = false;
                }
            }
        }

        canvas.handleDrawing(drawing, points);

        if (!drawing)
        {
            auto sensors = robot.readSensors(canvas.getImage());
            userLoop(robot, sensors, dt);
            robot.update(dt, canvas.getImage());
        }

        window.clear(sf::Color::White);
        window.draw(sf::Sprite(canvas.getTexture()));
        robot.draw(window);
        window.display();
    }
}
