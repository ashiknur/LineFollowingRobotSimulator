#include "AppUI.hpp"
#include "config.hpp"
#include "Paths.hpp"
#include "Math.hpp"
#include <cstdio>
#include <imgui-SFML.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <chrono>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <commdlg.h>
#  pragma comment(lib, "comdlg32.lib")
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static ImU32 sfColorToIM(sf::Color c)
{
    return IM_COL32(c.r, c.g, c.b, 200);
}

// Native PNG open/save dialog. Returns "" if the user cancels.
static std::string pngFileDialog(sf::RenderWindow& window, bool save)
{
#if defined(_WIN32)
    char file[MAX_PATH] = "track.png";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window.getNativeHandle();
    ofn.lpstrFilter = "PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "png";
    ofn.Flags = OFN_NOCHANGEDIR |
        (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));

    BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    return ok ? std::string(file) : std::string();
#else
    (void)window; (void)save;
    return {};
#endif
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

    applyTheme();
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
        beginRun();               // the scored run starts with the sim
    }

    updateRunTracking();

    const float menuH = renderMenuBar();
    handleGlobalShortcuts();

    const float ww = static_cast<float>(window_.getSize().x);
    const float wh = static_cast<float>(window_.getSize().y);
    const float ph = wh - menuH;

    // Clamp panel widths so every panel keeps a usable minimum
    toolsW_ = std::clamp(toolsW_, 120.f, 320.f);
    codeW_ = std::clamp(codeW_, 280.f, std::max(280.f, ww - toolsW_ - 250.f));
    const float cw = ww - toolsW_ - codeW_;

    buildScene();

    const ImGuiWindowFlags WF =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // ── LEFT  – Drawing Tools ─────────────────────────────────────────────
    ImGui::SetNextWindowPos({ 0,       menuH }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ toolsW_, ph }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 10.f });
    ImGui::Begin("##tools", nullptr, WF);
    renderToolsPanel(toolsW_, ph);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── CENTER – Canvas ───────────────────────────────────────────────────
    ImGui::SetNextWindowPos({ toolsW_,     menuH }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ cw,          ph }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::Begin("##canvas", nullptr, WF);
    renderCanvasPanel(cw, ph);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── RIGHT – Code Editor ───────────────────────────────────────────────
    ImGui::SetNextWindowPos({ toolsW_ + cw, menuH }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ codeW_,       ph }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 8.f });
    ImGui::Begin("##code", nullptr, WF);
    renderCodePanel(codeW_, ph);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── Panel splitters (drawn last, on top) ──────────────────────────────
    // splitterActive_ is consumed by the panels NEXT frame; while a drag is
    // in progress the canvas and editor must not react to the mouse.
    splitterActive_ = false;
    renderSplitter("##split_tools", toolsW_, menuH, ph, toolsW_,
        /*invert=*/false, 120.f, 320.f);
    renderSplitter("##split_code", ww - codeW_, menuH, ph, codeW_,
        /*invert=*/true, 280.f, std::max(280.f, ww - toolsW_ - 250.f));

    renderReplacePopup();
    if (statsOpen_)
        renderStatsWindow();

    // ── Track open/save error modal ───────────────────────────────────────
    if (!trackIoError_.empty() && !ImGui::IsPopupOpen("Track Error"))
        ImGui::OpenPopup("Track Error");
    if (ImGui::BeginPopupModal("Track Error", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(trackIoError_.c_str());
        if (ImGui::Button("OK", { 90.f, 0.f }))
        {
            trackIoError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return firstRun_;
}

// ===========================================================================
// Menu bar, theme, splitters, shortcuts
// ===========================================================================
float AppUI::renderMenuBar()
{
    float h = 0.f;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Track..."))
            {
                std::string p = pngFileDialog(window_, /*save=*/false);
                if (!p.empty() && !canvas_.loadFrom(p))
                    trackIoError_ = "Could not open: " + p;
            }
            if (ImGui::MenuItem("Save Track As..."))
            {
                std::string p = pngFileDialog(window_, /*save=*/true);
                if (!p.empty() && !canvas_.saveTo(p))
                    trackIoError_ = "Could not save: " + p;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo Code", "Ctrl+Z", false, editor_.CanUndo()))
                editor_.Undo();
            if (ImGui::MenuItem("Redo Code", "Ctrl+Shift+Z", false, editor_.CanRedo()))
                editor_.Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Undo Canvas", "Ctrl+Z", false, canvas_.canUndo()))
                canvas_.undo();
            if (ImGui::MenuItem("Redo Canvas", "Ctrl+Shift+Z", false, canvas_.canRedo()))
                canvas_.redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Toggle Comment", "Ctrl+/"))
                editor_.ToggleComments();
            if (ImGui::MenuItem("Edit All Occurrences...", "Ctrl+D", false,
                editor_.HasSelection()))
                openReplacePopup_ = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Stats"))
        {
            ImGui::MenuItem("Statistics Panel", nullptr, &statsOpen_);
            if (ImGui::MenuItem("Place Checkpoint with Mouse", nullptr,
                &cpPlaceMode_))
            {
                if (cpPlaceMode_) statsOpen_ = true;
            }
            ImGui::Separator();
            bool canSkip = simStarted_ && curCp_ + 1 < (int)checkpoints_.size();
            if (ImGui::MenuItem("Skip to Next Checkpoint", "Ctrl+K", false, canSkip))
                doSkip();
            if (ImGui::MenuItem("Restart from Checkpoint", "Ctrl+R", false, simStarted_))
                doRestartCp();
            if (ImGui::MenuItem("New Run", nullptr, false, simStarted_))
                newRun();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Dark Theme", nullptr, darkTheme_))
            {
                darkTheme_ = true; applyTheme();
            }
            if (ImGui::MenuItem("Light Theme", nullptr, !darkTheme_))
            {
                darkTheme_ = false; applyTheme();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Editor Text Size");
            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("##fontscale", &editorFontScale_, 0.8f, 2.0f, "%.1fx");
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                toolsW_ = 172.f; codeW_ = 430.f;
            }
            ImGui::EndMenu();
        }
        h = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }
    return h > 0.f ? h : ImGui::GetFrameHeight();
}

void AppUI::applyTheme()
{
    if (darkTheme_)
    {
        ImGui::StyleColorsDark();
        editor_.SetPalette(TextEditor::GetDarkPalette());
    }
    else
    {
        ImGui::StyleColorsLight();
        editor_.SetPalette(TextEditor::GetLightPalette());
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 1.f;
    style.FrameRounding = 4.f;
    style.ItemSpacing = { 8.f, 5.f };
    style.ScrollbarSize = 10.f;
}

void AppUI::renderSplitter(const char* id, float x, float y, float h,
    float& target, bool invert, float minW, float maxW)
{
    ImGui::SetNextWindowPos({ x - 3.f, y }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ 6.f, h }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin(id, nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::InvisibleButton("##grip", { 6.f, h });
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            { x - 1.f, y }, { x + 1.f, y + h }, IM_COL32(100, 140, 220, 220));
    }
    if (ImGui::IsItemActive())
    {
        splitterActive_ = true;
        float dx = ImGui::GetIO().MouseDelta.x;
        target = std::clamp(target + (invert ? -dx : dx), minW, maxW);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void AppUI::handleGlobalShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // Canvas undo/redo — only when no text widget owns the keyboard.
    // io.WantTextInput covers InputText fields, but the code editor only sets
    // it during its own render (later in the frame), so also check the
    // editor's focus state remembered from the previous frame.
    if (!io.WantTextInput && !editorFocused_ && io.KeyCtrl)
    {
        if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            canvas_.undo();
        else if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            canvas_.redo();
    }

    // Ctrl+D — edit all occurrences of the selected text
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) &&
        editor_.HasSelection())
        openReplacePopup_ = true;

    // Checkpoint shortcuts (not while typing)
    if (!io.WantTextInput && io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_K, false))
            doSkip();
        else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            doRestartCp();
    }

    // Esc leaves checkpoint placement mode.
    // Read the key from SFML rather than ImGui: ImGui never reports Escape in
    // this ImGui-SFML build (other keys, e.g. Ctrl+Z, arrive fine), so
    // IsKeyPressed(ImGuiKey_Escape) would never fire. Edge-detected here
    // because isKeyPressed() is a level, not an event.
    bool escNow = window_.hasFocus() &&
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
    if (cpPlaceMode_ && escNow && !escPrev_ && !io.WantTextInput)
    {
        cpDragging_ = false;
        cpPlaceMode_ = false;
    }
    escPrev_ = escNow;
}

