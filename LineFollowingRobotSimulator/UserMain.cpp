#include "UserAPI.hpp"
#include "SharedState.hpp"
#include "HotReload.hpp"
#include <chrono>
#include <thread>

// ---------------------------------------------------------------------------
// userMain – runs on the dedicated user thread.
//
// Flow:
//   1. Wait until HotReload has a valid compiled library loaded.
//      (The UI compiles first; the thread is spawned only after success, so
//      this wait is typically zero iterations.)
//   2. Call lfr_setup() once.
//   3. Loop calling lfr_loop() with:
//        • Pause support  – sleep while shared->paused (used during compile).
//        • Reload support – when HotReload signals reloadPending, call
//          lfr_setup() once with the new library before resuming lfr_loop().
// ---------------------------------------------------------------------------
void userMain(SharedState* shared, HotReload* hotreload)
{
    // ── Safety wait: lib should already be loaded, but guard just in case ──
    while (shared->running.load() && !hotreload->isLoaded())
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!shared->running.load())
        return;

    // ── Initial setup ───────────────────────────────────────────────────────
    hotreload->callSetup();

    // ── Main loop ───────────────────────────────────────────────────────────
    while (shared->running.load())
    {
        // Sleep while paused (used both for user-pause and during hot-reload)
        if (shared->paused.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // A successful Compile & Reload sets reloadPending.
        // Call setup() once with the new code, then resume looping.
        if (hotreload->reloadPending.exchange(false))
            hotreload->callSetup();

        hotreload->callLoop();
    }
}