// main_win.cpp
// Windows-only GUI entry point. Creates a native window, embeds a
// WebView2 control, and answers every request the page makes entirely
// in-process via webview_bridge.cpp -- there is NO TCP socket, NO port,
// and NO localhost HTTP server anywhere in this app. The page navigates
// to a virtual https://officepanel.local/ origin that WebView2 never
// actually makes a network connection for; every request against it is
// intercepted and answered directly from the same routing/business logic
// (routes.cpp, data_store.cpp, auth.cpp, sheets_logic.cpp) already
// covered by the test suite in test/.
//
// The only real network activity this app ever does is the outbound
// "Sync from Sheets" fetch to Google's servers (sheets_fetch_win.cpp,
// via WinHTTP) -- that's inherent to the feature itself, not a local
// server.
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include "WebView2.h" // from the WebView2 NuGet/vcpkg package

#include <string>

#include "http_server.h"
#include "routes.h"
#include "data_store.h"
#include "auth.h"
#include "paths.h"
#include "sheets_fetch_win.h"
#include "webview_bridge.h"

using namespace Microsoft::WRL;

namespace {

ComPtr<ICoreWebView2Controller> g_webviewController;
ComPtr<ICoreWebView2> g_webview;
ComPtr<ICoreWebView2Environment> g_webviewEnvironment;

void resizeWebviewToWindow(HWND hwnd) {
    if (!g_webviewController) return;
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    g_webviewController->put_Bounds(bounds);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            resizeWebviewToWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void initWebView2(HWND hwnd, op::HttpServer* server) {
    // User-data folder for WebView2 itself (cookies, cache) -- kept
    // separate from our own app data folder so a browser-engine cache
    // clear never touches the endpoints data. Built directly as a wide
    // string (not narrowed-then-widened) so it can't mangle non-ASCII
    // characters in the Windows user profile path.
    std::wstring userDataFolder = op::getAppSupportDirW() + L"\\webview2";

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, server](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    MessageBoxW(hwnd,
                        L"Couldn't start the embedded browser (WebView2).\n\n"
                        L"Install the WebView2 Runtime (Evergreen Bootstrapper) from Microsoft "
                        L"and try again.",
                        L"Office Endpoint Panel", MB_OK | MB_ICONERROR);
                    PostQuitMessage(1);
                    return S_OK;
                }

                // Environment must outlive the webview (webview_bridge.cpp
                // needs it to construct responses), so keep it alive for
                // the lifetime of the app.
                g_webviewEnvironment = env;

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, server](HRESULT result2, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result2) || !controller) {
                                MessageBoxW(hwnd, L"Failed to create the browser view.",
                                            L"Office Endpoint Panel", MB_OK | MB_ICONERROR);
                                PostQuitMessage(1);
                                return S_OK;
                            }

                            g_webviewController = controller;
                            g_webviewController->get_CoreWebView2(&g_webview);

                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
                                settings->put_AreDevToolsEnabled(FALSE);
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                            }

                            op::installWebViewBridge(g_webviewEnvironment.Get(), g_webview.Get(), server);

                            resizeWebviewToWindow(hwnd);
                            g_webview->Navigate((std::wstring(op::kVirtualOrigin) + L"/").c_str());
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // ---- wire up the route table (no socket, no port -- see file header) ----
    std::string basePath = op::getBasePath();
    op::DataStore store(op::getDataPath(basePath));
    store.load();
    op::SessionStore sessions;

    op::HttpServer server;
    op::RouteDeps deps;
    deps.store = &store;
    deps.sessions = &sessions;
    deps.basePath = basePath;
    deps.fetchUrl = op::winHttpFetch; // the app's only real network use: Sync from Sheets
    op::registerRoutes(server, deps);

    // ---- native window ----
    const wchar_t* className = L"OfficeEndpointPanelWindowClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, className, L"Office Endpoint Management Panel",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    initWebView2(hwnd, &server);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

#endif // _WIN32
