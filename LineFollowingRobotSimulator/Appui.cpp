#include "AppUI.hpp"
#include "config.hpp"
#include "Paths.hpp"
#include <imgui-SFML.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <chrono>

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
    SharedState& shared, HotReload& hotreload,
    sf::RenderWindow& window)
    : canvas_(canvas), robot_(robot),
    shared_(shared), hotreload_(hotreload),
    window_(window)
{
    sceneRT_.resize({ WIDTH, HEIGHT });

    // ── TextEditor setup ───────────────────────────────────────────────────
    auto lang = TextEditor::LanguageDefinition::CPlusPlus();
    editor_.SetLanguageDefinition(lang);
    editor_.SetShowWhitespaces(false);
    editor_.SetTabSize(4);

    auto palette = TextEditor::GetDarkPalette();
    editor_.SetPalette(palette);

    loadCode();
}

AppUI::~AppUI()
{
    // If a compile is still running when the window closes, wait for it.
    if (compileThread_.joinable())
        compileThread_.join();
}

// ---------------------------------------------------------------------------
// startCompile – launches an async compile on a background thread.
//
// forRun  = true  → called from the Run button; on success sets
//                   pendingThreadStart_ so the main loop spawns the thread.
// forRun  = false → called from Compile & Reload; on success the user thread
//                   picks up HotReload::reloadPending and calls setup() once.
// ---------------------------------------------------------------------------
void AppUI::startCompile(bool forRun)
{
    // Join any previous compile thread before starting a new one
    if (compileThread_.joinable())
        compileThread_.join();

    compileStatus_ = 1;   // Compiling
    {
        std::lock_guard<std::mutex> lk(compileOutputMutex_);
        compileOutput_.clear();
    }
    scrollCompileLog_ = false;

    // Pause the simulation so the user thread is not inside callLoop()
    // while we unload/reload the shared library.
    shared_.paused = true;

    compileThread_ = std::thread([this, forRun]
        {
            // Give the user thread a brief moment to finish any in-progress
            // callLoop() invocation and enter its sleep-while-paused branch.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            auto res = hotreload_.compile();

            // Write output under lock so the render thread can safely read it
            {
                std::lock_guard<std::mutex> lk(compileOutputMutex_);
                compileOutput_ = res.output;
                scrollCompileLog_ = true;
            }

            if (res.success)
            {
                compileStatus_ = 2;   // Success
                if (forRun)
                {
                    // Signal main loop to spawn the user thread
                    pendingThreadStart_ = true;
                    // Keep paused = true until the thread is actually running;
                    // the thread itself will proceed once unpaused in render().
                }
                else
                {
                    // Hot-reload: user thread will call setup() once via
                    // reloadPending, then resume looping.
                    shared_.paused = false;
                }
            }
            else
            {
                compileStatus_ = 3;   // Failed
                // Leave sim paused so the user can read the error and fix the code.
                // They can click Resume (or Compile & Reload again) when ready.
            }
        });
}

