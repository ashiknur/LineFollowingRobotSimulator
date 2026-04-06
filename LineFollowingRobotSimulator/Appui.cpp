#include "AppUI.hpp"
#include "config.hpp"
#include <imgui-SFML.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr float TOOLS_W = 172.f;
static constexpr float CODE_W = 430.f;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static ImU32 sfColorToIM(sf::Color c)
{
    return IM_COL32(c.r, c.g, c.b, 200);
}

// ---------------------------------------------------------------------------
AppUI::AppUI(Canvas& canvas, Robot& robot,
    SharedState& shared, sf::RenderWindow& window)
    : canvas_(canvas), robot_(robot), shared_(shared), window_(window)
{
    sceneRT_.resize({ WIDTH, HEIGHT });

    // ── TextEditor setup ───────────────────────────────────────────────────
    auto lang = TextEditor::LanguageDefinition::CPlusPlus();
    editor_.SetLanguageDefinition(lang);
    editor_.SetShowWhitespaces(false);
    editor_.SetTabSize(4);

    // Dark palette that fits inside both light and dark ImGui themes
    auto palette = TextEditor::GetDarkPalette();
    editor_.SetPalette(palette);

    loadCode();
}

// ---------------------------------------------------------------------------
bool AppUI::render()
{
    firstRun_ = false;

    const float ww = static_cast<float>(window_.getSize().x);
    const float wh = static_cast<float>(window_.getSize().y);
    const float cw = ww - TOOLS_W - CODE_W;   // canvas panel width

    buildScene();

    // Common flags: no chrome, pinned, no bring-to-front
    const ImGuiWindowFlags WF =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // ── LEFT  – Drawing Tools ────────────────────────────────────────────
    ImGui::SetNextWindowPos({ 0,                0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ TOOLS_W,         wh }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 10.f });
    ImGui::Begin("##tools", nullptr, WF);
    renderToolsPanel(TOOLS_W, wh);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── CENTER – Canvas ──────────────────────────────────────────────────
    ImGui::SetNextWindowPos({ TOOLS_W,          0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ cw,              wh }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::Begin("##canvas", nullptr, WF);
    renderCanvasPanel(cw, wh);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── RIGHT – Code Editor ──────────────────────────────────────────────
    ImGui::SetNextWindowPos({ TOOLS_W + cw,     0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ CODE_W,          wh }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 8.f });
    ImGui::Begin("##code", nullptr, WF);
    renderCodePanel(CODE_W, wh);
    ImGui::End();
    ImGui::PopStyleVar();

    return firstRun_;
}

// ===========================================================================
// Scene compositor
// ===========================================================================
void AppUI::buildScene()
{
    sceneRT_.clear(sf::Color::White);
    sf::Sprite cs(canvas_.getCommittedTexture());
    sceneRT_.draw(cs);
    robot_.draw(sceneRT_);
    sceneRT_.display();
}

