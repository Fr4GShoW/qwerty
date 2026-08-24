// paths.h
// Resolves two locations, same split as the Python version had:
//   getBasePath()   -> read-only bundled resources (web/index.html, web/data/*)
//                      = the folder the .exe lives in (Windows) / this
//                      binary's own folder (test build), since this is a
//                      onedir-style native distribution: no temp extraction.
//   getDataPath()   -> writable, persistent per-user data folder
//                      (%LOCALAPPDATA%\OfficeEndpointPanel\data on Windows),
//                      seeded from the bundled defaults on first run.
#pragma once

#include <string>

namespace op {

std::string getExeDir();          // folder containing the running executable
std::string getBasePath();        // getExeDir() + "/web" (bundled, read-only) -- a default guess only
std::string getAppSupportDir();   // persistent per-user app folder
// Writable, seeded-on-first-run data folder. Takes the actual resolved
// bundled-resources folder explicitly (rather than recomputing its own
// guess via getBasePath() internally) so callers can't end up seeding
// from a different location than the one they're actually serving
// index.html/login.html from -- e.g. the dev test harness, which may run
// from a build directory that isn't next to web/, resolves basePath with
// its own fallback and must pass that same resolved value in here.
std::string getDataPath(const std::string& basePath);
std::string getLogsPath();        // getAppSupportDir() + "/logs"

#ifdef _WIN32
// Wide-string variant for Windows-only callers (e.g. main_win.cpp passing
// a path into WebView2's wide-char API) -- goes through
// std::filesystem::path::wstring() directly instead of a narrow/wide
// round-trip, so it can't mangle non-ASCII characters that can appear in
// a Windows user profile path (accented characters in the username, etc).
std::wstring getAppSupportDirW();
#endif

// Cross-platform-ish helpers used by the above and by data_store/routes.
bool pathExists(const std::string& path);
bool isDirectory(const std::string& path);
void makeDirs(const std::string& path); // mkdir -p equivalent
std::string joinPath(const std::string& a, const std::string& b);
bool copyFile(const std::string& src, const std::string& dst);
std::string readFile(const std::string& path, bool& ok);
bool writeFile(const std::string& path, const std::string& content);

} // namespace op
