# Building the Windows installers

Two installer variants are built from the same script, `LineFollowerSim.iss`
(requires [Inno Setup 6](https://jrsoftware.org/isinfo.php)):

| Variant | Command | Size | Behavior |
|---|---|---|---|
| Full (offline) | `ISCC LineFollowerSim.iss` | ~31 MB | Bundles the MinGW toolchain; works with no internet |
| Slim | `ISCC /DSlim LineFollowerSim.iss` | ~3 MB | Downloads `lfr-toolchain-x64.zip` during setup |

Output goes to `installer\Output\`.

## Prerequisites (staging the Release folder)

The script packages `..\LineFollowingRobotSimulator\x64\Release\`, which must
contain:

1. `LineFollowingRobotSimulator.exe` — build **Release x64** in Visual Studio.
2. SFML/vcpkg DLLs — copied automatically by vcpkg during the build.
3. VC++ CRT DLLs (`msvcp140*.dll`, `vcruntime140*.dll`, ...) — copy from
   `<VS>\VC\Redist\MSVC\<ver>\x64\Microsoft.VC14x.CRT\`.
4. `LFR body.png` — copy from the project folder.
5. `usersrc\` — the user-code template sources: `UserCode.cpp`, `UserCode.hpp`,
   `UserAPI.hpp`, `LfrHostApi.h`, `Usercodeadapter.cpp`, `UserAPI_hotreload.cpp`.
6. `toolchain\` (full variant only) — pruned [w64devkit](https://github.com/skeeto/w64devkit)
   MinGW-w64 GCC. Keep only: `bin\{g++,as,ld}.exe`,
   `libexec\gcc\x86_64-w64-mingw32\<ver>\{cc1plus,collect2,lto-wrapper,g++-mapper-server}.exe`
   plus its DLLs, and the full `include\` and `lib\` trees (minus
   `libgfortran`, `lib*apiset*`, `libonecore*` and similar exotic import
   libs, `share\`, `src\`). Result is ~250 MB on disk, ~47 MB zipped.

## Hosting the toolchain zip (slim variant)

Create the zip from the staged folder (zip root must be `toolchain\...`):

    cd ..\LineFollowingRobotSimulator\x64\Release
    tar -a -c -f ..\..\..\installer\lfr-toolchain-x64.zip toolchain

Upload `lfr-toolchain-x64.zip` as a GitHub release asset with tag
`toolchain-v1` on this repository — that matches `ToolchainUrl` in the
script. If you host it elsewhere, update the `#define ToolchainUrl` line.

## What the installed app does at runtime

- On first run it copies `usersrc\` templates to
  `%LOCALAPPDATA%\LineFollowingRobotSimulator\` — the editor and the runtime
  compiler work there (Program Files is read-only).
- The runtime compiler search order: `LFR_TOOLCHAIN` env var →
  `<install>\toolchain` → `%LOCALAPPDATA%\LineFollowingRobotSimulator\toolchain`
  → `g++` on `PATH` → MSVC via vswhere (developer machines).
- Uninstalling keeps `%LOCALAPPDATA%\LineFollowingRobotSimulator` (the user's
  own code); delete it manually for a fully clean removal.

## Licensing note

The bundled toolchain is [w64devkit](https://github.com/skeeto/w64devkit)
(GCC/MinGW-w64, free software). Binaries produced by it are unencumbered;
when distributing the toolchain itself, keep a pointer to its source
(the w64devkit releases page) to satisfy GPL source-offer requirements.