void AppUI::renderReplacePopup()
{
    if (openReplacePopup_)
    {
        findText_ = editor_.GetSelectedText();
        if (!findText_.empty())
        {
            strncpy_s(replaceBuf_, findText_.c_str(), sizeof(replaceBuf_) - 1);
            ImGui::OpenPopup("Edit All Occurrences");
        }
        openReplacePopup_ = false;
    }

    if (ImGui::BeginPopupModal("Edit All Occurrences", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Replace every occurrence of \"%s\" with:", findText_.c_str());
        ImGui::SetNextItemWidth(320.f);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##repl", replaceBuf_, sizeof(replaceBuf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Replace All", { 110.f, 0.f }) || enter)
        {
            editor_.ReplaceAll(findText_, replaceBuf_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 90.f, 0.f }) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ===========================================================================
// Checkpoints & run statistics
// ===========================================================================
void AppUI::beginRun()
{
    curCp_ = -1;
    skips_ = restarts_ = 0;
    runActive_ = true;
    runFinished_ = false;
    stoppedAtEnd_ = false;
    runTime_ = 0.f;
    stillTime_ = 0.f;
    lastRobotX_ = robot_.getPosition().x;
    lastRobotY_ = robot_.getPosition().y;
}

void AppUI::newRun()
{
    robot_.reset(startingPosX_, startingPosY_, startingAngle_);
    beginRun();
    hotreload_.reloadPending = true;   // re-run setup(): fresh controller state
    shared_.paused = false;
}

void AppUI::doSkip()
{
    if (!simStarted_ || curCp_ + 1 >= (int)checkpoints_.size())
        return;
    ++curCp_;
    const Checkpoint& cp = checkpoints_[curCp_];
    robot_.reset(cp.x, cp.y, cp.angle);
    if (runActive_) ++skips_;
    // NOTE: deliberately no setup() re-run here — the controller must keep
    // its state (e.g. a "left the start square" flag) across the teleport.
    stillTime_ = 0.f;
}

void AppUI::doRestartCp()
{
    if (!simStarted_)
        return;
    if (curCp_ >= 0)
    {
        const Checkpoint& cp = checkpoints_[curCp_];
        robot_.reset(cp.x, cp.y, cp.angle);
    }
    else
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);
    if (runActive_) ++restarts_;
    stillTime_ = 0.f;
    if (runFinished_)                  // restarting after a finish resumes the
    {                                  // run; the controller needs a setup()
        runFinished_ = false;          // re-run to clear its finished latch
        runActive_ = true;
        hotreload_.reloadPending = true;
    }
    shared_.paused = false;
}

void AppUI::updateRunTracking()
{
    if (!runActive_ || !simStarted_)
        return;

    float dt = ImGui::GetIO().DeltaTime;
    if (shared_.paused.load())
        return;                        // paused time doesn't count

    runTime_ += dt;

    sf::Vector2f p = robot_.getPosition();

    // Auto-advance when the robot drives near the next checkpoint
    if (curCp_ + 1 < (int)checkpoints_.size())
    {
        const Checkpoint& next = checkpoints_[curCp_ + 1];
        float dx = p.x - next.x, dy = p.y - next.y;
        if (dx * dx + dy * dy < 40.f * 40.f)
            ++curCp_;
    }

    // Finish detection: the robot has been stationary for 2 s
    float moved = std::fabs(p.x - lastRobotX_) + std::fabs(p.y - lastRobotY_);
    lastRobotX_ = p.x; lastRobotY_ = p.y;

    if (moved < 0.05f && runTime_ > 3.f)
    {
        stillTime_ += dt;
        if (stillTime_ >= 2.f)
        {
            runActive_ = false;
            runFinished_ = true;
            runTime_ -= stillTime_;    // don't count the stationary tail
            if (!checkpoints_.empty())
            {
                const Checkpoint& last = checkpoints_.back();
                float dx = p.x - last.x, dy = p.y - last.y;
                stoppedAtEnd_ = (dx * dx + dy * dy < 70.f * 70.f);
            }
            else
                stoppedAtEnd_ = true;  // no checkpoints: any clean stop counts
            statsOpen_ = true;         // pop the results
        }
    }
    else
        stillTime_ = 0.f;
}

float AppUI::computeScore(bool& clean, float& t) const
{
    clean = (skips_ == 0 && restarts_ == 0);
    t = runTime_;
    float score = scoreB_
        - scoreS_ * (float)skips_
        - scoreR_ * (float)restarts_
        - t
        + (clean ? scoreC_ : 0.f)
        + (stoppedAtEnd_ ? scoreSt_ : 0.f);
    return score;
}

void AppUI::renderStatsWindow()
{
    ImGui::SetNextWindowPos({ 240.f, 60.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 380.f, 0.f }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Run Statistics", &statsOpen_,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    // ── Live run state ────────────────────────────────────────────────────
    const char* status = !simStarted_ ? "waiting for Run"
        : runFinished_ ? "FINISHED"
        : runActive_ ? "running" : "-";
    ImGui::Text("Status:      %s", status);
    ImGui::Text("Time:        %.1f s", runTime_);
    ImGui::Text("Skips:       %d", skips_);
    ImGui::Text("Restarts:    %d", restarts_);
    ImGui::Text("Checkpoint:  %d / %d", curCp_ + 1, (int)checkpoints_.size());

    ImGui::Spacing();
    bool canSkip = simStarted_ && curCp_ + 1 < (int)checkpoints_.size();
    if (!simStarted_) ImGui::BeginDisabled();
    if (ImGui::Button("New Run", { 100.f, 0.f })) newRun();
    ImGui::SameLine();
    if (ImGui::Button("Restart (Ctrl+R)", { 130.f, 0.f })) doRestartCp();
    ImGui::SameLine();
    if (!canSkip && simStarted_) ImGui::BeginDisabled();
    if (ImGui::Button("Skip (Ctrl+K)", { 110.f, 0.f })) doSkip();
    if (!canSkip && simStarted_) ImGui::EndDisabled();
    if (!simStarted_) ImGui::EndDisabled();

    // ── Score parameters ──────────────────────────────────────────────────
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Score Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float w = 110.f;
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Base score (b)", &scoreB_, 0.f, 0.f, "%.1f");
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Skip penalty (s)", &scoreS_, 0.f, 0.f, "%.1f");
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Restart penalty (r)", &scoreR_, 0.f, 0.f, "%.1f");
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Clean-run bonus (c)", &scoreC_, 0.f, 0.f, "%.1f");
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Stop-at-end bonus (st)", &scoreSt_, 0.f, 0.f, "%.1f");
        ImGui::TextDisabled("score = b - s*skips - r*restarts - t\n"
            "        + c (no skip/restart) + st (stopped at end)");
    }

    // ── Checkpoints ───────────────────────────────────────────────────────
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Checkpoints", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float w = 64.f;
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("X", &cpX_, 0.f, 0.f, "%.0f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Y", &cpY_, 0.f, 0.f, "%.0f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(w);
        ImGui::InputFloat("Angle", &cpAngle_, 0.f, 0.f, "%.0f");

        if (ImGui::Button("Add Checkpoint", { 130.f, 0.f }))
            checkpoints_.push_back({ cpX_, cpY_, cpAngle_ });
        ImGui::SameLine();
        if (ImGui::Button("Use Robot Pos", { 120.f, 0.f }))
        {
            cpX_ = robot_.getPosition().x;
            cpY_ = robot_.getPosition().y;
            cpAngle_ = robot_.getAngle();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Copy the robot's current position/angle\n"
                "into the fields above");

        // ── Mouse placement ───────────────────────────────────────────────
        if (cpPlaceMode_)
            ImGui::PushStyleColor(ImGuiCol_Button, { 0.15f, 0.55f, 0.85f, 1.f });
        if (ImGui::Button(cpPlaceMode_ ? "Placing... (Esc to stop)"
            : "Place with Mouse", { 256.f, 0.f }))
            cpPlaceMode_ = !cpPlaceMode_;
        if (cpPlaceMode_)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click the canvas to drop a checkpoint, then\n"
                "drag out to aim its direction and release.\n"
                "Right-click a marker to delete it.\n"
                "Esc, this button, or a drawing tool stops.");
        if (cpPlaceMode_)
            ImGui::TextDisabled("Click = point, drag = direction.\n"
                "Right-click a marker deletes it.\n"
                "Drawing is paused while placing.");

        for (int i = 0; i < (int)checkpoints_.size(); ++i)
        {
            ImGui::PushID(i);
            const Checkpoint& cp = checkpoints_[i];
            bool reached = (i <= curCp_);
            ImGui::TextColored(reached
                ? ImVec4{ 0.3f, 0.85f, 0.3f, 1.f }
                : ImVec4{ 0.95f, 0.65f, 0.2f, 1.f },
                "#%d  (%.0f, %.0f)  %.0f deg", i + 1, cp.x, cp.y, cp.angle);
            ImGui::SameLine(230.f);
            if (ImGui::SmallButton("Go"))
            {
                robot_.reset(cp.x, cp.y, cp.angle);
                hotreload_.reloadPending = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Del"))
            {
                checkpoints_.erase(checkpoints_.begin() + i);
                if (curCp_ >= i) --curCp_;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (checkpoints_.empty())
            ImGui::TextDisabled("(none — the last checkpoint you add\n"
                " is treated as the end point)");
    }

    // ── Result ────────────────────────────────────────────────────────────
    if (runFinished_)
    {
        ImGui::Spacing();
        ImGui::Separator();
        bool clean; float t;
        float score = computeScore(clean, t);
        ImGui::Text("Run complete in %.1f s", t);
        ImGui::Text("  base                %+.1f", scoreB_);
        ImGui::Text("  skips     %d x %-5.1f %+.1f", skips_, scoreS_, -scoreS_ * skips_);
        ImGui::Text("  restarts  %d x %-5.1f %+.1f", restarts_, scoreR_, -scoreR_ * restarts_);
        ImGui::Text("  time                %+.1f", -t);
        ImGui::Text("  clean-run bonus     %+.1f", clean ? scoreC_ : 0.f);
        ImGui::Text("  stop-at-end bonus   %+.1f", stoppedAtEnd_ ? scoreSt_ : 0.f);
        ImGui::PushStyleColor(ImGuiCol_Text, { 0.35f, 0.9f, 1.f, 1.f });
        ImGui::Text("SCORE: %.1f", score);
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

// Circle + arrow showing a checkpoint's point and heading
static void drawCpArrow(ImDrawList* dl, ImVec2 c, ImVec2 tip, ImU32 col,
    float thick)
{
    dl->AddCircle(c, 10.f, col, 20, thick);
    dl->AddLine(c, tip, col, thick);

    // Arrow head
    float dx = tip.x - c.x, dy = tip.y - c.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.f)
    {
        dx /= len; dy /= len;
        float hx = -dy, hy = dx;              // perpendicular
        const float hl = 8.f, hw = 4.5f;
        dl->AddTriangleFilled(
            tip,
            { tip.x - dx * hl + hx * hw, tip.y - dy * hl + hy * hw },
            { tip.x - dx * hl - hx * hw, tip.y - dy * hl - hy * hw },
            col);
    }
}

void AppUI::drawCheckpointMarkers()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < (int)checkpoints_.size(); ++i)
    {
        const Checkpoint& cp = checkpoints_[i];
        ImVec2 c = canvasToScreen({ cp.x, cp.y });
        ImU32 col = (i <= curCp_)
            ? IM_COL32(60, 200, 60, 230)
            : IM_COL32(240, 160, 40, 230);
        float a = deg2rad(cp.angle);
        ImVec2 tip = canvasToScreen({ cp.x + 26.f * std::cos(a),
                                      cp.y + 26.f * std::sin(a) });
        drawCpArrow(dl, c, tip, col, 2.5f);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        dl->AddText({ c.x + 10.f, c.y - 18.f }, col, buf);
    }

    // ── Live preview while aiming a new checkpoint with the mouse ──────────
    if (cpPlaceMode_ && cpDragging_)
    {
        ImVec2 c = canvasToScreen(cpDragStart_);
        ImVec2 cur = canvasToScreen(cpDragCur_);
        ImU32  col = IM_COL32(80, 190, 255, 240);

        float dx = cpDragCur_.x - cpDragStart_.x;
        float dy = cpDragCur_.y - cpDragStart_.y;
        bool aimed = (dx * dx + dy * dy >= 64.f);
        float ang = aimed ? rad2deg(std::atan2(dy, dx)) : cpAngle_;

        float a = deg2rad(ang);
        ImVec2 tip = canvasToScreen({ cpDragStart_.x + 26.f * std::cos(a),
                                      cpDragStart_.y + 26.f * std::sin(a) });
        drawCpArrow(dl, c, tip, col, 3.f);
        if (aimed)
            dl->AddLine(c, cur, IM_COL32(80, 190, 255, 110), 1.5f);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f deg", ang);
        dl->AddText({ c.x + 12.f, c.y + 10.f }, col, buf);
    }
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
            {
                canvas_.tool = t;
                cpPlaceMode_ = false;   // picking a draw tool leaves cp mode
            }
            if (active)
                ImGui::PopStyleColor(2);
        };

    toolBtn("✏  Freehand", DrawTool::Freehand);
    toolBtn("⬜ Eraser", DrawTool::Eraser);
    toolBtn("╱  Straight Line", DrawTool::StraightLine);
    toolBtn("▪  Filled Rect", DrawTool::FilledRect);
    toolBtn("○  Circle Border", DrawTool::CircleBorder);

    // Checkpoint placement is a canvas mode like the drawing tools, so it
    // belongs next to them (it is also on the Stats menu).
    if (cpPlaceMode_)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.15f, 0.55f, 0.85f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.9f, 1.f });
    }
    if (ImGui::Button(cpPlaceMode_ ? "◎  Placing... (Esc)"
        : "◎  Checkpoint", { btnW, 30.f }))
    {
        cpPlaceMode_ = !cpPlaceMode_;
        if (cpPlaceMode_) statsOpen_ = true;
    }
    if (cpPlaceMode_)
        ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Place a checkpoint with the mouse:\n"
            "click the canvas for the point, drag out to aim\n"
            "its direction, release to place.\n"
            "Right-click a marker deletes it.\n"
            "Esc, this button, or picking a drawing tool stops.");

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

    // ── Sensors ─────────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Sensors");
    ImGui::Spacing();

    int sensorCount = robot_.getSensorCount();
    ImGui::SetNextItemWidth(btnW);
    if (ImGui::SliderInt("##sensorCount", &sensorCount, 2, 15, "%d sensors"))
        robot_.setSensorCount(sensorCount);

    float sensorSpacing = robot_.getSensorSpacing();
    ImGui::SetNextItemWidth(btnW);
    if (ImGui::SliderFloat("##sensorSpacing", &sensorSpacing, 2.f, 30.f,
        "gap %.0f px"))
        robot_.setSensorSpacing(sensorSpacing);

    // ── Robot starting position ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Robot Start Position");
    ImGui::Spacing();

    // Labels above the fields so they are never clipped by the canvas panel
    ImGui::TextDisabled("Pos X");
    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("##startX", &startingPosX_, 0.f, 0.f, "%.1f"))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);

    ImGui::TextDisabled("Pos Y");
    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("##startY", &startingPosY_, 0.f, 0.f, "%.1f"))
        robot_.reset(startingPosX_, startingPosY_, startingAngle_);

    ImGui::TextDisabled("Angle (deg)");
    ImGui::SetNextItemWidth(btnW);
    if (ImGui::InputFloat("##startA", &startingAngle_, 0.f, 0.f, "%.1f"))
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

    bool hovered = ImGui::IsItemHovered() && !splitterActive_;
    bool mouseNow = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !splitterActive_;
    ImVec2 mp = ImGui::GetMousePos();

    // ── Checkpoint placement mode ──────────────────────────────────────────
    // Press = the checkpoint's point, drag = its heading, release = commit.
    // Drawing is suppressed while this mode is armed so a stray stroke can't
    // land on the track.
    if (cpPlaceMode_)
    {
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (hovered && mouseNow && !cpDragging_)
        {
            cpDragging_ = true;
            cpDragStart_ = screenToCanvas(mp);
            cpDragCur_ = cpDragStart_;
        }
        else if (cpDragging_ && mouseNow)
        {
            cpDragCur_ = screenToCanvas(mp);
        }
        else if (cpDragging_ && !mouseNow)
        {
            cpDragging_ = false;
            float dx = cpDragCur_.x - cpDragStart_.x;
            float dy = cpDragCur_.y - cpDragStart_.y;
            // A click with no meaningful drag keeps the angle already in the
            // panel field rather than snapping to an arbitrary direction.
            float ang = (dx * dx + dy * dy >= 64.f)
                ? rad2deg(std::atan2(dy, dx))
                : cpAngle_;
            checkpoints_.push_back({ cpDragStart_.x, cpDragStart_.y, ang });
            cpX_ = cpDragStart_.x; cpY_ = cpDragStart_.y; cpAngle_ = ang;
        }

        // Right-click a marker to remove it
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            sf::Vector2f c = screenToCanvas(mp);
            for (int i = (int)checkpoints_.size() - 1; i >= 0; --i)
            {
                float dx = c.x - checkpoints_[i].x;
                float dy = c.y - checkpoints_[i].y;
                if (dx * dx + dy * dy < 18.f * 18.f)
                {
                    checkpoints_.erase(checkpoints_.begin() + i);
                    if (curCp_ >= i) --curCp_;
                    break;
                }
            }
        }

        prevMouse_ = false;      // never hand this drag to the canvas
    }
    else
    {
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
    }

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

    drawCheckpointMarkers();
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

    // ── Auto compile toggle ───────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::Checkbox("Auto", &autoCompile_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically compile & reload ~1.5 s after you stop typing");

    // ── Compile output log ────────────────────────────────────────────────
    const float logH = 120.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, darkTheme_
        ? ImVec4{ 0.12f, 0.12f, 0.12f, 1.f }
        : ImVec4{ 0.92f, 0.92f, 0.92f, 1.f });
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

    // Wrap the editor in our own child so the font scale applies to the
    // window the editor actually draws in.
    ImGui::BeginChild("##edwrap", ImVec2{ panelW - 16.f, edH }, 0,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(editorFontScale_);
    editor_.SetImGuiChildIgnored(true);
    editor_.SetHandleMouseInputs(!splitterActive_);
    editor_.Render("##code_editor");
    editorFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::EndChild();

    // ── Auto compile (debounced) ─────────────────────────────────────────
    if (editor_.IsTextChanged())
        lastEditTime_ = ImGui::GetTime();
    if (autoCompile_ && simStarted_ && lastEditTime_ > 0.0 &&
        ImGui::GetTime() - lastEditTime_ > 1.5 &&
        compileStatus_.load() != 1)
    {
        lastEditTime_ = -1.0;
        saveCode();
        startCompile(/*forRun=*/false);
    }
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