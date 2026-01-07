#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>

// ---------------- CONSTANTS ----------------
constexpr unsigned WIDTH = 700;
constexpr unsigned HEIGHT = 700;
constexpr int THRESHOLD = 120;

// ---------------- ROBOT ----------------
sf::Vector2f robotPos{ 100.f, 100.f };
float robotAngle = 0.f;
float robotRadius = 6.f;
float baseSpeed = 60.f;   // pixels per second

// ---------------- PID ----------------
float Kp = 1.0f;
float Ki = 0.0f;
float Kd = 0.2f;
float integral = 0.f;
float prevError = 0.f;

// ---------------- STATE ----------------
bool drawing = true;
std::vector<sf::Vector2f> linePoints;

// ---------------- SENSOR ----------------
float sensorOffset = 15.f;
float sensorSpacing = 15.f;

// ---------------- HELPERS ----------------
float deg2rad(float d) { return d * 3.1415926f / 180.f; }

// Read pixel grayscale
int readGray(const sf::Image& img, sf::Vector2i p)
{
    p.x = std::clamp(p.x, 0, (int)WIDTH - 1);
    p.y = std::clamp(p.y, 0, (int)HEIGHT - 1);

    sf::Color c = img.getPixel(sf::Vector2u{ (unsigned)p.x, (unsigned)p.y });
    return (c.r + c.g + c.b) / 3;
}

// ---------------- MAIN ----------------
int main()
{
    sf::RenderWindow window(
        sf::VideoMode{ sf::Vector2u{WIDTH, HEIGHT} },
        "SFML 3 – PID Line Follower"
    );

    window.setFramerateLimit(60);

    // Line canvas
    sf::RenderTexture canvas(sf::Vector2u{ WIDTH, HEIGHT });
    canvas.clear(sf::Color::White);
    canvas.display();

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // ---------- EVENTS ----------
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (drawing)
            {
                if (event->is<sf::Event::MouseButtonPressed>())
                {
                    auto m = event->getIf<sf::Event::MouseButtonPressed>();
                    if (m->button == sf::Mouse::Button::Left)
                        linePoints.push_back(sf::Vector2f{ (float)m->position.x, (float)m->position.y });
                }

                if (event->is<sf::Event::MouseMoved>() &&
                    sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
                {
                    auto m = event->getIf<sf::Event::MouseMoved>();
                    linePoints.push_back(sf::Vector2f{ (float)m->position.x, (float)m->position.y });
                }

                if (event->is<sf::Event::KeyPressed>())
                {
                    if (event->getIf<sf::Event::KeyPressed>()->code ==
                        sf::Keyboard::Key::Enter)
                    {
                        drawing = false;
                        std::cout << "--- ROBOT STARTED ---\n";
                    }
                }
            }
        }

        // ---------- DRAW LINE ----------
        if (drawing && linePoints.size() > 1)
        {
            sf::VertexArray strip(sf::PrimitiveType::LineStrip, linePoints.size());
            for (size_t i = 0; i < linePoints.size(); ++i)
            {
                strip[i].position = linePoints[i];
                strip[i].color = sf::Color::Black;
            }
            canvas.draw(strip);
            canvas.display();
        }

        // ---------- SENSOR POSITIONS ----------
        std::vector<sf::Vector2f> sensors;
        for (float s : {-sensorSpacing, 0.f, sensorSpacing})
        {
            float a = deg2rad(robotAngle + s);
            sensors.push_back({
                robotPos.x + sensorOffset * std::cos(a),
                robotPos.y + sensorOffset * std::sin(a)
                });
        }

        // ---------- SENSOR READ ----------
        sf::Image img = canvas.getTexture().copyToImage();
        std::vector<int> digital;

        for (size_t i = 0; i < sensors.size(); ++i)
        {
            int gray = readGray(img, sf::Vector2i{
                (int)sensors[i].x,
                (int)sensors[i].y
                });


            int d = gray < THRESHOLD ? 1 : 0;
            digital.push_back(d);

            std::cout << "Sensor " << i
                << " Gray=" << gray
                << " Digital=" << d << "\n";
        }

        // ---------- PID ----------
        if (!drawing)
        {
            float error = (digital[2] ? 1.f : 0.f) -
                (digital[0] ? 1.f : 0.f);

            integral += error * dt;
            float derivative = (error - prevError) / dt;
            float correction = Kp * error + Ki * integral + Kd * derivative;
            prevError = error;

            float left = baseSpeed - correction * 50.f;
            float right = baseSpeed + correction * 50.f;

            float avg = (left + right) * 0.5f;
            float rot = (right - left) * 0.05f;

            robotAngle += rot;
            robotPos.x += avg * std::cos(deg2rad(robotAngle)) * dt;
            robotPos.y += avg * std::sin(deg2rad(robotAngle)) * dt;
        }

        std::cout << "---------------------------\n";

        // ---------- RENDER ----------
        window.clear(sf::Color::White);
        window.draw(sf::Sprite(canvas.getTexture()));

        // Robot
        sf::CircleShape robot(robotRadius);
        robot.setOrigin(sf::Vector2f{ robotRadius, robotRadius });
        robot.setPosition(robotPos);
        robot.setFillColor(sf::Color::Red);
        window.draw(robot);

        // Sensors
        for (auto& s : sensors)
        {
            sf::CircleShape c(3.f);
            c.setOrigin(sf::Vector2f{ 3.f, 3.f });
            c.setPosition(s);
            c.setFillColor(sf::Color::Green);
            window.draw(c);
        }

        window.display();
    }
}
