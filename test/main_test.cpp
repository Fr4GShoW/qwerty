// main_test.cpp
// Non-Windows dev entry point: runs the exact same core (http_server,
// routes, data_store, auth, sheets_logic) that ships in OfficePanel.exe,
// just without the WebView2 window -- open the printed URL in any browser
// instead. Useful for developing/debugging the app's logic on a Linux or
// macOS machine without needing Windows at all; NOT what gets shipped to
// end users (see main_win.cpp for that).
#include "http_server.h"
#include "routes.h"
#include "data_store.h"
#include "auth.h"
#include "paths.h"
#include "sheets_fetch_stub.h"

#include <iostream>
#include <csignal>
#include <cstring>

namespace {
op::HttpServer* g_server = nullptr;

void handleSigint(int) {
    if (g_server) g_server->stop();
}
}

int main(int argc, char** argv) {
    int port = 5050;
    if (argc > 1) {
        try { port = std::stoi(argv[1]); } catch (...) {}
    }

    std::string basePath = op::getExeDir() + "/web";
    // Fall back to the source tree's web/ folder if run straight from the
    // build directory (CMake doesn't copy it there automatically).
    if (!op::pathExists(basePath + "/index.html")) {
        basePath = std::string(SOURCE_DIR) + "/web";
    }

    op::DataStore store(op::getDataPath(basePath));
    store.load();
    op::SessionStore sessions;

    op::HttpServer server;
    op::RouteDeps deps;
    deps.store = &store;
    deps.sessions = &sessions;
    deps.basePath = basePath;
    deps.fetchUrl = op::curlFetch;
    op::registerRoutes(server, deps);

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to bind 127.0.0.1:" << port << " (already in use?)\n";
        return 1;
    }

    g_server = &server;
    std::signal(SIGINT, handleSigint);
    std::signal(SIGTERM, handleSigint);

    std::cout << "==================================================\n";
    std::cout << "  Office Endpoint Panel (dev/test build)\n";
    std::cout << "==================================================\n";
    std::cout << "  Serving from: " << basePath << "\n";
    std::cout << "  Data dir:     " << store.dataDir() << "\n";
    std::cout << "  Open:         http://127.0.0.1:" << port << "/\n";
    std::cout << "  Ctrl+C to stop.\n";
    std::cout << "==================================================\n";

    server.run(); // blocks until stop() is called (from the signal handler)

    std::cout << "\nStopped.\n";
    return 0;
}
