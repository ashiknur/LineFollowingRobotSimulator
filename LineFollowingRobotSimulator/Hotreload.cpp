#include "HotReload.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

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
#if defined(_WIN32)
// Use vswhere.exe (ships with VS 2017+) to locate the VS installation dir.
// Falls back to the VS 2022 Community default if vswhere is absent.
static std::string findVSInstallPath()
{
    const char* cmd =
        "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\""
        " -latest -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64"
        " -property installationPath 2>nul";

    FILE* f = _popen(cmd, "r");
    if (!f)
        return "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community";

    char buf[512] = {};
    fgets(buf, (int)sizeof(buf) - 1, f);
    _pclose(f);

    std::string path(buf);
    while (!path.empty() &&
        (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
        path.pop_back();

    return path.empty()
        ? "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community"
        : path;
}
// Use VCPKG_ROOT env var if set, otherwise fall back to the default
// system-wide vcpkg install location (C:\vcpkg).
static std::string findVcpkgIncludePath()
{
    char* root = nullptr;
    size_t len = 0;
    _dupenv_s(&root, &len, "VCPKG_ROOT");
    std::string vcpkgRoot = (root && len > 0) ? root : "C:\\vcpkg";
    free(root);
    return vcpkgRoot + "\\installed\\x64-windows\\include";
}
#endif // _WIN32

// ---------------------------------------------------------------------------
HotReload::HotReload(const std::string& sourceFile,
    const std::string& libOut)
    : sourceFile_(sourceFile),
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
    fnSetShared_ = nullptr;

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

    fnSetup_ = reinterpret_cast<SetupFn>    (DL_SYM(libHandle_, "lfr_setup"));
    fnLoop_ = reinterpret_cast<LoopFn>     (DL_SYM(libHandle_, "lfr_loop"));
    fnSetShared_ = reinterpret_cast<SetSharedFn>(DL_SYM(libHandle_, "lfr_setShared"));

    if (!fnSetup_ || !fnLoop_ || !fnSetShared_)
    {
        std::cerr << "[HotReload] Missing symbols: " << dlError() << "\n";
        unload();
        return false;
    }

    // Inject SharedState pointer into the freshly loaded lib
    if (sharedPtr_)
        fnSetShared_(sharedPtr_);

    return true;
}

// ---------------------------------------------------------------------------
CompileResult HotReload::compile()
{
    CompileResult res;
    unload();
    ++generation_;
    libPath_ = makeLibPath();

    // ────────────────────────────────────────────────────────────────────────
    // Build the compiler command
    // ────────────────────────────────────────────────────────────────────────
    //
    // Sources compiled into the shared library:
    //   UserCode.cpp          – user-written setup() / loop()
    //   Usercodeadapter.cpp   – lfr_setup / lfr_loop / lfr_setShared exports
    //   UserAPI_hotreload.cpp – readSensor / setMotorSpeed / delayMs for DLL
    //
    // All source + header files are expected in the current working directory
    // (which is the project source folder when run from the VS debugger).

#if defined(_WIN32)
    // ── Windows: write a .bat file and run it via _popen ──────────────────
    //
    // We write a batch script that:
    //   1. Calls vcvars64.bat to set up the MSVC toolchain
    //   2. Compiles the shared library with cl.exe
    //
    // This is more reliable than trying to embed vcvars invocation inside a
    // single popen string with complex quoting.

    const std::string batPath = "_lfr_compile.bat";
    std::string vsPath = findVSInstallPath();
    std::string vcpkgInclude = findVcpkgIncludePath();

    {
        std::ofstream bat(batPath);
        if (!bat.is_open())
        {
            res.output = "[HotReload] Could not create _lfr_compile.bat\n";
            res.success = false;
            return res;
        }

        bat << "@echo off\r\n";
        // Suppress vcvars output so we only capture compiler messages
        bat << "call \"" << vsPath
            << "\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul 2>&1\r\n";
        bat << "if errorlevel 1 (\r\n";
        bat << "  echo [HotReload] vcvars64.bat failed. VS path: " << vsPath << "\r\n";
        bat << "  exit /b 1\r\n";
        bat << ")\r\n";

        // /LD  = build DLL
        // /MDd = dynamic debug CRT (matches a typical Debug project; change
        //        to /MD for Release builds)
        // /EHsc = C++ exceptions
        // /Fe: = output DLL path
        bat << "cl /nologo /EHsc /LD /MDd /O2 /std:c++17"
            << " /DLFR_HOTRELOAD"
            << " \"UserCode.cpp\""
            << " \"Usercodeadapter.cpp\""
            << " \"UserAPI_hotreload.cpp\""
            << " /Fe:" << libPath_
            << " /I."
            << " /I\"" << vcpkgInclude << "\""
            << " 2>&1\r\n";
    }

    std::cout << "[HotReload] Running " << batPath << " (VS: " << vsPath << ")\n";
    FILE* pipe = _popen(batPath.c_str(), "r");

#else
    // ── Linux / macOS: g++ directly ────────────────────────────────────────
    std::string fullCmd =
        "g++ -std=c++17 -O2 -shared -fPIC"
        " -DLFR_HOTRELOAD"
        " UserCode.cpp Usercodeadapter.cpp UserAPI_hotreload.cpp"
        " -o " + libPath_ +
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