// sheets_fetch_win.cpp
// Windows-only. Fetches a URL over HTTPS via WinHTTP -- built into every
// Windows install, so this needs zero extra library/download (unlike
// libcurl). This is the ONLY networking code in the whole app; everything
// else (routing, parsing, JSON, sessions) is the portable code already
// exercised by the test suite on Linux.
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "routes.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace op {

bool winHttpFetch(const std::string& url, std::string& outBody, std::string& outError) {
    // Our own generated Google Sheets export URLs are plain ASCII, so a
    // naive widen is fine here.
    std::wstring wUrl(url.begin(), url.end());

    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    wchar_t extraInfo[1024] = {};

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);
    urlComp.lpszExtraInfo = extraInfo;
    urlComp.dwExtraInfoLength = sizeof(extraInfo) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.size()), 0, &urlComp)) {
        outError = "Failed to parse URL";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(
        L"OfficePanel/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        outError = "WinHttpOpen failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    // Generous timeouts: resolve/connect/send/receive, in milliseconds.
    // Google Sheets CSV export can take a few seconds for a large sheet.
    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 20000);

    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
    if (!hConnect) {
        outError = "WinHttpConnect failed (error " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring pathAndQuery = std::wstring(urlComp.lpszUrlPath) + std::wstring(urlComp.lpszExtraInfo);
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", pathAndQuery.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        outError = "WinHttpOpenRequest failed (error " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // WinHTTP follows HTTP redirects by default (Google's export URL
    // sometimes 307-redirects to a signed download URL) -- no extra flag
    // needed for that.
    bool sent = WinHttpSendRequest(
        hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    bool received = sent && WinHttpReceiveResponse(hRequest, nullptr);

    if (!received) {
        outError = "Network request failed (error " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX);

    if (statusCode < 200 || statusCode >= 300) {
        outError = "HTTP status " + std::to_string(statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::string body;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;

        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) break;
        body.append(buffer.data(), bytesRead);
    } while (bytesAvailable > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    outBody = std::move(body);
    return true;
}

} // namespace op

#endif // _WIN32
