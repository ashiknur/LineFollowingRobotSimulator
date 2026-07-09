#pragma once
#include <string>

// ---------------------------------------------------------------------------
// Paths
//
// An installed app lives in a read-only directory (Program Files), so every
// file the app writes at runtime — the user's editable UserCode.cpp, the
// compiled usercode_hot_N.dll, imgui.ini — goes to a per-user data dir:
//
//     %LOCALAPPDATA%\LineFollowingRobotSimulator
//
// The read-only template sources the compiler needs (headers + adapter
// sources) ship next to the exe in  <exeDir>\usersrc  and are copied /
// refreshed into the data dir on startup.
// ---------------------------------------------------------------------------
namespace paths
{
    // Directory containing the running executable (no trailing slash).
    const std::string& exeDir();

    // Per-user writable data dir (created on first call, no trailing slash).
    const std::string& dataDir();

    // <dataDir>\UserCode.cpp — the copy the in-app editor reads and writes.
    std::string userCodePath();

    // <dataDir>\imgui.ini
    std::string imguiIniPath();

    // Copies template sources into dataDir. UserCode.cpp is only copied if
    // missing (never overwrite the user's edits); the support files
    // (UserAPI.hpp, UserCode.hpp, LfrHostApi.h, Usercodeadapter.cpp,
    // UserAPI_hotreload.cpp) are refreshed every run so app updates take
    // effect. Also deletes stale usercode_hot_*.dll from previous sessions.
    void ensureUserSources();
}
