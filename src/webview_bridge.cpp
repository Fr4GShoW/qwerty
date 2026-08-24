// webview_bridge.cpp
#ifdef _WIN32

// webview_bridge.h now brings in <windows.h>/<unknwn.h> itself (in that
// order, before WebView2.h) with WIN32_LEAN_AND_MEAN/NOMINMAX already
// set -- redefining them again here would just be a second, redundant
// copy of the same macros (harmless, but it's what was producing the
// "macro redefinition" warnings you saw in the build log).
#include "webview_bridge.h"
#include <shlwapi.h> // SHCreateMemStream
#include <wrl.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")

using namespace Microsoft::WRL;

namespace op {

const wchar_t* kVirtualHost = L"officepanel.local";
const wchar_t* kVirtualOrigin = L"https://officepanel.local";

namespace {

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), size);
    return out;
}

std::string wstrToUtf8(LPCWSTR ws) {
    return ws ? wideToUtf8(std::wstring(ws)) : std::string();
}

std::string readAllFromStream(IStream* stream) {
    std::string out;
    if (!stream) return out;
    char buf[8192];
    ULONG read = 0;
    while (true) {
        HRESULT hr = stream->Read(buf, sizeof(buf), &read);
        if (FAILED(hr) || read == 0) break;
        out.append(buf, read);
        if (read < sizeof(buf)) break; // short read = end of stream for this stream type
    }
    return out;
}

// Splits "https://officepanel.local/api/foo?x=1" (or any absolute URI on
// our virtual host) into path="/api/foo" and query="x=1".
void splitPathAndQuery(const std::string& uri, std::string& path, std::string& query) {
    std::string originPrefix = wideToUtf8(std::wstring(kVirtualOrigin));
    std::string rest = uri;
    if (rest.rfind(originPrefix, 0) == 0) {
        rest = rest.substr(originPrefix.size());
    }
    size_t qpos = rest.find('?');
    if (qpos != std::string::npos) {
        path = rest.substr(0, qpos);
        query = rest.substr(qpos + 1);
    } else {
        path = rest;
        query.clear();
    }
    if (path.empty()) path = "/";
}

void parseCookieHeader(const std::string& cookieHeaderValue, std::map<std::string, std::string>& outCookies) {
    std::istringstream iss(cookieHeaderValue);
    std::string part;
    while (std::getline(iss, part, ';')) {
        size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        size_t start = part.find_first_not_of(' ');
        if (start == std::string::npos) continue;
        std::string name = part.substr(start, eq - start);
        std::string value = part.substr(eq + 1);
        outCookies[name] = value;
    }
}

const char* defaultReasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 302: return "Found";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

HttpRequest buildRequestFromWebView2(ICoreWebView2WebResourceRequest* webReq) {
    HttpRequest req;

    LPWSTR method = nullptr;
    webReq->get_Method(&method);
    req.method = wstrToUtf8(method);
    if (method) CoTaskMemFree(method);

    LPWSTR uri = nullptr;
    webReq->get_Uri(&uri);
    std::string uriUtf8 = wstrToUtf8(uri);
    if (uri) CoTaskMemFree(uri);
    splitPathAndQuery(uriUtf8, req.path, req.query);
    req.path = urlDecode(req.path);

    ComPtr<ICoreWebView2HttpRequestHeaders> headers;
    if (SUCCEEDED(webReq->get_Headers(&headers)) && headers) {
        ComPtr<ICoreWebView2HttpHeadersCollectionIterator> it;
        if (SUCCEEDED(headers->GetIterator(&it))) {
            BOOL hasCurrent = FALSE;
            while (SUCCEEDED(it->get_HasCurrentHeader(&hasCurrent)) && hasCurrent) {
                LPWSTR name = nullptr;
                LPWSTR value = nullptr;
                if (SUCCEEDED(it->GetCurrentHeader(&name, &value))) {
                    std::string nameUtf8 = wstrToUtf8(name);
                    std::string valueUtf8 = wstrToUtf8(value);
                    for (auto& c : nameUtf8) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    req.headers[nameUtf8] = valueUtf8;
                    if (nameUtf8 == "cookie") parseCookieHeader(valueUtf8, req.cookies);
                }
                if (name) CoTaskMemFree(name);
                if (value) CoTaskMemFree(value);
                BOOL hasNext = FALSE;
                if (FAILED(it->MoveNext(&hasNext)) || !hasNext) break;
            }
        }
    }

    ComPtr<IStream> content;
    webReq->get_Content(&content);
    if (content) req.body = readAllFromStream(content.Get());

    return req;
}

ComPtr<ICoreWebView2WebResourceResponse> buildWebView2Response(
    ICoreWebView2Environment* env, const HttpResponse& res) {

    std::ostringstream headerBlock;
    for (auto& h : res.headers) {
        headerBlock << h.first << ": " << h.second << "\r\n";
    }
    for (auto& c : res.setCookies) {
        headerBlock << "Set-Cookie: " << c << "\r\n";
    }
    std::wstring headersW = utf8ToWide(headerBlock.str());
    std::wstring reasonW = utf8ToWide(defaultReasonPhrase(res.status));

    // SHCreateMemStream copies the buffer internally, so it's safe even
    // though `res.body` (a temporary/local by the time this returns) goes
    // out of scope shortly after.
    ComPtr<IStream> contentStream;
    contentStream.Attach(SHCreateMemStream(
        reinterpret_cast<const BYTE*>(res.body.data()),
        static_cast<UINT>(res.body.size())));

    ComPtr<ICoreWebView2WebResourceResponse> response;
    env->CreateWebResourceResponse(
        contentStream.Get(), res.status, reasonW.c_str(), headersW.c_str(), &response);
    return response;
}

} // namespace

void installWebViewBridge(ICoreWebView2Environment* env, ICoreWebView2* webview, HttpServer* server) {
    webview->AddWebResourceRequestedFilter(
        (std::wstring(kVirtualOrigin) + L"/*").c_str(),
        COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

    EventRegistrationToken token;
    webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [env, server](ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                ComPtr<ICoreWebView2WebResourceRequest> webReq;
                args->get_Request(&webReq);
                if (!webReq) return S_OK;

                HttpRequest req = buildRequestFromWebView2(webReq.Get());
                HttpResponse res;
                server->dispatch(req, res);

                ComPtr<ICoreWebView2WebResourceResponse> webRes = buildWebView2Response(env, res);
                args->put_Response(webRes.Get());
                return S_OK;
            }).Get(),
        &token);
}

} // namespace op

#endif // _WIN32
