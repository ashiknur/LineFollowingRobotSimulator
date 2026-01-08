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
constexpr float LINE_WIDTH = 8.f; // adjust thickness here


// ---------------- SENSOR ----------------
float sensorOffset = 30.f;
float sensorSpacing = 10.f;

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

    // ---------- ROBOT SPRITE ----------
    sf::Texture robotTexture;
    if (!robotTexture.loadFromFile("D:\\Final Year Project\\LineFollowingRobotSimulator\\lfr body.png"))
    {
        std::cerr << "Failed to load LFR body.png\n";
        return -1;
    }

    const int FRAME_WIDTH = 220;
    const int FRAME_HEIGHT = 220;
    int currentFrame = 0;
    float animationTimer = 0.f;
    float animationSpeed = 0.1f; // seconds per frame

    sf::Sprite robotSprite(robotTexture);
    robotSprite.setOrigin(sf::Vector2f{ FRAME_WIDTH / 2.f, FRAME_HEIGHT / 2.f });
    robotSprite.setScale(sf::Vector2f{ 0.25f, 0.25f });
    //robotSprite.setRotation(sf::degrees(90));


    // Line canvas
    sf::RenderTexture canvas(sf::Vector2u{ WIDTH, HEIGHT });
    canvas.clear(sf::Color::White);
    canvas.display();

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        //if (!drawing)
        //{
            //animationTimer += dt;
            //if (animationTimer >= animationSpeed)
            //{
                //animationTimer = 0.f;
                //currentFrame = (currentFrame + 1) % 4;
            //}
        //}
        currentFrame = 0;

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
            for (size_t i = 1; i < linePoints.size(); ++i)
            {
                sf::Vector2f p0 = linePoints[i - 1];
                sf::Vector2f p1 = linePoints[i];

                sf::Vector2f direction = p1 - p0;
                float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                if (length == 0) continue;

                sf::Vector2f unitDir = direction / length;
                sf::Vector2f normal(-unitDir.y, unitDir.x);

                sf::Vertex quad[4];
                quad[0].position = p0 + normal * (LINE_WIDTH * 0.5f);
                quad[1].position = p1 + normal * (LINE_WIDTH * 0.5f);
                quad[2].position = p1 - normal * (LINE_WIDTH * 0.5f);
                quad[3].position = p0 - normal * (LINE_WIDTH * 0.5f);

                for (auto& v : quad)
                    v.color = sf::Color::Black;

                canvas.draw(quad, 4, sf::PrimitiveType::TriangleFan);
            }
            canvas.display();

        }



        // ---------- SENSOR POSITIONS ----------
        std::vector<sf::Vector2f> sensors;

        float forwardAngle = deg2rad(robotAngle);
        float perpendicularAngle = forwardAngle + 3.1415926f / 2.f;

        for (float offset : {-sensorSpacing, 0.f, sensorSpacing})
        {
            sensors.push_back({
                robotPos.x + std::cos(forwardAngle) * sensorOffset +
                std::cos(perpendicularAngle) * offset,

                robotPos.y + std::sin(forwardAngle) * sensorOffset +
                std::sin(perpendicularAngle) * offset
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

            //std::cout << "Sensor " << i
                //<< " Gray=" << gray
                //<< " Digital=" << d << "\n";
        }

        // ---------- PID ----------
        if (!drawing)
        {
            float error = (digital[2] ? 1.f : 0.f) -
                (digital[0] ? 1.f : 0.f);

            integral += error * dt;
            float derivative = (error - prevError);
            float correction = Kp * error + Ki * integral + Kd * derivative;
            prevError = error;

            float left = baseSpeed - correction * 50.f;
            float right = baseSpeed + correction * 50.f;

            float avg = (left + right) * 0.5f;
            float rot = (right - left) * 0.05f;

            robotAngle += rot * dt * (180.f / 3.1415926f); // convert to degrees;
            if (robotAngle > 360.f) robotAngle -= 360.f;
            if (robotAngle < 0.f)   robotAngle += 360.f;
            robotPos.x += avg * std::cos(deg2rad(robotAngle)) * dt;
            robotPos.y += avg * std::sin(deg2rad(robotAngle)) * dt;
            std::cout << "Error: " << error << " Correction: " << correction << " Left: " << left << " Right: " << right << "\n";
        }

        //std::cout << "---------------------------\n";

        // ---------- RENDER ----------
        window.clear(sf::Color::White);
        window.draw(sf::Sprite(canvas.getTexture()));

        // Robot
        robotSprite.setTextureRect(
            sf::IntRect(
                sf::Vector2i(currentFrame * FRAME_WIDTH, 0),
                sf::Vector2i(FRAME_WIDTH, FRAME_HEIGHT)
            )
        );

        robotSprite.setPosition(robotPos);
        float rta = robotAngle + 90;
        if (rta >= 360.f) rta -= 360.f;
        if (rta < 0.f)   rta += 360.f;
        robotSprite.setRotation(sf::degrees(rta));
        std::cout << std::fixed << std::setprecision(0) << robotAngle << " " << rta << " " << robotPos.x << " " << robotPos.y << std::endl;
        window.draw(robotSprite);


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
