@echo off
REM Build the quiet LHP2 d3d9 fix proxy (32-bit).
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo [*] Setting up MSVC x86 environment...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
)
cd /d "%~dp0"

echo Building d3d9.dll...
cl /nologo /LD /O2 /W3 lhp2_d3d9_fix.c /Fe:d3d9.dll /link /DEF:d3d9.def user32.lib kernel32.lib
if errorlevel 1 (
    echo FAILED
    exit /b 1
)

echo === BUILD COMPLETE ===
dir /b d3d9.dll 2>nul
