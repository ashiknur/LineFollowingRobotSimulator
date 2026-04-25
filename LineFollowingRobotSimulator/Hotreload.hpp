#pragma once
#include <string>
#include <atomic>
#include <functional>

// ---------------------------------------------------------------------------
// HotReload
//
// Compiles UserCode.cpp into a shared library at runtime, then loads it via
// dlopen (Linux/macOS) or LoadLibrary (Windows).  Calling reload() unloads
// the old lib, recompiles, and loads the new one.
//
// Symbols exported by the shared lib:
//   void  lfr_setup()          – called once after each successful load
//   void  lfr_loop ()          – called every user-thread iteration
//   void  lfr_setShared(void*) – receives the SharedState* cast to void*
//
// UserCode.cpp must implement setup() / loop() using the UserAPI as normal;
// a thin adapter (UserCodeAdapter.cpp) that is compiled INTO the shared lib
// translates the lfr_* exports to those calls.
// ---------------------------------------------------------------------------

struct CompileResult
{
    bool        success = false;
    std::string output;        // stdout + stderr from the compiler
};

class HotReload
{
public:
    explicit HotReload(const std::string& sourceFile = "UserCode.cpp",
        const std::string& libOut = "./usercode_hot");

    ~HotReload();

    // Compile + load (or reload).  Thread-safe w.r.t. the user thread via
    // the atomic flag below.
    CompileResult compile();

    // True if a valid lib is currently loaded.
    bool isLoaded() const { return libHandle_ != nullptr; }

    // Call from user thread ─────────────────────────────────────────────────
    void callSetup();
    void callLoop();

    // Shared pointer passed into the lib so UserAPI can reach SharedState
    void setSharedPtr(void* p) { sharedPtr_ = p; }

    // Set after compile() succeeds; user thread sees it and calls setup() once
    std::atomic<bool> reloadPending{ false };

private:
    std::string  sourceFile_;
    std::string  libBase_;     // path without extension
    std::string  libPath_;     // actual .so / .dll path
    int          generation_;  // incremented each compile so the OS doesn't cache

    void* libHandle_ = nullptr;
    void* sharedPtr_ = nullptr;

    using SetupFn = void(*)();
    using LoopFn = void(*)();
    using SetSharedFn = void(*)(void*);

    SetupFn     fnSetup_ = nullptr;
    LoopFn      fnLoop_ = nullptr;
    SetSharedFn fnSetShared_ = nullptr;

    void unload();
    bool load(const std::string& path);
    std::string makeLibPath() const;
};