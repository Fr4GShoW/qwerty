// routes.h
// Registers every route from the original server.py onto an HttpServer:
// login/session endpoints, the endpoints CRUD API, stats/search/export,
// and the Sync-from-Sheets refresh. Also serves index.html/login.html and
// the bundled data files.
//
// The Google Sheets fetch function is injected rather than called directly,
// so this file has zero dependency on WinHTTP/libcurl/etc and can be
// compiled and tested on any platform -- only main_win.cpp (Windows) wires
// in the real network fetch; the Linux test harness wires in a fixture
// fetch instead.
#pragma once

#include "http_server.h"
#include "data_store.h"
#include "auth.h"
#include <functional>
#include <string>

namespace op {

// Fetches a URL and returns true+body on success, or false+error message.
using FetchFn = std::function<bool(const std::string& url, std::string& outBody, std::string& outError)>;

struct RouteDeps {
    DataStore* store;
    SessionStore* sessions;
    std::string basePath; // bundled web/ folder (index.html, login.html, web/data defaults)
    FetchFn fetchUrl;      // platform-specific Google Sheets fetch
};

void registerRoutes(HttpServer& server, RouteDeps deps);

} // namespace op
