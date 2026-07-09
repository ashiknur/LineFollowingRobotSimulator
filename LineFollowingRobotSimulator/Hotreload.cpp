#include "HotReload.hpp"
#include "Paths.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// ── Platform DL API ──────────────────────────────────────────────────────────
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define DL_OPEN(p)    static_cast<void*>(LoadLibraryA(p))
#  define DL_SYM(h,s)   static_cast<void*>(GetProcAddress(static_cast<HMODULE>(h),(s)))
#  define DL_CLOSE(h)   FreeLibrary(static_cast<HMODULE>(h))
#  define LIB_EXT       ".dll"
static std::string dlError()
{
    char buf[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), 0, buf, 255, nullptr);
    return buf;
}
#else
#  include <dlfcn.h>
#  define DL_OPEN(p)    dlopen((p), RTLD_NOW | RTLD_LOCAL)
#  define DL_SYM(h,s)   dlsym((h),(s))
#  define DL_CLOSE(h)   dlclose(h)
#  if defined(__APPLE__)
#    define LIB_EXT     ".dylib"
#  else
#    define LIB_EXT     ".so"
#  endif
static std::string dlError() { return dlerror() ? dlerror() : "unknown"; }
#endif

// ---------------------------------------------------------------------------
// Toolchain discovery
//
// The installed app ships a self-contained MinGW toolchain, so runtime
// compilation works on machines with no development tools at all.
// Search order:
//   1. LFR_TOOLCHAIN env var  – explicit override (toolchain root or full
//      path to the compiler exe)
//   2. <exeDir>\toolchain\bin – bundled by the installer
//   3. <dataDir>\toolchain\bin – downloaded post-install (slim installer)
//   4. g++ on PATH
//   5. MSVC located via vswhere – developer-machine fallback
// ---------------------------------------------------------------------------
#if defined(_WIN32)

struct Toolchain
{
    enum Kind { None, Gxx, Msvc };
    Kind        kind = None;
    std::string path;   // Gxx: full path to g++/clang++ exe; Msvc: VS install dir
};

static bool fileExists(const std::string& p)
{
    std::error_code ec;
    return fs::exists(p, ec);
}

// Returns the compiler exe inside a toolchain root, or "" if absent.
static std::string compilerInRoot(const fs::path& root)
{
    for (const char* name : { "g++.exe", "clang++.exe" })
    {
        fs::path c = root / "bin" / name;
        std::error_code ec;
        if (fs::exists(c, ec))
            return c.string();
    }
    return {};
}

