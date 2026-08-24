// paths.cpp
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
#endif
#include "paths.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <climits>
#endif

namespace fs = std::filesystem;

namespace op {

std::string getExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0) return ".";
    fs::path p(std::wstring(buf, len));
    return p.parent_path().string();
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return ".";
    buf[len] = '\0';
    fs::path p(buf);
    return p.parent_path().string();
#endif
}

std::string getBasePath() {
    return joinPath(getExeDir(), "web");
}

std::string getAppSupportDir() {
    std::string root;
#ifdef _WIN32
    wchar_t* localAppData = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData) == S_OK) {
        fs::path p(localAppData);
        root = p.string();
        CoTaskMemFree(localAppData);
    } else {
        const char* env = std::getenv("LOCALAPPDATA");
        root = env ? env : ".";
    }
#else
    const char* home = std::getenv("HOME");
    root = joinPath(home ? home : ".", ".local/share");
#endif
    std::string dir = joinPath(root, "OfficeEndpointPanel");
    makeDirs(dir);
    return dir;
}

#ifdef _WIN32
std::wstring getAppSupportDirW() {
    fs::path root;
    wchar_t* localAppData = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData) == S_OK) {
        root = fs::path(localAppData);
        CoTaskMemFree(localAppData);
    } else {
        const wchar_t* env = _wgetenv(L"LOCALAPPDATA");
        root = env ? fs::path(env) : fs::path(L".");
    }
    fs::path dir = root / L"OfficeEndpointPanel";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.wstring();
}
#endif

std::string getDataPath(const std::string& basePath) {
    std::string dataDir = joinPath(getAppSupportDir(), "data");
    makeDirs(dataDir);

    std::string bundledDataDir = joinPath(basePath, "data");
    if (isDirectory(bundledDataDir)) {
        static const char* files[] = {
            "endpoints.json", "layout.json", "office_data.json", "workplace.json"
        };
        for (const char* name : files) {
            std::string src = joinPath(bundledDataDir, name);
            std::string dst = joinPath(dataDir, name);
            if (pathExists(src) && !pathExists(dst)) {
                copyFile(src, dst);
            }
        }
    }
    return dataDir;
}

std::string getLogsPath() {
    std::string dir = joinPath(getAppSupportDir(), "logs");
    makeDirs(dir);
    return dir;
}

bool pathExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool isDirectory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

void makeDirs(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
}

std::string joinPath(const std::string& a, const std::string& b) {
    fs::path p = fs::path(a) / b;
    return p.string();
}

bool copyFile(const std::string& src, const std::string& dst) {
    std::error_code ec;
    return fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
}

std::string readFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;
    return ss.str();
}

bool writeFile(const std::string& path, const std::string& content) {
    fs::path p(path);
    if (p.has_parent_path()) makeDirs(p.parent_path().string());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return static_cast<bool>(f);
}

} // namespace op
