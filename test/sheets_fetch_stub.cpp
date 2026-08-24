// sheets_fetch_stub.cpp
// Non-Windows dev/test build only. Fetches a URL via the `curl` CLI (kept
// as a subprocess call rather than adding a libcurl dependency, since this
// file only exists to let office_panel_test run the real Sync-from-Sheets
// flow end-to-end on a developer's own Linux/macOS machine -- it is never
// part of the Windows build, which uses WinHTTP directly instead
// (sheets_fetch_win.cpp).
#include "routes.h"
#include <array>
#include <memory>
#include <cstdio>
#include "sheets_fetch_stub.h"

namespace op {

bool curlFetch(const std::string& url, std::string& outBody, std::string& outError) {
    std::string cmd = "curl -sL --max-time 20 '" + url + "'";
    std::array<char, 8192> buffer;
    std::string result;

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        outError = "popen() failed";
        return false;
    }
    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), pipe.get())) > 0) {
        result.append(buffer.data(), n);
    }

    if (result.empty()) {
        outError = "empty response (no network access, or curl not installed)";
        return false;
    }
    outBody = std::move(result);
    return true;
}

} // namespace op
