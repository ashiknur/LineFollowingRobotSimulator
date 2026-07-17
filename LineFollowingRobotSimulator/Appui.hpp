#pragma once
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

#include "Canvas.hpp"
#include "Robot.hpp"
#include "SharedState.hpp"
#include "HotReload.hpp"

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
        HotReload& hotreload,
        sf::RenderWindow& window);

    ~AppUI();   // joins compileThread_ if still running

    // Call once per frame AFTER ImGui::SFML::Update().
    // Returns true on the single frame the user thread should be spawned
    // (i.e. the frame when the initial compile finishes successfully and the
    // simulation begins).
    bool render();

private:
    // ── References ──────────────────────────────────────────────────────────
    Canvas& canvas_;
    Robot& robot_;
    SharedState& shared_;
    HotReload& hotreload_;
    sf::RenderWindow& window_;

    // ── Scene composite texture (canvas + robot rendered together) ───────────
    sf::RenderTexture sceneRT_;

    // ── Code editor ─────────────────────────────────────────────────────────
    TextEditor editor_;

    // ── Simulation state ─────────────────────────────────────────────────────
    bool simStarted_ = false;  // thread has been (or will be) spawned
    bool firstRun_ = false;  // true for the one frame the thread should spawn

    // Set by compile thread when initial-run compile succeeds;
    // consumed by render() to flip simStarted_ and return true.
    std::atomic<bool> pendingThreadStart_{ false };

    // True while we are waiting for the very first compile (Run button pressed
    // but thread not yet spawned).
    bool compilingForRun_ = false;

    // ── Hot-reload compile state ─────────────────────────────────────────────
    // 0 = idle, 1 = compiling, 2 = success, 3 = failed
    std::atomic<int>  compileStatus_{ 0 };
    std::string       compileOutput_;         // protected by compileOutputMutex_
    std::mutex        compileOutputMutex_;
    std::thread       compileThread_;
    bool              scrollCompileLog_ = false; // auto-scroll flag

    // Starts an async compile; shared_.paused is set true during the compile.
    // forRun: if true, sets pendingThreadStart_ on success instead of just
    //         updating reloadPending.
    void startCompile(bool forRun);

    // ── Panel renderers ──────────────────────────────────────────────────────
    void renderToolsPanel(float w, float h);
    void renderCanvasPanel(float w, float h);
    void renderCodePanel(float w, float h);

    // ── Menu bar / theme / layout ────────────────────────────────────────────
    float renderMenuBar();              // returns menu bar height
    void  applyTheme();                 // ImGui style + editor palette
    void  renderSplitter(const char* id, float x, float y, float h, float& target,
                         bool invert, float minW, float maxW);
    void  handleGlobalShortcuts();      // canvas undo/redo, Ctrl+D popup open
    void  renderReplacePopup();

    bool  darkTheme_ = true;
    float toolsW_ = 172.f;              // resizable panel widths
    float codeW_ = 430.f;
    bool  splitterActive_ = false;      // a splitter is being dragged
                                        // (canvas/editor must ignore the mouse)
    bool  editorFocused_ = false;       // editor had focus last frame — global
                                        // Ctrl+Z must not also hit the canvas
    float editorFontScale_ = 1.0f;

    // Ctrl+D edit-all-occurrences state
    bool        openReplacePopup_ = false;
    std::string findText_;
    char        replaceBuf_[256] = {};

    // Track open/save failure message (shown as a modal until dismissed)
    std::string trackIoError_;

    // Auto-compile (debounced, after edits stop)
    bool   autoCompile_ = false;
    double lastEditTime_ = -1.0;        // ImGui::GetTime() of last edit

    // ── Scene helper ─────────────────────────────────────────────────────────
    void buildScene();

    // ── Canvas mouse tracking ────────────────────────────────────────────────
    ImVec2 canvasOrigin_ = { 0.f, 0.f };
    ImVec2 canvasDisp_ = { 0.f, 0.f };
    bool   prevMouse_ = false;

    sf::Vector2f screenToCanvas(ImVec2 p) const;
    ImVec2       canvasToScreen(sf::Vector2f p) const;

    // ── File helpers ─────────────────────────────────────────────────────────
    void loadCode();
    void saveCode() const;

    // ── Robot starting position ───────────────────────────────────────────────
    float startingPosX_ = 100.f;
    float startingPosY_ = 100.f;
    float startingAngle_ = 0.f;

    // ── Checkpoints & run statistics ─────────────────────────────────────────
    struct Checkpoint { float x, y, angle; };

    std::vector<Checkpoint> checkpoints_;
    int    curCp_ = -1;           // last reached checkpoint (-1 = start)
    int    skips_ = 0;
    int    restarts_ = 0;
    bool   runActive_ = false;
    bool   runFinished_ = false;
    float  runTime_ = 0.f;        // active (unpaused) run seconds
    float  stillTime_ = 0.f;      // seconds the robot has been stationary
    bool   stoppedAtEnd_ = false; // stopped near the last checkpoint
    float  lastRobotX_ = 0.f, lastRobotY_ = 0.f;

    // Score parameters (user editable in the Stats panel)
    float scoreB_ = 100.f;   // base score            (b)
    float scoreS_ = 10.f;    // penalty per skip      (s)
    float scoreR_ = 5.f;     // penalty per restart   (r)
    float scoreC_ = 50.f;    // clean-run bonus       (c)
    float scoreSt_ = 20.f;   // stop-at-end bonus     (st)

    bool  statsOpen_ = false;
    float cpX_ = 100.f, cpY_ = 100.f, cpAngle_ = 0.f;   // add-checkpoint fields

    void  beginRun();             // reset counters/timer; called on sim start
    void  newRun();               // teleport to start + beginRun + re-setup()
    void  doSkip();               // advance to next checkpoint (counted)
    void  doRestartCp();          // back to current checkpoint (counted)
    void  updateRunTracking();    // per-frame: timer, auto-progress, finish
    float computeScore(bool& clean, float& t) const;
    void  renderStatsWindow();
    void  drawCheckpointMarkers();
};