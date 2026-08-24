cmake_minimum_required(VERSION 3.20)
project(OfficePanel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Portable core -- compiles and is unit/integration-tested on any platform.
# This is the bulk of the app: HTTP server, routing, JSON, CSV, the Google
# Sheets business logic, session/auth, and JSON-file persistence.
# ---------------------------------------------------------------------------
set(CORE_SOURCES
    src/json.cpp
    src/csv.cpp
    src/paths.cpp
    src/http_server.cpp
    src/auth.cpp
    src/data_store.cpp
    src/sheets_logic.cpp
    src/routes.cpp
)

add_library(office_panel_core STATIC ${CORE_SOURCES})
target_include_directories(office_panel_core PUBLIC src)

if(WIN32)
    target_link_libraries(office_panel_core PUBLIC Ws2_32)
    # PUBLIC so this also propagates to OfficePanel.exe's own sources
    # (main_win.cpp, webview_bridge.cpp, sheets_fetch_win.cpp) via the
    # target_link_libraries(OfficePanel PRIVATE office_panel_core) below.
    # Belt-and-suspenders alongside the #define at the top of every
    # Windows-touching .cpp file: windows.h's "min"/"max" macros silently
    # break std::min/std::max/numeric_limits<T>::max() (MSVC error C2589)
    # unless NOMINMAX is defined before windows.h is ever included, in
    # ANY translation unit in these targets -- including ones added later.
    #
    # UNICODE/_UNICODE: every Win32 call in this codebase uses the
    # explicit wide (W-suffixed) variant and wide string literals
    # (L"..."), matching a UNICODE build. Without these defined, resource
    # macros like IDC_ARROW still resolve through MAKEINTRESOURCE to the
    # ANSI (LPSTR) form, which then fails to convert when passed to an
    # explicit *W function (e.g. LoadCursorW) -- MSVC error C2664.
    target_compile_definitions(office_panel_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(office_panel_core PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Windows GUI build: OfficePanel.exe (WinMain + WebView2 window + WinHTTP
# fetch). Only configured/built when generating on Windows.
# ---------------------------------------------------------------------------
if(WIN32)
    # The WebView2 SDK is fetched as raw files from the official NuGet
    # package (see build_windows.bat), NOT via vcpkg -- this is the method
    # Microsoft's own docs describe for non-Visual-Studio C++ builds, and
    # keeps this file from depending on a specific vcpkg port's exact
    # target/variable names.
    set(WEBVIEW2_SDK_DIR "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native"
        CACHE PATH "Path to the WebView2 SDK's build/native folder from the NuGet package")

    if(NOT EXISTS "${WEBVIEW2_SDK_DIR}/include/WebView2.h")
        message(FATAL_ERROR
            "WebView2 SDK not found at ${WEBVIEW2_SDK_DIR}.\n"
            "Run build_windows.bat first (it downloads the SDK via nuget.exe), "
            "or set -DWEBVIEW2_SDK_DIR=<path> to an existing build/native folder "
            "from the Microsoft.Web.WebView2 NuGet package.")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WEBVIEW2_ARCH "x64")
    else()
        set(WEBVIEW2_ARCH "x86")
    endif()

    add_executable(OfficePanel WIN32
        src/main_win.cpp
        src/sheets_fetch_win.cpp
        src/webview_bridge.cpp
    )
    target_link_libraries(OfficePanel PRIVATE office_panel_core)
    target_include_directories(OfficePanel PRIVATE "${WEBVIEW2_SDK_DIR}/include")
    target_link_directories(OfficePanel PRIVATE "${WEBVIEW2_SDK_DIR}/${WEBVIEW2_ARCH}")
    target_link_libraries(OfficePanel PRIVATE
        WebView2LoaderStatic.lib
        Ws2_32.lib
        WinHttp.lib
        Shell32.lib
        Ole32.lib
        User32.lib
        Shlwapi.lib
    )
    # Static WebView2 loader -> no WebView2Loader.dll needs to ship next
    # to the .exe. Deliberately NOT forcing a static C++ runtime (/MT)
    # here: the WebView2 SDK's prebuilt WebView2LoaderStatic.lib is a
    # binary Microsoft ships (not something this project compiles), and
    # nothing here confirms it was built against the static runtime --
    # real-world projects that link this exact file (e.g.
    # github.com/Soundux/webviewpp) do so without forcing one, using
    # CMake's default (dynamic, /MD) instead. Forcing /MT to chase a
    # fully-standalone .exe risks trading today's runtime-library
    # mismatch for a new, unrecoverable one against a binary that can't
    # be recompiled here. The tradeoff: this .exe depends on the
    # Visual C++ Redistributable being present on the machine it runs
    # on (see README_BUILD_WINDOWS.md) -- overwhelmingly already true on
    # any Windows 10/11 machine, and a small, standard, free Microsoft
    # installer on the rare machine where it isn't.

    # Keep web/ next to the .exe wherever CMake actually puts it -- this
    # runs after every build, not just `cmake --install`, so pressing F5 in
    # Visual Studio still gives you a runnable app without a separate
    # install step (VS's Release build lands in build\Release\, not the
    # dist\ folder the install step produces).
    add_custom_command(TARGET OfficePanel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/web" "$<TARGET_FILE_DIR:OfficePanel>/web"
        COMMENT "Copying web/ next to OfficePanel.exe"
    )

    install(TARGETS OfficePanel RUNTIME DESTINATION .)
    install(DIRECTORY web/ DESTINATION web)
endif()

# ---------------------------------------------------------------------------
# Linux/macOS test harness: NOT shipped to users. Drives the exact same
# office_panel_core through real HTTP requests, with a stub Google Sheets
# fetch standing in for WinHTTP. This is how the core was validated during
# development; kept in the repo so changes can be re-verified the same way.
# ---------------------------------------------------------------------------
if(NOT WIN32)
    add_executable(office_panel_test
        test/main_test.cpp
        test/sheets_fetch_stub.cpp
    )
    target_include_directories(office_panel_test PRIVATE test)
    target_link_libraries(office_panel_test PRIVATE office_panel_core)
    target_compile_definitions(office_panel_test PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()