// ---------------------------------------------------------------------------
bool AppUI::render()
{
    firstRun_ = false;

    // ── Pending thread start (compile for initial Run succeeded) ─────────────
    if (pendingThreadStart_.exchange(false))
    {
        simStarted_ = true;
        compilingForRun_ = false;
        firstRun_ = true;       // main.cpp will spawn the user thread
        shared_.paused = false;   // unblock the thread once it starts
    }

    const float ww = static_cast<float>(window_.getSize().x);
    const float wh = static_cast<float>(window_.getSize().y);
    const float cw = ww - TOOLS_W - CODE_W;

    buildScene();

    const ImGuiWindowFlags WF =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // ── LEFT  – Drawing Tools ─────────────────────────────────────────────
    ImGui::SetNextWindowPos({ 0,       0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ TOOLS_W, wh }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 10.f });
    ImGui::Begin("##tools", nullptr, WF);
    renderToolsPanel(TOOLS_W, wh);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── CENTER – Canvas ───────────────────────────────────────────────────
    ImGui::SetNextWindowPos({ TOOLS_W,     0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ cw,          wh }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::Begin("##canvas", nullptr, WF);
    renderCanvasPanel(cw, wh);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── RIGHT – Code Editor ───────────────────────────────────────────────
    ImGui::SetNextWindowPos({ TOOLS_W + cw, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ CODE_W,       wh }, ImGuiCond_Always);
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

    ImGui::TextDisabled("Drawing Tools");
    ImGui::Separator();
    ImGui::Spacing();

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

    bool blk = (canvas_.penColor == sf::Color::Black);
    ImGui::PushStyleColor(ImGuiCol_Button,
        blk ? ImVec4{ 0.08f,0.08f,0.08f,1.f } : ImVec4{ 0.25f,0.25f,0.25f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.08f,0.08f,0.08f,1.f });
    ImGui::PushStyleColor(ImGuiCol_Text, { 1.f,1.f,1.f,1.f });
    if (ImGui::Button("Black", { halfW, 30.f }))
        canvas_.penColor = sf::Color::Black;
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 4.f);

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

    // ── Robot starting position ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Robot Start Position");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("Pos X##startX", &startingPosX_, 0.f, 0.f, "%.1f"))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);

    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("Pos Y##startY", &startingPosY_, 0.f, 0.f, "%.1f"))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);

    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("Angle##startA", &startingAngle_, 0.f, 0.f, "%.1f deg"))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.20f, 0.45f, 0.70f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.28f, 0.55f, 0.85f, 1.f });
    if (ImGui::Button("Reset Robot Pos", { btnW, 30.f }))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);
    ImGui::PopStyleColor(2);

    // ── Status strip ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* toolName = "?";
    switch (canvas_.tool)
    {
    case DrawTool::Freehand:     toolName = "Freehand";    break;
    case DrawTool::Eraser:       toolName = "Eraser";      break;
    case DrawTool::StraightLine: toolName = "Line";        break;
    case DrawTool::FilledRect:   toolName = "Filled Rect"; break;
    case DrawTool::CircleBorder: toolName = "Circle";      break;
    }
    ImGui::TextDisabled("Tool: %s", toolName);

    int cs = compileStatus_.load();
    bool paused = shared_.paused.load();
    if (!simStarted_ && !compilingForRun_)
        ImGui::TextDisabled("Sim: stopped");
    else if (compilingForRun_ || cs == 1)
        ImGui::TextColored({ 1.f, 0.8f, 0.2f, 1.f }, "Sim: compiling...");
    else if (cs == 3)
        ImGui::TextColored({ 1.f, 0.4f, 0.4f, 1.f }, "Sim: compile failed");
    else if (paused)
        ImGui::TextDisabled("Sim: paused");
    else if (simStarted_)
        ImGui::TextColored({ 0.3f, 0.9f, 0.3f, 1.f }, "Sim: running");
}

