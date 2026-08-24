// routes.cpp
#include "routes.h"
#include "sheets_logic.h"
#include "csv.h"
#include "paths.h"
#include <algorithm>
#include <ctime>
#include <sstream>
#include <memory>

namespace op {

namespace {

std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n\f\v");
    return s.substr(start, end - start + 1);
}

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
    return std::string(buf);
}

Json parseBodyOrEmpty(const std::string& body) {
    if (body.empty()) return Json::object();
    try {
        Json j = Json::parse(body);
        if (j.isObject()) return j;
    } catch (...) {
        // fall through
    }
    return Json::object();
}

std::string sessionCookieFromRequest(const HttpRequest& req) {
    auto it = req.cookies.find("op_session");
    return it != req.cookies.end() ? it->second : "";
}

bool requireAuth(RouteDeps& deps, const HttpRequest& req, HttpResponse& res,
                  std::string& username, std::string& role) {
    std::string token = sessionCookieFromRequest(req);
    if (deps.sessions->lookup(token, username, role)) return true;
    res.json(R"({"error":"Unauthorized","login_required":true})", 401);
    return false;
}

bool isMeaningfulUser(const std::string& user) {
    if (user.empty()) return false;
    std::string lower = toLowerStr(user);
    return lower != "unassigned" && lower != "nan" && lower != "liber" && lower != "";
}

void applyUpdateFields(Json& endpoint, const Json& data) {
    // Mirrors: for key,value in data.items(): if key not in ['id','location','zone','zone_color']: endpoints[i][key] = value
    static const std::vector<std::string> excluded = {"id", "location", "zone", "zone_color"};
    for (auto& kv : data.items()) {
        if (std::find(excluded.begin(), excluded.end(), kv.first) == excluded.end()) {
            endpoint.set(kv.first, kv.second);
        }
    }
}

std::string recomputeUpdateStatus(const Json& endpoint, const Json& data) {
    std::string user = endpoint.get("user").asString(); // == data.get('user', endpoints[i].get('user',''))
    std::string substitutePc = data.get("substitute_pc_name").asString(); // strictly from request body, matches original

    if (!trimStr(substitutePc).empty()) return "substituted";
    if (isMeaningfulUser(user)) return "active";
    if (!endpoint.get("pc_name").asString().empty() || !data.get("pc_name").asString().empty()) return "available";
    return "empty";
}

std::string readFileOr404(const std::string& path, HttpResponse& res, const std::string& contentType) {
    bool ok = false;
    std::string content = readFile(path, ok);
    if (!ok) {
        res.json(R"({"error":"Not found"})", 404);
        return "";
    }
    res.file(content, contentType);
    return content;
}

} // namespace

