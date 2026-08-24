// http_server.h
// A small single-purpose HTTP/1.1 server: just enough to serve this app's
// static files and JSON API. Thread-per-connection (fine for a local,
// single-user desktop app talking to itself). No third-party dependency --
// raw sockets, #ifdef'd between Winsock2 (Windows) and BSD sockets (POSIX,
// used only by the Linux test harness in this project, never shipped in the
// Windows build).
#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>
#include <atomic>

namespace op {

struct HttpRequest {
    std::string method;                        // "GET", "POST", "DELETE", ...
    std::string path;                           // decoded path, no query string
    std::string query;                          // raw query string (after '?'), may be empty
    std::map<std::string, std::string> headers;  // lowercase keys
    std::map<std::string, std::string> cookies;
    std::map<std::string, std::string> params;   // path params captured via ":name"/"*name"
    std::string body;

    std::string queryParam(const std::string& key, const std::string& def = "") const;
};

struct HttpResponse {
    int status = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers; // Content-Type set automatically if you use the json()/text() helpers
    std::vector<std::string> setCookies;
    std::string body;

    void json(const std::string& jsonBody, int statusCode = 200);
    void text(const std::string& textBody, int statusCode = 200, const std::string& contentType = "text/plain; charset=utf-8");
    void file(const std::string& fileBody, const std::string& contentType);
    void redirect(const std::string& location);
};

using Handler = std::function<void(const HttpRequest&, HttpResponse&)>;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    void get(const std::string& pattern, Handler h);
    void post(const std::string& pattern, Handler h);
    void del(const std::string& pattern, Handler h);
    // Catch-all fallback (used for serving arbitrary static files by path).
    void fallback(Handler h);

    // Binds and listens; returns false on failure (port already in use, etc).
    bool listen(const std::string& host, int port);

    // Blocking accept loop -- call this from a background thread.
    void run();

    // Signals run() to stop and unblocks the accept() call.
    void stop();

    // Routes a request through the registered handlers and fills `res`.
    // Public so a non-socket transport (e.g. the Windows GUI build's
    // WebView2 resource-interception bridge, which never opens a port at
    // all) can reuse the exact same routing/business logic as the
    // socket-based server used by listen()/run(). This is the same method
    // handleConnection() calls internally for real socket traffic.
    void dispatch(const HttpRequest& req, HttpResponse& res);

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments; // pattern split by '/'
        Handler handler;
    };

    bool matchRoute(const Route& route, const std::string& method, const std::string& path,
                     std::map<std::string, std::string>& outParams) const;
    void handleConnection(intptr_t clientSocketRaw);

    std::vector<Route> routes_;
    Handler fallback_;

    intptr_t listenSocketRaw_ = -1;
    std::atomic<bool> running_{false};
};

// --- small helpers shared with routes.cpp ---
std::string urlDecode(const std::string& s);
std::map<std::string, std::string> parseQueryString(const std::string& query);
std::string guessContentType(const std::string& path);

} // namespace op