// ===========================================================================
// PANEL: Drawing Tools
// ===========================================================================
void AppUI::renderToolsPanel(float w, float /*h*/)
{
    const float btnW = w - 16.f;

    // ── Tool heading ────────────────────────────────────────────────────────
    ImGui::TextDisabled("Drawing Tools");
    ImGui::Separator();
    ImGui::Spacing();

    // Helper: highlighted button when that tool is active
    auto toolBtn = [&](const char* label, DrawTool t)
        {
            bool active = (canvas_.tool == t);
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            if (ImGui::Button(label, { btnW, 30.f }))
                canvas_.tool = t;
            if (active)
                ImGui::PopStyleColor(2);
        };

    toolBtn("✏  Freehand", DrawTool::Freehand);
    toolBtn("⬜ Eraser", DrawTool::Eraser);
    toolBtn("╱  Straight Line", DrawTool::StraightLine);
    toolBtn("▪  Filled Rect", DrawTool::FilledRect);
    toolBtn("○  Circle Border", DrawTool::CircleBorder);

    // ── Color ───────────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Color");
    ImGui::Spacing();

    float halfW = (btnW - 4.f) / 2.f;

    // Black button
    bool blk = (canvas_.penColor == sf::Color::Black);
    ImGui::PushStyleColor(ImGuiCol_Button,
        blk ? ImVec4{ 0.08f,0.08f,0.08f,1.f } : ImVec4{ 0.25f,0.25f,0.25f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.08f,0.08f,0.08f,1.f });
    ImGui::PushStyleColor(ImGuiCol_Text, { 1.f,1.f,1.f,1.f });
    if (ImGui::Button("Black", { halfW, 30.f }))
        canvas_.penColor = sf::Color::Black;
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 4.f);

    // White button
    bool wht = (canvas_.penColor == sf::Color::White);
    ImGui::PushStyleColor(ImGuiCol_Button,
        wht ? ImVec4{ 0.98f,0.98f,0.98f,1.f } : ImVec4{ 0.75f,0.75f,0.75f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.98f,0.98f,0.98f,1.f });
    ImGui::PushStyleColor(ImGuiCol_Text, { 0.1f,0.1f,0.1f,1.f });
    if (ImGui::Button("White", { halfW, 30.f }))
        canvas_.penColor = sf::Color::White;
    ImGui::PopStyleColor(3);

    // ── Brush size ──────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Brush size");
    ImGui::SetNextItemWidth(btnW);
    ImGui::SliderFloat("##brush", &canvas_.brushSize, 2.f, 50.f, "%.0f px");

    // ── Clear canvas ────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, { 0.65f,0.15f,0.15f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.80f,0.20f,0.20f,1.f });
    if (ImGui::Button("Clear canvas", { btnW, 30.f }))
        canvas_.clear();
    ImGui::PopStyleColor(2);

    // ── Status strip ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* toolName = "?";
    switch (canvas_.tool)
    {
    case DrawTool::Freehand:    toolName = "Freehand";    break;
    case DrawTool::Eraser:      toolName = "Eraser";      break;
    case DrawTool::StraightLine:toolName = "Line";        break;
    case DrawTool::FilledRect:  toolName = "Filled Rect"; break;
    case DrawTool::CircleBorder:toolName = "Circle";      break;
    }
    ImGui::TextDisabled("Tool: %s", toolName);

    bool paused = shared_.paused.load();
    bool running = simStarted_ && !paused;
    if (!simStarted_)
        ImGui::TextDisabled("Sim: stopped");
    else if (paused)
        ImGui::TextDisabled("Sim: paused");
    else
        ImGui::TextColored({ 0.3f,0.9f,0.3f,1.f }, "Sim: running");
}

// ===========================================================================
// PANEL: Canvas
// ===========================================================================
void AppUI::renderCanvasPanel(float panelW, float panelH)
{
    sf::Vector2u ts = sceneRT_.getSize();

    // Scale uniformly to fit panel
    float scale = std::min(panelW / (float)ts.x, panelH / (float)ts.y);
    ImVec2 disp = { ts.x * scale, ts.y * scale };

    float padX = (panelW - disp.x) / 2.f;
    float padY = (panelH - disp.y) / 2.f;
    ImGui::SetCursorPos({ padX, padY });

    // Record screen-space origin for coordinate conversion
    canvasOrigin_ = ImGui::GetCursorScreenPos();
    canvasDisp_ = disp;

    // imgui-sfml overload – handles Y-flip automatically
    ImGui::Image(sceneRT_, sf::Vector2f{ disp.x, disp.y });

    bool hovered = ImGui::IsItemHovered();
    bool mouseNow = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImVec2 mp = ImGui::GetMousePos();

    if (hovered)
    {
        sf::Vector2f cp = screenToCanvas(mp);
        if (mouseNow && !prevMouse_)
            canvas_.mousePressed(cp);
        else if (mouseNow && prevMouse_)
            canvas_.mouseMoved(cp);
    }

    // Release even if cursor drifted outside the image
    if (!mouseNow && prevMouse_ && canvas_.isDragging())
        canvas_.mouseReleased(screenToCanvas(mp));

    prevMouse_ = mouseNow && hovered;

    // ── Preview overlay for shape tools ────────────────────────────────────
    if (canvas_.isDragging() &&
        canvas_.tool != DrawTool::Freehand &&
        canvas_.tool != DrawTool::Eraser)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      s = canvasToScreen(canvas_.getDragStart());
        ImVec2      cur = canvasToScreen(canvas_.getDragCur());
        ImU32       col = sfColorToIM(canvas_.penColor);
        float       sw = canvas_.brushSize * scale; // brush size in screen px

        switch (canvas_.tool)
        {
        case DrawTool::StraightLine:
            dl->AddLine(s, cur, col, sw);
            break;

        case DrawTool::FilledRect:
            dl->AddRectFilled(
                { std::min(s.x, cur.x), std::min(s.y, cur.y) },
                { std::max(s.x, cur.x), std::max(s.y, cur.y) },
                col);
            break;

        case DrawTool::CircleBorder: {
            float dx = cur.x - s.x, dy = cur.y - s.y;
            float rScreen = std::sqrt(dx * dx + dy * dy);
            dl->AddCircle(s, rScreen, col, 64, sw);
            break;
        }

        default: break;
        }
    }
}

