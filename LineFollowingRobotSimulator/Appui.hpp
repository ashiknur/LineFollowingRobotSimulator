#pragma once
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <string>

#include "Canvas.hpp"
#include "Robot.hpp"
#include "SharedState.hpp"

// ImGuiColorTextEdit – drop TextEditor.h / TextEditor.cpp from
// https://github.com/BalazsJako/ImGuiColorTextEdit into your source tree.
#include "TextEditor.hpp"

// ---------------------------------------------------------------------------
// Three-panel ImGui UI:
//   [Drawing Tools] | [Canvas + robot scene] | [Code Editor]
// ---------------------------------------------------------------------------
class AppUI
{
public:
    AppUI(Canvas& canvas,
        Robot& robot,
        SharedState& shared,
        sf::RenderWindow& window);

    // Call once per frame AFTER ImGui::SFML::Update().
    // Returns true on the single frame the user first clicks "Run"
    // (so main.cpp knows to spawn the user thread exactly once).
    bool render();

private:
    // ── References ──────────────────────────────────────────────────────────
    Canvas& canvas_;
    Robot& robot_;
    SharedState& shared_;
    sf::RenderWindow& window_;

    // ── Scene composite texture (canvas + robot rendered together) ───────────
    sf::RenderTexture sceneRT_;

    // ── Code editor ─────────────────────────────────────────────────────────
    TextEditor editor_;

    // ── State ────────────────────────────────────────────────────────────────
    bool simStarted_ = false;  // has Run been pressed at least once?
    bool firstRun_ = false;  // true for exactly one frame when Run is pressed

    // ── Panel renderers ──────────────────────────────────────────────────────
    void renderToolsPanel(float w, float h);
    void renderCanvasPanel(float w, float h);
    void renderCodePanel(float w, float h);

    // ── Scene helper ─────────────────────────────────────────────────────────
    void buildScene();

    // ── Canvas mouse tracking ────────────────────────────────────────────────
    ImVec2 canvasOrigin_ = { 0.f, 0.f };  // top-left of displayed image (screen)
    ImVec2 canvasDisp_ = { 0.f, 0.f };  // displayed size (screen pixels)
    bool   prevMouse_ = false;

    sf::Vector2f screenToCanvas(ImVec2 p) const;
    ImVec2       canvasToScreen(sf::Vector2f p) const;

    // ── File helpers ─────────────────────────────────────────────────────────
    void loadCode();
    void saveCode() const;
};