// Use vswhere.exe (ships with VS 2017+) to locate the VS installation dir.
static std::string findVSInstallPath()
{
    const char* cmd =
        "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\""
        " -latest -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64"
        " -property installationPath 2>nul";

    FILE* f = _popen(cmd, "r");
    if (!f)
        return {};

    char buf[512] = {};
    fgets(buf, (int)sizeof(buf) - 1, f);
    _pclose(f);

    std::string path(buf);
    while (!path.empty() &&
        (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
        path.pop_back();

    return path;
}

static Toolchain findToolchain()
{
    Toolchain tc;

    // 1. Explicit override
    {
        char* env = nullptr; size_t len = 0;
        _dupenv_s(&env, &len, "LFR_TOOLCHAIN");
        std::string override_ = (env && len > 0) ? env : "";
        free(env);
        if (!override_.empty())
        {
            std::string c = fileExists(override_) && fs::path(override_).extension() == ".exe"
                ? override_
                : compilerInRoot(override_);
            if (!c.empty())
            {
                tc.kind = Toolchain::Gxx; tc.path = c;
                return tc;
            }
            std::cerr << "[HotReload] LFR_TOOLCHAIN set but no compiler found under: "
                      << override_ << "\n";
        }
    }

    // 2. Bundled next to the exe / 3. downloaded into the data dir
    for (const fs::path root : { fs::path(paths::exeDir()) / "toolchain",
                                 fs::path(paths::dataDir()) / "toolchain" })
    {
        std::string c = compilerInRoot(root);
        if (!c.empty())
        {
            tc.kind = Toolchain::Gxx; tc.path = c;
            return tc;
        }
    }

    // 4. g++ on PATH
    {
        char found[MAX_PATH] = {};
        if (SearchPathA(nullptr, "g++", ".exe", MAX_PATH, found, nullptr) > 0)
        {
            tc.kind = Toolchain::Gxx; tc.path = found;
            return tc;
        }
    }

    // 5. MSVC (developer machines)
    {
        std::string vs = findVSInstallPath();
        if (!vs.empty())
        {
            tc.kind = Toolchain::Msvc; tc.path = vs;
            return tc;
        }
    }

    return tc; // None
}

#endif // _WIN32

// ---------------------------------------------------------------------------
HotReload::HotReload(const std::string& srcDir,
    const std::string& libOut)
    : srcDir_(srcDir),
    libBase_(libOut),
    generation_(0)
{}

HotReload::~HotReload() { unload(); }

// ---------------------------------------------------------------------------
std::string HotReload::makeLibPath() const
{
    return libBase_ + "_" + std::to_string(generation_) + LIB_EXT;
}

// ---------------------------------------------------------------------------
void HotReload::unload()
{
    fnSetup_ = nullptr;
    fnLoop_ = nullptr;
    fnSetHostApi_ = nullptr;

    if (libHandle_)
    {
        DL_CLOSE(libHandle_);
        libHandle_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
bool HotReload::load(const std::string& path)
{
    libHandle_ = DL_OPEN(path.c_str());
    if (!libHandle_)
    {
        std::cerr << "[HotReload] dlopen failed: " << dlError() << "\n";
        return false;
    }

    fnSetup_ = reinterpret_cast<SetupFn>     (DL_SYM(libHandle_, "lfr_setup"));
    fnLoop_ = reinterpret_cast<LoopFn>       (DL_SYM(libHandle_, "lfr_loop"));
    fnSetHostApi_ = reinterpret_cast<SetHostApiFn>(DL_SYM(libHandle_, "lfr_setHostApi"));

    if (!fnSetup_ || !fnLoop_ || !fnSetHostApi_)
    {
        std::cerr << "[HotReload] Missing symbols: " << dlError() << "\n";
        unload();
        return false;
    }

    // Hand the C callback table to the freshly loaded lib
    fnSetHostApi_(&hostApi_);

    return true;
}

// ---------------------------------------------------------------------------
CompileResult HotReload::compile()
{
    CompileResult res;
    unload();
    ++generation_;
    libPath_ = makeLibPath();

    // Sources compiled into the shared library (all inside srcDir_):
    //   UserCode.cpp          – user-written setup() / loop()
    //   Usercodeadapter.cpp   – lfr_setup / lfr_loop / lfr_setHostApi exports
    //   UserAPI_hotreload.cpp – readSensor / setMotorSpeed / delayMs for DLL

#if defined(_WIN32)
    Toolchain tc = findToolchain();
    if (tc.kind == Toolchain::None)
    {
        res.output =
            "[HotReload] No C++ compiler found.\n"
            "Reinstall the application (the installer includes the compiler),\n"
            "or set the LFR_TOOLCHAIN environment variable to a MinGW root.\n";
        res.success = false;
        return res;
    }

    // Write a batch script into the (writable) source dir and run it via
    // _popen. This is more reliable than a single popen string with nested
    // quoting, and lets us cd into srcDir_ so cl.exe drops its .obj files
    // there instead of in the exe dir.
    const std::string batPath = (fs::path(srcDir_) / "_lfr_compile.bat").string();
    const std::string src = srcDir_;

    {
        std::ofstream bat(batPath);
        if (!bat.is_open())
        {
            res.output = "[HotReload] Could not create " + batPath + "\n";
            res.success = false;
            return res;
        }

        bat << "@echo off\r\n";
        bat << "cd /d \"" << src << "\"\r\n";

        if (tc.kind == Toolchain::Gxx)
        {
            // -static* : the produced DLL must not depend on libstdc++/
            // libwinpthread DLLs living inside the toolchain dir.
            bat << "\"" << tc.path << "\""
                << " -std=c++17 -O2 -shared"
                << " -DLFR_HOTRELOAD"
                << " \"" << src << "\\UserCode.cpp\""
                << " \"" << src << "\\Usercodeadapter.cpp\""
                << " \"" << src << "\\UserAPI_hotreload.cpp\""
                << " -o \"" << libPath_ << "\""
                << " -I\"" << src << "\""
                << " -static -static-libgcc -static-libstdc++"
                << " 2>&1\r\n";
        }
        else // Msvc
        {
            bat << "call \"" << tc.path
                << "\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul 2>&1\r\n";
            bat << "if errorlevel 1 (\r\n";
            bat << "  echo [HotReload] vcvars64.bat failed. VS path: " << tc.path << "\r\n";
            bat << "  exit /b 1\r\n";
            bat << ")\r\n";
            bat << "cl /nologo /EHsc /LD /MD /O2 /std:c++17"
                << " /DLFR_HOTRELOAD"
                << " \"" << src << "\\UserCode.cpp\""
                << " \"" << src << "\\Usercodeadapter.cpp\""
                << " \"" << src << "\\UserAPI_hotreload.cpp\""
                << " /Fe:\"" << libPath_ << "\""
                << " /I\"" << src << "\""
                << " 2>&1\r\n";
        }
    }

    std::cout << "[HotReload] Compiler: "
              << (tc.kind == Toolchain::Gxx ? tc.path : tc.path + " (MSVC)") << "\n";

    // Double quotes around the whole /c payload survive cmd's quote stripping
    // even when batPath itself contains spaces.
    const std::string cmdLine = "\"\"" + batPath + "\"\"";
    FILE* pipe = _popen(cmdLine.c_str(), "r");

#else
    // ── Linux / macOS: g++ directly ────────────────────────────────────────
    std::string fullCmd =
        "cd \"" + srcDir_ + "\" && g++ -std=c++17 -O2 -shared -fPIC"
        " -DLFR_HOTRELOAD"
        " UserCode.cpp Usercodeadapter.cpp UserAPI_hotreload.cpp"
        " -o \"" + libPath_ + "\""
        " -I."
        " 2>&1";

    std::cout << "[HotReload] " << fullCmd << "\n";
    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif

    // ── Capture compiler output ──────────────────────────────────────────────
    if (pipe)
    {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe))
            res.output += buf;

#if defined(_WIN32)
        int ret = _pclose(pipe);
#else
        int ret = pclose(pipe);
#endif
        res.success = (ret == 0);
    }
    else
    {
        res.output = "[HotReload] Failed to launch compiler process.\n";
        res.success = false;
    }

    // ── Load the freshly built library ───────────────────────────────────────
    if (res.success)
    {
        if (load(libPath_))
        {
            // Signal the user thread to call setup() once on its next iteration
            reloadPending = true;
            std::cout << "[HotReload] Loaded: " << libPath_ << "\n";
        }
        else
        {
            res.success = false;
            res.output += "\n[HotReload] Library load (dlopen) failed after compile.\n";
        }
    }

    return res;
}

// ---------------------------------------------------------------------------
void HotReload::callSetup()
{
    if (fnSetup_) fnSetup_();
}

void HotReload::callLoop()
{
    if (fnLoop_) fnLoop_();
}