void registerRoutes(HttpServer& server, RouteDeps deps) {
    auto depsPtr = std::make_shared<RouteDeps>(std::move(deps));

    // ---------------- auth ----------------

    server.get("/login", [depsPtr](const HttpRequest&, HttpResponse& res) {
        readFileOr404(joinPath(depsPtr->basePath, "login.html"), res, "text/html; charset=utf-8");
    });

    server.post("/api/login", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        Json data = parseBodyOrEmpty(req.body);
        std::string username = data.get("username").asString();
        std::string password = data.get("password").asString();

        const UserAccount* acc = verifyLogin(username, password);
        if (acc) {
            std::string token = depsPtr->sessions->create(acc->username, acc->role);
            res.setCookies.push_back("op_session=" + token + "; Path=/; HttpOnly; SameSite=Lax");
            Json out = Json::object();
            out.set("success", Json::boolean(true));
            out.set("user", Json::string(acc->username));
            out.set("role", Json::string(acc->role));
            res.json(out.dump());
        } else {
            Json out = Json::object();
            out.set("success", Json::boolean(false));
            out.set("error", Json::string("Invalid credentials"));
            res.json(out.dump(), 401);
        }
    });

    server.post("/api/logout", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string token = sessionCookieFromRequest(req);
        if (!token.empty()) depsPtr->sessions->destroy(token);
        res.setCookies.push_back("op_session=; Path=/; HttpOnly; Max-Age=0");
        res.json(R"({"success":true})");
    });

    server.get("/api/check-auth", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        std::string token = sessionCookieFromRequest(req);
        if (depsPtr->sessions->lookup(token, username, role)) {
            Json out = Json::object();
            out.set("authenticated", Json::boolean(true));
            out.set("user", Json::string(username));
            out.set("role", Json::string(role));
            res.json(out.dump());
        } else {
            res.json(R"({"authenticated":false})");
        }
    });

    // ---------------- main page ----------------

    server.get("/", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        std::string token = sessionCookieFromRequest(req);
        if (!depsPtr->sessions->lookup(token, username, role)) {
            res.redirect("/login");
            return;
        }
        readFileOr404(joinPath(depsPtr->basePath, "index.html"), res, "text/html; charset=utf-8");
    });

    // ---------------- data files (writable dir) ----------------

    server.get("/office_panel/data/*filename", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string filename = req.params.at("filename");
        readFileOr404(joinPath(depsPtr->store->dataDir(), filename), res, guessContentType(filename));
    });

    // ---------------- endpoints CRUD ----------------

    server.get("/api/endpoints", [depsPtr](const HttpRequest&, HttpResponse& res) {
        res.json(depsPtr->store->combinedForApi().dump());
    });

    server.get("/api/endpoints/:location", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string location = req.params.at("location");
        long long idx = depsPtr->store->findIndexByLocation(location);
        if (idx < 0) {
            res.json(R"({"error":"Endpoint not found"})", 404);
            return;
        }
        res.json(depsPtr->store->endpoints().at(static_cast<size_t>(idx)).dump());
    });

    server.post("/api/endpoints/update", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        Json data = parseBodyOrEmpty(req.body);
        long long id = data.get("id").asInt(-1);
        std::string location = data.get("location").asString();

        long long idx = -1;
        if (data.has("id")) idx = depsPtr->store->findIndexById(id);
        if (idx < 0 && !location.empty()) idx = depsPtr->store->findIndexByLocation(location);

        if (idx < 0) {
            res.json(R"({"error":"Endpoint not found"})", 404);
            return;
        }

        Json& endpoint = depsPtr->store->endpoints().at(static_cast<size_t>(idx));
        applyUpdateFields(endpoint, data);
        endpoint.set("status", Json::string(recomputeUpdateStatus(endpoint, data)));
        endpoint.set("last_updated", Json::string(nowIso()));
        endpoint.set("updated_by", Json::string(username));

        depsPtr->store->save();
        res.json(R"({"success":true,"message":"Endpoint updated successfully"})");
    });

    server.post("/api/endpoints/remove-substitute", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        Json data = parseBodyOrEmpty(req.body);
        std::string location = data.get("location").asString();
        long long idx = depsPtr->store->findIndexByLocation(location);
        if (idx < 0) {
            res.json(R"({"error":"Endpoint not found"})", 404);
            return;
        }

        Json& endpoint = depsPtr->store->endpoints().at(static_cast<size_t>(idx));
        endpoint.set("substitute_pc_name", Json::string(""));
        endpoint.set("substitute_pc", Json::string(""));
        endpoint.set("substitute_owner", Json::string(""));
        endpoint.set("substitute_reason", Json::string(""));

        std::string user = endpoint.get("user").asString();
        std::string status;
        if (isMeaningfulUser(user)) status = "active";
        else if (!endpoint.get("pc_name").asString().empty()) status = "available";
        else status = "empty";
        endpoint.set("status", Json::string(status));
        endpoint.set("last_updated", Json::string(nowIso()));
        endpoint.set("updated_by", Json::string(username));

        depsPtr->store->save();
        Json out = Json::object();
        out.set("success", Json::boolean(true));
        out.set("endpoint", endpoint);
        res.json(out.dump());
    });

    server.post("/api/endpoints/replace-component", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        Json data = parseBodyOrEmpty(req.body);
        std::string location = data.get("location").asString();
        std::string component = data.get("component").asString();
        std::string oldValue = data.get("old_value").asString();
        std::string newValue = data.get("new_value").asString();
        std::string reason = data.get("reason").asString();

        long long idx = depsPtr->store->findIndexByLocation(location);
        if (idx < 0) {
            res.json(R"({"error":"Endpoint not found"})", 404);
            return;
        }

        Json& endpoint = depsPtr->store->endpoints().at(static_cast<size_t>(idx));
        if (!endpoint.has("replacement_history")) {
            endpoint.set("replacement_history", Json::array());
        }
        Json replacement = Json::object();
        replacement.set("date", Json::string(nowIso()));
        replacement.set("component", Json::string(component));
        replacement.set("old_value", Json::string(oldValue));
        replacement.set("new_value", Json::string(newValue));
        replacement.set("reason", Json::string(reason));
        replacement.set("replaced_by", Json::string(username));

        Json& history = endpoint["replacement_history"];
        history.push_back(replacement);

        static const std::vector<std::pair<std::string, std::string>> componentMap = {
            {"cpu", "cpu"}, {"ram", "ram_size"}, {"motherboard", "motherboard"},
            {"hdd", "hdd"}, {"ssd", "ssd"}, {"psu", "psu"},
            {"ram_brand", "ram_brand"}, {"ram_mhz", "ram_mhz"},
        };
        for (auto& kv : componentMap) {
            if (kv.first == component) {
                endpoint.set(kv.second, Json::string(newValue));
                break;
            }
        }

        endpoint.set("last_updated", Json::string(nowIso()));
        endpoint.set("updated_by", Json::string(username));

        depsPtr->store->save();
        Json out = Json::object();
        out.set("success", Json::boolean(true));
        out.set("endpoint", endpoint);
        res.json(out.dump());
    });

    server.post("/api/endpoints/create", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        Json data = parseBodyOrEmpty(req.body);
        data.set("id", Json::number(depsPtr->store->nextEndpointId()));
        if (!data.has("status")) data.set("status", Json::string("empty"));

        std::string location = data.get("location").asString();
        ZoneInfo zone = zoneForLocation(location);
        data.set("zone", Json::string(zone.name));
        data.set("zone_color", Json::string(zone.color));
        data.set("created_at", Json::string(nowIso()));

        depsPtr->store->endpoints().push_back(data);
        depsPtr->store->save();

        Json out = Json::object();
        out.set("success", Json::boolean(true));
        out.set("endpoint", data);
        res.json(out.dump());
    });

    server.del("/api/endpoints/delete/:id", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        long long id = 0;
        try { id = std::stoll(req.params.at("id")); } catch (...) {}

        auto& arr = depsPtr->store->endpoints().elements();
        arr.erase(std::remove_if(arr.begin(), arr.end(), [id](const Json& e) {
            return e.get("id").asInt() == id;
        }), arr.end());

        depsPtr->store->save();
        res.json(R"({"success":true,"message":"Endpoint deleted"})");
    });

    // ---------------- stats / search / export ----------------

    server.get("/api/stats", [depsPtr](const HttpRequest&, HttpResponse& res) {
        Json stats = Json::object();
        long long total = 0, active = 0, available = 0, empty = 0, substituted = 0;
        Json byZone = Json::object();
        Json byOs = Json::object();

        for (auto& e : depsPtr->store->endpoints().elements()) {
            ++total;
            std::string status = e.get("status").asString();
            if (status == "active") ++active;
            else if (status == "available") ++available;
            else if (status == "empty") ++empty;
            else if (status == "substituted") ++substituted;

            std::string zone = e.has("zone") ? e.get("zone").asString() : "Other";
            if (zone.empty()) zone = "Other";
            byZone.set(zone, Json::number(byZone.get(zone).asInt(0) + 1));

            std::string osVer = e.has("os") ? e.get("os").asString() : "";
            if (!osVer.empty() && osVer != "nan") {
                byOs.set(osVer, Json::number(byOs.get(osVer).asInt(0) + 1));
            }
        }

        stats.set("total", Json::number(total));
        stats.set("active", Json::number(active));
        stats.set("available", Json::number(available));
        stats.set("empty", Json::number(empty));
        stats.set("substituted", Json::number(substituted));
        stats.set("by_zone", byZone);
        stats.set("by_os", byOs);
        res.json(stats.dump());
    });

    server.get("/api/search", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string query = toLowerStr(req.queryParam("q"));
        if (query.empty()) {
            res.json("[]");
            return;
        }
        Json results = Json::array();
        for (auto& e : depsPtr->store->endpoints().elements()) {
            auto field = [&](const char* name) { return toLowerStr(e.get(name).asString()); };
            if (field("location").find(query) != std::string::npos ||
                field("user").find(query) != std::string::npos ||
                field("pc_name").find(query) != std::string::npos ||
                field("cpu").find(query) != std::string::npos ||
                field("os").find(query) != std::string::npos) {
                results.push_back(e);
            }
        }
        res.json(results.dump());
    });

    server.get("/api/export", [depsPtr](const HttpRequest&, HttpResponse& res) {
        bool ok = false;
        std::string path = joinPath(depsPtr->store->dataDir(), "office_data.json");
        std::string content = readFile(path, ok);
        if (!ok) {
            res.json(R"({"error":"No data available"})", 404);
            return;
        }
        res.json(content); // frontend triggers the actual browser download client-side via exportData()
        res.headers["Content-Disposition"] = "attachment; filename=\"office_endpoints_export.json\"";
    });

    // ---------------- sync from sheets ----------------

    server.post("/api/refresh", [depsPtr](const HttpRequest& req, HttpResponse& res) {
        std::string username, role;
        if (!requireAuth(*depsPtr, req, res, username, role)) return;

        if (!depsPtr->fetchUrl) {
            res.json(R"({"error":"No network fetch implementation configured"})", 500);
            return;
        }

        long long nextId = 1;
        std::vector<Json> allEndpoints;
        int succeeded = 0;
        int totalDepartments = static_cast<int>(kDepartments.size());

        for (auto& dept : kDepartments) {
            std::string body, err;
            bool ok = depsPtr->fetchUrl(sheetCsvUrl(dept.gid), body, err);
            if (!ok) {
                continue; // matches Python: log and skip this department, keep going
            }
            auto rows = parseCsv(body);
            auto endpoints = parseDepartmentRows(rows, dept.name, nextId);
            for (auto& e : endpoints) allEndpoints.push_back(e);
            ++succeeded;
        }

        Json arr = Json::array();
        for (auto& e : allEndpoints) arr.push_back(e);
        depsPtr->store->replaceAll(arr);

        std::ostringstream msg;
        msg << "Data refreshed live from Google Sheets (" << allEndpoints.size() << " endpoints";
        if (succeeded < totalDepartments) {
            msg << " -- warning: only " << succeeded << "/" << totalDepartments << " department tabs were reachable";
        }
        msg << ")";

        Json out = Json::object();
        out.set("success", Json::boolean(true));
        out.set("message", Json::string(msg.str()));
        res.json(out.dump());
    });

    // ---------------- static fallback ----------------

    server.fallback([depsPtr](const HttpRequest& req, HttpResponse& res) {
        if (req.method != "GET") {
            res.json(R"({"error":"Not found"})", 404);
            return;
        }
        std::string relative = req.path;
        if (!relative.empty() && relative[0] == '/') relative = relative.substr(1);
        std::string fullPath = joinPath(depsPtr->basePath, relative);
        if (pathExists(fullPath) && !isDirectory(fullPath)) {
            readFileOr404(fullPath, res, guessContentType(fullPath));
        } else {
            res.json(R"({"error":"Not found"})", 404);
        }
    });
}

} // namespace op
