@echo off
REM Builds OfficePanel.exe on Windows.
REM Run this from a normal (non-admin) Command Prompt inside the
REM OfficePanel_CPP folder.

setlocal enabledelayedexpansion

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found on PATH.
    echo         Install it from https://cmake.org/download/ ^(check "Add to PATH"
    echo         during setup^), or install it via the Visual Studio Installer under
    echo         "Individual Components" -^> "C++ CMake tools for Windows".
    pause
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo [NOTE] cl.exe not found on PATH -- that's expected if you're not running
    echo        this from a "Developer Command Prompt for VS". If the build step
    echo        below fails, open a "x64 Native Tools Command Prompt for VS" ^(from
    echo        the Start Menu, installed with Visual Studio / Build Tools^) and
    echo        run this script from there instead.
    echo.
)

REM ---------------------------------------------------------------------
REM 1. Get the WebView2 SDK (headers + loader lib) as raw files from the
REM    official NuGet package. This is the method Microsoft's own docs
REM    describe for C++ projects that aren't a Visual Studio project with
REM    NuGet Package Manager integration.
REM ---------------------------------------------------------------------
if not exist packages\Microsoft.Web.WebView2\build\native\include\WebView2.h (
    echo === Fetching the WebView2 SDK via nuget.exe ===

    if not exist nuget.exe (
        echo Downloading nuget.exe ...
        powershell -Command "Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe"
        if errorlevel 1 (
            echo [ERROR] Could not download nuget.exe. Check your internet connection,
            echo         or download it manually from https://www.nuget.org/downloads
            echo         and place it in this folder as nuget.exe, then re-run this script.
            pause
            exit /b 1
        )
    )

    nuget.exe install Microsoft.Web.WebView2 -OutputDirectory packages -ExcludeVersion -NonInteractive
    if errorlevel 1 (
        echo [ERROR] Failed to fetch the Microsoft.Web.WebView2 NuGet package.
        pause
        exit /b 1
    )
) else (
    echo === WebView2 SDK already present in packages\, skipping download ===
)

REM ---------------------------------------------------------------------
REM 2. Configure and build with CMake.
REM ---------------------------------------------------------------------
echo === Configuring with CMake ===
cmake -S . -B build -A x64
if errorlevel 1 (
    echo [ERROR] CMake configure failed. See the message above.
    echo         Common cause: no C++ compiler on PATH -- run this from a
    echo         "x64 Native Tools Command Prompt for VS" instead.
    pause
    exit /b 1
)

echo === Building OfficePanel.exe (Release) ===
cmake --build build --config Release
if errorlevel 1 (
    echo [ERROR] Build failed. See the message above.
    pause
    exit /b 1
)

echo === Installing into dist\OfficePanel ===
cmake --install build --config Release --prefix dist\OfficePanel
if errorlevel 1 (
    echo [ERROR] Install step failed. See the message above.
    pause
    exit /b 1
)

echo.
echo === Done ===
echo Your app is in: dist\OfficePanel\
echo Run it via:      dist\OfficePanel\OfficePanel.exe
echo.
echo Copy the WHOLE "OfficePanel" folder ^(not just the .exe^) wherever you want
echo to run it -- it needs the web\ folder sitting next to it.
echo.
pause
