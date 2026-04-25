#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <thread>
#include <iostream>

#include "Canvas.hpp"
#include "Robot.hpp"
#include "SharedState.hpp"
#include "HotReload.hpp"
#include "config.hpp"
#include "AppUI.hpp"

// Defined in UserThread.cpp
void userThreadFunc(SharedState*, HotReload*);

// ---------------------------------------------------------------------------
int main()
{
    const unsigned WIN_W = WIDTH + static_cast<unsigned>(172 + 430);
    const unsigned WIN_H = HEIGHT;

    sf::RenderWindow window(
        sf::VideoMode({ WIN_W, WIN_H }),
        "Line Follower Simulator"
    );
    window.setFramerateLimit(60);

    // ── ImGui-SFML init ─────────────────────────────────────────────────────
    if (!ImGui::SFML::Init(window))
    {
        std::cerr << "Failed to initialize ImGui-SFML\n";
        return 1;
    }

    // ── Style tweaks ─────────────────────────────────────────────────────────
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 1.f;
    style.FrameRounding = 4.f;
    style.ItemSpacing = { 8.f, 5.f };
    style.ScrollbarSize = 10.f;
    ImGui::StyleColorsDark();

    // ── Simulation objects ───────────────────────────────────────────────────
    Canvas      canvas;
    Robot       robot;
    SharedState shared;

    // HotReload manages compiling UserCode.cpp into a shared library at
    // runtime and loading it via dlopen/LoadLibrary.
    // sourceFile: path to UserCode.cpp (relative to CWD = project source dir)
    // libOut:     base path for the generated .dll/.so (no extension)
    HotReload hotreload("UserCode.cpp", "./usercode_hot");
    hotreload.setSharedPtr(&shared);

    AppUI ui(canvas, robot, shared, hotreload, window);

    bool        userThreadStarted = false;
    std::thread userThread;
    sf::Clock   deltaClock;

    // ── Main loop ────────────────────────────────────────────────────────────
    while (window.isOpen())
    {
        sf::Time dt = deltaClock.restart();

        while (auto ev = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *ev);
            if (ev->is<sf::Event::Closed>())
                window.close();
        }

        ImGui::SFML::Update(window, dt);

        // render() returns true on the single frame the sim thread should start
        bool startSim = ui.render();

        // Spawn user thread exactly once, after the initial compile succeeds
        if (startSim && !userThreadStarted)
        {
            std::cout << "Starting simulation thread...\n";
            userThreadStarted = true;
            userThread = std::thread(userThreadFunc, &shared, &hotreload);
        }

        // ── Simulation step (skipped when paused or thread not yet started) ──
        if (userThreadStarted && !shared.paused.load())
        {
            {
                std::lock_guard<std::mutex> lock(shared.sensorMutex);
                shared.sensors = robot.readSensors(canvas.getImage());
            }
            robot.leftMotor.setSpeed(shared.leftMotor.load());
            robot.rightMotor.setSpeed(shared.rightMotor.load());
            robot.update(dt.asSeconds(), canvas.getImage());
        }

        // ── Render ────────────────────────────────────────────────────────
        window.clear({ 35, 35, 38, 255 });
        ImGui::SFML::Render(window);
        window.display();
    }

    // ── Clean shutdown ────────────────────────────────────────────────────
    shared.running = false;
    shared.paused = false;   // wake thread if sleeping
    if (userThreadStarted)
        userThread.join();

    ImGui::SFML::Shutdown();
    return 0;
}