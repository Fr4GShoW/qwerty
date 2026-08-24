// http_server.cpp
// NOMINMAX/WIN32_LEAN_AND_MEAN must come before ANY header that might
// transitively pull in windows.h (winsock2.h does, via windef.h) -- both
// are needed here since std::min() is used below at recvExact(); without
// NOMINMAX, windows.h's own "min"/"max" preprocessor macros silently
// mangle every std::min/std::max/numeric_limits<T>::max() call in this
// file into a syntax error (MSVC error C2589).
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
#endif
#include "http_server.h"
#include "json.h"
#include <sstream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    using raw_socket_t = SOCKET;
    static const raw_socket_t kInvalidSocket = INVALID_SOCKET;
    #define OP_CLOSESOCK closesocket
#else
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using raw_socket_t = int;
    static const raw_socket_t kInvalidSocket = -1;
    #define OP_CLOSESOCK close
#endif

namespace op {

// ---------------- small helpers ----------------

std::string urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '%' && i + 2 < s.size()) {
            auto hex = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                return -1;
            };
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
            out += c;
        } else if (c == '+') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

std::map<std::string, std::string> parseQueryString(const std::string& query) {
    std::map<std::string, std::string> result;
    std::istringstream iss(query);
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        if (pair.empty()) continue;
        size_t eq = pair.find('=');
        std::string key = urlDecode(eq == std::string::npos ? pair : pair.substr(0, eq));
        std::string val = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
        result[key] = val;
    }
    return result;
}

std::string guessContentType(const std::string& path) {
    auto endsWith = [&](const char* suffix) {
        size_t sl = std::strlen(suffix);
        return path.size() >= sl && path.compare(path.size() - sl, sl, suffix) == 0;
    };
    if (endsWith(".html")) return "text/html; charset=utf-8";
    if (endsWith(".js")) return "application/javascript; charset=utf-8";
    if (endsWith(".css")) return "text/css; charset=utf-8";
    if (endsWith(".json")) return "application/json; charset=utf-8";
    if (endsWith(".png")) return "image/png";
    if (endsWith(".jpg") || endsWith(".jpeg")) return "image/jpeg";
    if (endsWith(".svg")) return "image/svg+xml";
    if (endsWith(".ico")) return "image/x-icon";
    return "application/octet-stream";
}

std::string HttpRequest::queryParam(const std::string& key, const std::string& def) const {
    auto q = parseQueryString(query);
    auto it = q.find(key);
    return it != q.end() ? it->second : def;
}

void HttpResponse::json(const std::string& jsonBody, int statusCode) {
    status = statusCode;
    headers["Content-Type"] = "application/json; charset=utf-8";
    body = jsonBody;
}

void HttpResponse::text(const std::string& textBody, int statusCode, const std::string& contentType) {
    status = statusCode;
    headers["Content-Type"] = contentType;
    body = textBody;
}

void HttpResponse::file(const std::string& fileBody, const std::string& contentType) {
    status = 200;
    headers["Content-Type"] = contentType;
    body = fileBody;
}

void HttpResponse::redirect(const std::string& location) {
    status = 302;
    headers["Location"] = location;
    body.clear();
}

// ---------------- socket plumbing ----------------

namespace {

#ifdef _WIN32
struct WinsockInit {
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() { WSACleanup(); }
};
static WinsockInit g_winsockInit;
#endif

bool recvLine(raw_socket_t sock, std::string& line) {
    line.clear();
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return !line.empty();
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }
        line += c;
        if (line.size() > 8192) return true; // guard against pathological input
    }
}

