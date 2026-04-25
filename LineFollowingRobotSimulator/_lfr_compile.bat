@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
  echo [HotReload] vcvars64.bat failed. VS path: C:\Program Files\Microsoft Visual Studio\18\Community
  exit /b 1
)
cl /nologo /EHsc /LD /MDd /O2 /std:c++17 /DLFR_HOTRELOAD "UserCode.cpp" "Usercodeadapter.cpp" "UserAPI_hotreload.cpp" /Fe:./usercode_hot_1.dll /I. /I"C:\vcpkg\installed\x64-windows\include" 2>&1