// ===========================================================================
// PANEL: Canvas
// ===========================================================================
void AppUI::renderCanvasPanel(float panelW, float panelH)
{
    sf::Vector2u ts = sceneRT_.getSize();

    float scale = std::min(panelW / (float)ts.x, panelH / (float)ts.y);
    ImVec2 disp = { ts.x * scale, ts.y * scale };

    float padX = (panelW - disp.x) / 2.f;
    float padY = (panelH - disp.y) / 2.f;
    ImGui::SetCursorPos({ padX, padY });

    canvasOrigin_ = ImGui::GetCursorScreenPos();
    canvasDisp_ = disp;

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

    if (!mouseNow && prevMouse_ && canvas_.isDragging())
        canvas_.mouseReleased(screenToCanvas(mp));

    prevMouse_ = mouseNow && hovered;

    // ── Preview overlay for shape tools ────────────────────────────────────
    if (canvas_.isDragging() &&
        canvas_.tool != DrawTool::Freehand &&
        canvas_.tool != DrawTool::Eraser)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 s = canvasToScreen(canvas_.getDragStart());
        ImVec2 cur = canvasToScreen(canvas_.getDragCur());
        ImU32  col = sfColorToIM(canvas_.penColor);
        float  sw = canvas_.brushSize * scale;

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

    int  cs = compileStatus_.load();
    bool compiling = (cs == 1);

    // ── Row 1: Run / Pause / Resume  +  Save  +  Reload ──────────────────
    if (!simStarted_)
    {
        if (compilingForRun_)
        {
            // Show a disabled "Compiling…" stand-in where Run used to be
            ImGui::BeginDisabled();
            ImGui::Button("⏳ Compiling", { btnW, btnH });
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                { 0.18f, 0.65f, 0.28f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                { 0.25f, 0.80f, 0.35f, 1.f });
            if (ImGui::Button("▶  Run", { btnW, btnH }))
            {
                saveCode();
                compilingForRun_ = true;
                startCompile(/*forRun=*/true);
            }
            ImGui::PopStyleColor(2);
        }
    }
    else
    {
        bool paused = shared_.paused.load();
        if (paused && !compiling)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                { 0.18f, 0.65f, 0.28f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                { 0.25f, 0.80f, 0.35f, 1.f });
            if (ImGui::Button("▶  Resume", { btnW, btnH }))
                shared_.paused = false;
            ImGui::PopStyleColor(2);
        }
        else if (compiling)
        {
            ImGui::BeginDisabled();
            ImGui::Button("⏳ Compiling", { btnW, btnH });
            ImGui::EndDisabled();
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

    // ── Save ──────────────────────────────────────────────────────────────
    ImGui::SameLine(0.f, sep);
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.20f,0.40f,0.75f,1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.28f,0.50f,0.90f,1.f });
    if (ImGui::Button("💾 Save", { btnW, btnH }))
        saveCode();
    ImGui::PopStyleColor(2);

    // ── Reload from disk ──────────────────────────────────────────────────
    ImGui::SameLine(0.f, sep);
    if (ImGui::Button("↺ Reload", { btnW, btnH }))
        loadCode();

    // ── Line / column display ─────────────────────────────────────────────
    ImGui::SameLine();
    auto cpos = editor_.GetCursorPosition();
    ImGui::TextDisabled("  Ln %d  Col %d", cpos.mLine + 1, cpos.mColumn + 1);

    // ── Row 2: Compile & Reload button ────────────────────────────────────
    ImGui::Spacing();

    if (compiling) ImGui::BeginDisabled();

    ImGui::PushStyleColor(ImGuiCol_Button, { 0.50f, 0.18f, 0.70f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.62f, 0.25f, 0.85f, 1.f });

    const float compileW = panelW - 16.f;  // full width
    bool doCompile = ImGui::Button("⚙  Compile & Reload", { compileW, btnH });
    ImGui::PopStyleColor(2);

    if (compiling) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !compiling)
        ImGui::SetTooltip(
            "Saves, recompiles UserCode.cpp into a shared library,\n"
            "and hot-reloads it — no need to restart the app.");

    if (doCompile && simStarted_ && !compiling)
    {
        saveCode();
        startCompile(/*forRun=*/false);
    }

    // ── Compile status badge ──────────────────────────────────────────────
    ImGui::Spacing();
    switch (cs)
    {
    case 0:  ImGui::TextDisabled("No compile yet"); break;
    case 1:  ImGui::TextColored({ 1.f, 0.85f, 0.2f,  1.f }, "⏳ Compiling..."); break;
    case 2:  ImGui::TextColored({ 0.3f, 0.9f, 0.3f,  1.f }, "✓  Compile succeeded – code reloaded"); break;
    case 3:  ImGui::TextColored({ 1.f,  0.4f, 0.35f, 1.f }, "✗  Compile failed – see output below"); break;
    }

    // ── Compile output log ────────────────────────────────────────────────
    const float logH = 120.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.f });
    ImGui::BeginChild("##compileLog", { compileW, logH },
        ImGuiChildFlags_Border);

    {
        // Copy under lock to avoid race with compile thread
        std::string outputCopy;
        {
            std::lock_guard<std::mutex> lk(compileOutputMutex_);
            outputCopy = compileOutput_;
            bool doScroll = scrollCompileLog_;
            if (doScroll) scrollCompileLog_ = false;  // consumed

            if (!outputCopy.empty())
                ImGui::TextUnformatted(outputCopy.c_str(),
                    outputCopy.c_str() + outputCopy.size());
            else
                ImGui::TextDisabled("(compile output will appear here)");

            if (doScroll)
                ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ── Editor (fills remaining height) ──────────────────────────────────
    // Heights consumed above the editor:
    //   btnH (row1) + spacing + btnH (row2) + spacing + lineH (status) +
    //   logH + separator + some padding ≈ 14px
    const float consumedH = btnH + 4.f + btnH + 4.f + ImGui::GetTextLineHeightWithSpacing()
        + 4.f + logH + 14.f;
    float edH = panelH - consumedH;
    if (edH < 80.f) edH = 80.f;

    editor_.Render("##code_editor", ImVec2{ panelW - 16.f, edH });
}

// ===========================================================================
// File I/O
// ===========================================================================
void AppUI::loadCode()
{
    std::ifstream f(paths::userCodePath());
    if (f.is_open())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        editor_.SetText(ss.str());
    }
    else
    {
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
    std::ofstream f(paths::userCodePath());
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