bool recvExact(raw_socket_t sock, std::string& out, size_t count) {
    out.clear();
    out.reserve(count);
    char buf[4096];
    size_t remaining = count;
    while (remaining > 0) {
        int chunk = static_cast<int>(std::min(remaining, sizeof(buf)));
        int n = recv(sock, buf, chunk, 0);
        if (n <= 0) return false;
        out.append(buf, static_cast<size_t>(n));
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

void sendAll(raw_socket_t sock, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(sock, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

HttpServer::HttpServer() = default;
HttpServer::~HttpServer() {
    stop();
    if (listenSocketRaw_ != -1) {
        raw_socket_t s = static_cast<raw_socket_t>(listenSocketRaw_);
        OP_CLOSESOCK(s);
        listenSocketRaw_ = -1;
    }
}

void HttpServer::get(const std::string& pattern, Handler h) {
    Route r; r.method = "GET";
    std::istringstream iss(pattern);
    std::string seg;
    while (std::getline(iss, seg, '/')) if (!seg.empty()) r.segments.push_back(seg);
    r.handler = std::move(h);
    routes_.push_back(std::move(r));
}

void HttpServer::post(const std::string& pattern, Handler h) {
    Route r; r.method = "POST";
    std::istringstream iss(pattern);
    std::string seg;
    while (std::getline(iss, seg, '/')) if (!seg.empty()) r.segments.push_back(seg);
    r.handler = std::move(h);
    routes_.push_back(std::move(r));
}

void HttpServer::del(const std::string& pattern, Handler h) {
    Route r; r.method = "DELETE";
    std::istringstream iss(pattern);
    std::string seg;
    while (std::getline(iss, seg, '/')) if (!seg.empty()) r.segments.push_back(seg);
    r.handler = std::move(h);
    routes_.push_back(std::move(r));
}

void HttpServer::fallback(Handler h) {
    fallback_ = std::move(h);
}

bool HttpServer::matchRoute(const Route& route, const std::string& method, const std::string& path,
                             std::map<std::string, std::string>& outParams) const {
    if (route.method != method) return false;

    std::vector<std::string> pathSegs;
    std::istringstream iss(path);
    std::string seg;
    while (std::getline(iss, seg, '/')) if (!seg.empty()) pathSegs.push_back(seg);

    outParams.clear();
    size_t pi = 0;
    for (size_t ri = 0; ri < route.segments.size(); ++ri) {
        const std::string& rs = route.segments[ri];
        if (!rs.empty() && rs[0] == '*') {
            // Greedy: capture the rest of the path (rejoin with '/').
            std::string rest;
            for (size_t k = pi; k < pathSegs.size(); ++k) {
                if (!rest.empty()) rest += '/';
                rest += pathSegs[k];
            }
            outParams[rs.substr(1)] = rest;
            return true;
        }
        if (pi >= pathSegs.size()) return false;
        if (!rs.empty() && rs[0] == ':') {
            outParams[rs.substr(1)] = pathSegs[pi];
        } else if (rs != pathSegs[pi]) {
            return false;
        }
        ++pi;
    }
    return pi == pathSegs.size();
}

void HttpServer::dispatch(const HttpRequest& req, HttpResponse& res) {
    for (auto& route : routes_) {
        std::map<std::string, std::string> params;
        if (matchRoute(route, req.method, req.path, params)) {
            HttpRequest reqWithParams = req;
            reqWithParams.params = params;
            route.handler(reqWithParams, res);
            return;
        }
    }
    if (fallback_) {
        fallback_(req, res);
        return;
    }
    res.status = 404;
    res.json(R"({"error":"Not found"})", 404);
}

bool HttpServer::listen(const std::string& host, int port) {
#ifdef _WIN32
    raw_socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    raw_socket_t s = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (s == kInvalidSocket) return false;

    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    }

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        OP_CLOSESOCK(s);
        return false;
    }
    if (::listen(s, 32) != 0) {
        OP_CLOSESOCK(s);
        return false;
    }

    listenSocketRaw_ = static_cast<intptr_t>(s);
    running_ = true;
    return true;
}

void HttpServer::run() {
    raw_socket_t listenSock = static_cast<raw_socket_t>(listenSocketRaw_);
    while (running_) {
        // Poll with a short timeout instead of blocking in accept() forever --
        // closing a socket from another thread to unblock a pending accept()
        // is unreliable on POSIX and not guaranteed on Windows either. This
        // way stop() just needs to flip running_ and we notice within
        // ~200ms, no races with the OS's accept() implementation.
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSock, &readfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200ms
        int sel = select(static_cast<int>(listenSock) + 1, &readfds, nullptr, nullptr, &tv);
        if (sel <= 0) continue; // timed out or interrupted; re-check running_

        sockaddr_in clientAddr{};
#ifdef _WIN32
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        raw_socket_t client = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (client == kInvalidSocket) {
            continue;
        }
        intptr_t clientRaw = static_cast<intptr_t>(client);
        std::thread([this, clientRaw]() { handleConnection(clientRaw); }).detach();
    }
}

void HttpServer::stop() {
    // Just flip the flag -- run()'s select() loop notices within ~200ms and
    // returns on its own. Deliberately does NOT touch the socket here: the
    // run() thread may still be inside select()/accept() on it, and closing
    // a fd another thread is actively using is a race. The listening socket
    // is closed once, safely, in the destructor -- call stop() and join the
    // thread that's running run() before letting the HttpServer go out of
    // scope.
    running_ = false;
}

void HttpServer::handleConnection(intptr_t clientSocketRaw) {
    raw_socket_t client = static_cast<raw_socket_t>(clientSocketRaw);

#ifndef _WIN32
    int one = 1;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#endif

    std::string requestLine;
    if (!recvLine(client, requestLine) || requestLine.empty()) {
        OP_CLOSESOCK(client);
        return;
    }

    HttpRequest req;
    {
        std::istringstream iss(requestLine);
        std::string urlPart;
        iss >> req.method >> urlPart;
        size_t qpos = urlPart.find('?');
        if (qpos != std::string::npos) {
            req.path = urlDecode(urlPart.substr(0, qpos));
            req.query = urlPart.substr(qpos + 1);
        } else {
            req.path = urlDecode(urlPart);
        }
    }

    // Headers
    size_t contentLength = 0;
    while (true) {
        std::string line;
        if (!recvLine(client, line)) break;
        if (line.empty()) break; // blank line = end of headers
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLower(line.substr(0, colon));
        size_t vstart = colon + 1;
        while (vstart < line.size() && line[vstart] == ' ') ++vstart;
        std::string value = line.substr(vstart);
        req.headers[key] = value;
        if (key == "content-length") {
            try { contentLength = static_cast<size_t>(std::stoul(value)); } catch (...) {}
        }
        if (key == "cookie") {
            std::istringstream cs(value);
            std::string part;
            while (std::getline(cs, part, ';')) {
                size_t eq = part.find('=');
                if (eq == std::string::npos) continue;
                size_t s0 = part.find_first_not_of(' ');
                std::string name = part.substr(s0, eq - s0);
                std::string val = part.substr(eq + 1);
                req.cookies[name] = val;
            }
        }
    }

    if (contentLength > 0) {
        recvExact(client, req.body, contentLength);
    }

    HttpResponse res;
    dispatch(req, res);

    std::ostringstream out;
    out << "HTTP/1.1 " << res.status << " " << (res.statusText.empty() ? "OK" : res.statusText) << "\r\n";
    if (res.headers.find("Content-Type") == res.headers.end()) {
        res.headers["Content-Type"] = "text/plain; charset=utf-8";
    }
    for (auto& h : res.headers) {
        out << h.first << ": " << h.second << "\r\n";
    }
    for (auto& c : res.setCookies) {
        out << "Set-Cookie: " << c << "\r\n";
    }
    out << "Content-Length: " << res.body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "\r\n";
    out << res.body;

    sendAll(client, out.str());
    OP_CLOSESOCK(client);
}

} // namespace op
