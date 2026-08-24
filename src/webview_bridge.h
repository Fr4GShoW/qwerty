// webview_bridge.h
// Windows-only. Routes all traffic between the WebView2-hosted UI and our
// existing HttpServer routing/business logic WITHOUT ever opening a TCP
// socket or listening on any port -- no localhost server at all.
//
// How: WebView2 lets you intercept every request made to a virtual,
// made-up hostname (WebResourceRequested) and hand back a response you
// construct yourself, entirely in-process. The page navigates to
// https://officepanel.local/ -- a URL that looks real to the page and to
// index.html's existing `fetch('/api/...')` calls, cookie handling, etc
// -- but WebView2 never actually opens a network connection for it; every
// request is caught here and answered directly from op::HttpServer's
// existing route table (the exact same dispatch() already covered by the
// test suite in test/, just reached a different way).
#pragma once

#ifdef _WIN32

// WebView2.h's generated COM interface declarations (`interface
// ICoreWebView2;` etc.) rely on the `interface` keyword, which is only
// defined (as `#define interface struct`) once <unknwn.h> has been
// processed. <windows.h> normally pulls that in transitively via
// <ole2.h> -- but WIN32_LEAN_AND_MEAN (defined project-wide, see
// CMakeLists.txt) deliberately excludes that COM/OLE machinery to keep
// builds lean, and this file is COM-heavy, so it needs to ask for it
// back explicitly. Get this order wrong (or omit <unknwn.h>) and every
// interface declaration in WebView2.h fails to parse with a wall of
// "missing type specifier" / "syntax error before identifier" errors.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include "http_server.h"
#include "WebView2.h"

namespace op {

// The virtual origin the page is navigated to and all its relative
// fetch() calls resolve against. Using an https:// scheme (even though no
// real TLS/network handshake ever happens) matches Microsoft's own
// documented convention for this pattern and avoids any mixed-content
// surprises if the page ever expects a secure context.
extern const wchar_t* kVirtualHost;      // L"officepanel.local"
extern const wchar_t* kVirtualOrigin;    // L"https://officepanel.local"

// Registers the WebResourceRequested interception on `webview`, answering
// every request against `server`'s route table. Call this once, after the
// ICoreWebView2 (and its owning Environment) are created and before the
// first Navigate(). `env` is needed because constructing a response object
// is an Environment method, not a webview method.
void installWebViewBridge(ICoreWebView2Environment* env, ICoreWebView2* webview, HttpServer* server);

} // namespace op

#endif // _WIN32
