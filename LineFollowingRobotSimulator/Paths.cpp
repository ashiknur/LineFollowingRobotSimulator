#include "Paths.hpp"

#include <filesystem>
#include <iostream>
#include <cstdlib>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
    std::string computeExeDir()
    {
#if defined(_WIN32)
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        fs::path p(buf);
        return p.parent_path().string();
#else
        return fs::current_path().string();
#endif
    }

    std::string computeDataDir()
    {
#if defined(_WIN32)
        char* base = nullptr; size_t len = 0;
        _dupenv_s(&base, &len, "LOCALAPPDATA");
        fs::path dir = (base && len > 0)
            ? fs::path(base) / "LineFollowingRobotSimulator"
            : fs::path(computeExeDir()) / "userdata";   // last-resort fallback
        free(base);
#else
        const char* home = std::getenv("HOME");
        fs::path dir = fs::path(home ? home : ".") / ".linefollowersim";
#endif
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec)
            std::cerr << "[Paths] Could not create data dir "
                      << dir.string() << ": " << ec.message() << "\n";
        return dir.string();
    }

    // The support files the runtime compiler needs besides UserCode.cpp.
    const char* kSupportFiles[] = {
        "UserAPI.hpp",
        "UserCode.hpp",
        "LfrHostApi.h",
        "Usercodeadapter.cpp",
        "UserAPI_hotreload.cpp",
    };
}

namespace paths
{
    const std::string& exeDir()
    {
        static const std::string dir = computeExeDir();
        return dir;
    }

    const std::string& dataDir()
    {
        static const std::string dir = computeDataDir();
        return dir;
    }

    std::string userCodePath()  { return (fs::path(dataDir()) / "UserCode.cpp").string(); }
    std::string imguiIniPath()  { return (fs::path(dataDir()) / "imgui.ini").string(); }

    void ensureUserSources()
    {
        const fs::path data(dataDir());

        // Template location: <exeDir>\usersrc for an installed app; fall back
        // to the current working directory, which is the project source dir
        // when running from the Visual Studio debugger.
        fs::path tpl = fs::path(exeDir()) / "usersrc";
        std::error_code ec;
        if (!fs::exists(tpl / "UserCode.cpp", ec))
            tpl = fs::current_path();

        auto copyFile = [&](const char* name, bool overwrite)
        {
            fs::path src = tpl / name;
            fs::path dst = data / name;
            std::error_code cec;
            if (!fs::exists(src, cec))
            {
                std::cerr << "[Paths] Template missing: " << src.string() << "\n";
                return;
            }
            if (!overwrite && fs::exists(dst, cec))
                return;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, cec);
            if (cec)
                std::cerr << "[Paths] Copy failed " << src.string()
                          << " -> " << dst.string() << ": " << cec.message() << "\n";
        };

        copyFile("UserCode.cpp", /*overwrite=*/false);   // keep user edits
        for (const char* f : kSupportFiles)
            copyFile(f, /*overwrite=*/true);             // refresh on update

        // Remove compiled DLLs from previous sessions. A file may be locked
        // by another running instance — ignore failures.
        for (fs::directory_iterator it(data, ec), end; !ec && it != end; it.increment(ec))
        {
            const fs::path& p = it->path();
            if (p.extension() == ".dll" &&
                p.filename().string().rfind("usercode_hot_", 0) == 0)
            {
                std::error_code dec;
                fs::remove(p, dec);
            }
        }
    }
}