// ===========================================================================
// PANEL: Code Editor
// ===========================================================================
void AppUI::renderCodePanel(float panelW, float panelH)
{
    const float btnH = 30.f;
    const float btnW = 100.f;
    const float sep = 6.f;

    // ── Run / Pause / Resume ────────────────────────────────────────────────
    if (!simStarted_)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
            { 0.18f, 0.65f, 0.28f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            { 0.25f, 0.80f, 0.35f, 1.f });
        if (ImGui::Button("▶  Run", { btnW, btnH }))
        {
            saveCode();
            simStarted_ = true;
            shared_.paused = false;
            firstRun_ = true;
        }
        ImGui::PopStyleColor(2);
    }
    else
    {
        bool paused = shared_.paused.load();
        if (paused)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                { 0.18f, 0.65f, 0.28f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                { 0.25f, 0.80f, 0.35f, 1.f });
            if (ImGui::Button("▶  Resume", { btnW, btnH }))
                shared_.paused = false;
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                { 0.70f, 0.48f, 0.08f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                { 0.85f, 0.60f, 0.10f, 1.f });
            if (ImGui::Button("⏸  Pause", { btnW, btnH }))
                shared_.paused = true;
            ImGui::PopStyleColor(2);
        }
    }

    // ── Save ─────────────────────────────────────────────────────────────────
    ImGui::SameLine(0.f, sep);
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.20f,0.40f,0.75f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.28f,0.50f,0.90f,1.f });
    if (ImGui::Button("💾 Save", { btnW, btnH }))
        saveCode();
    ImGui::PopStyleColor(2);

    // ── Reload from disk ─────────────────────────────────────────────────────
    ImGui::SameLine(0.f, sep);
    if (ImGui::Button("↺ Reload", { btnW, btnH }))
        loadCode();

    // ── Line / column display ────────────────────────────────────────────────
    ImGui::SameLine();
    auto cpos = editor_.GetCursorPosition();
    ImGui::TextDisabled("  Ln %d  Col %d", cpos.mLine + 1, cpos.mColumn + 1);

    ImGui::Separator();

    // ── Editor (fills remaining height) ──────────────────────────────────────
    float edH = panelH - btnH - 14.f; // 14 = separator + spacing
    editor_.Render("##code_editor",
        ImVec2{ panelW - 16.f, edH });
}

// ===========================================================================
// File I/O
// ===========================================================================
void AppUI::loadCode()
{
    std::ifstream f("UserCode.cpp");
    if (f.is_open())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        editor_.SetText(ss.str());
    }
    else
    {
        // Fallback – pre-fill with PID template so the editor isn't empty
        editor_.SetText(
            "#include \"UserAPI.hpp\"\n"
            "#include \"UserCode.hpp\"\n"
            "#include <algorithm>\n\n"
            "static float Kp = 1.0f, Ki = 0.0f, Kd = 0.2f;\n"
            "static float integral = 0.f, prevError = 0.f;\n"
            "static constexpr float BASE_SPEED = 60.f;\n\n"
            "void setup()\n{\n"
            "    // one-time init here\n"
            "}\n\n"
            "void loop()\n{\n"
            "    auto s = readSensor();\n"
            "    float error = (s[2] ? 1.f : 0.f) - (s[0] ? 1.f : 0.f);\n"
            "    float dt = 0.02f;\n"
            "    integral  += error * dt;\n"
            "    float derivative = error - prevError;\n"
            "    float correction  = Kp*error + Ki*integral + Kd*derivative;\n"
            "    prevError = error;\n"
            "    float left  = std::clamp(BASE_SPEED - correction*50.f, 10.f, 255.f);\n"
            "    float right = std::clamp(BASE_SPEED + correction*50.f, 10.f, 255.f);\n"
            "    setMotorSpeed(left, right);\n"
            "}\n"
        );
    }
}

void AppUI::saveCode() const
{
    std::ofstream f("UserCode.cpp");
    if (f.is_open())
        f << editor_.GetText();
}

// ===========================================================================
// Coordinate helpers
// ===========================================================================
sf::Vector2f AppUI::screenToCanvas(ImVec2 p) const
{
    float cx = (p.x - canvasOrigin_.x) / canvasDisp_.x * (float)WIDTH;
    float cy = (p.y - canvasOrigin_.y) / canvasDisp_.y * (float)HEIGHT;
    return {
        std::clamp(cx, 0.f, (float)WIDTH),
        std::clamp(cy, 0.f, (float)HEIGHT)
    };
}

ImVec2 AppUI::canvasToScreen(sf::Vector2f p) const
{
    return {
        canvasOrigin_.x + p.x / (float)WIDTH * canvasDisp_.x,
        canvasOrigin_.y + p.y / (float)HEIGHT * canvasDisp_.y
    };
}