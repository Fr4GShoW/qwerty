// sheets_logic.cpp
#include "sheets_logic.h"
#include <algorithm>
#include <cctype>

namespace op {

const std::string kSheetId = "1HT_hCz-xttdhq8eMxsJ1FNvFAc6NTz2YjySnkvHOzz4";

const std::vector<DepartmentSheet> kDepartments = {
    {"Safety_Maintenance",  "1839867226"},
    {"Load_Acquisition",    "1407460029"},
    {"Operations",          "595290834"},
    {"HR",                  "1205797397"},
    {"Accounting_AfterHours", "1610790005"},
};

std::string sheetCsvUrl(const std::string& gid) {
    return "https://docs.google.com/spreadsheets/d/" + kSheetId + "/export?format=csv&gid=" + gid;
}

namespace {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n\f\v");
    return s.substr(start, end - start + 1);
}

// Equivalent of Python's re.sub(r'\s+', '', raw_loc) -- removes ALL
// whitespace anywhere in the string, not just leading/trailing.
std::string removeAllWhitespace(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) out += c;
    }
    return out;
}

std::string getCol(const std::vector<std::string>& row, size_t idx) {
    if (idx < row.size()) return row[idx];
    return "";
}

bool isValidVal(const std::string& v) {
    if (v.empty()) return false;
    std::string upper = toUpper(trim(v));
    static const std::vector<std::string> placeholders = {
        "NA", "N/A", "NAN", "LIBER", "NONE", "-", "UNASSIGNED", "NO USER", "NO PC", "???", ""
    };
    for (auto& p : placeholders) if (upper == p) return false;
    return true;
}

std::string cleanStr(const std::string& v) {
    return isValidVal(v) ? trim(v) : "";
}

} // namespace

ZoneInfo zoneForLocation(const std::string& location) {
    std::string u = toUpper(location);
    auto starts = [&](const char* prefix) { return u.rfind(prefix, 0) == 0; };

    if (starts("LA")) return {"Load Acquisition", "#4CAF50"};
    if (starts("SF")) return {"Safety", "#2196F3"};
    if (starts("MN")) return {"Maintenance", "#9C27B0"};
    if (starts("OP")) return {"Operations", "#FF9800"};
    if (starts("AC")) return {"Accounting", "#F44336"};
    if (starts("AH")) return {"After Hours", "#795548"};
    if (starts("HR")) return {"HR", "#E91E63"};
    if (starts("MM")) return {"Management", "#607D8B"};
    return {"Other", "#9E9E9E"};
}

std::vector<Json> parseDepartmentRows(
    const std::vector<std::vector<std::string>>& rows,
    const std::string& departmentName,
    long long& nextId) {

    std::vector<Json> endpoints;

    for (const auto& row : rows) {
        std::string rawLoc = getCol(row, 0);
        std::string rawLocTrim = trim(rawLoc);

        if (rawLocTrim.empty()) continue;
        std::string lowerLoc = rawLocTrim;
        std::transform(lowerLoc.begin(), lowerLoc.end(), lowerLoc.begin(), ::tolower);
        static const std::vector<std::string> headerNames = {
            "location", "nr. locului de muncă", "nr.", "nr. locului", "pc place"
        };
        bool isHeader = false;
        for (auto& h : headerNames) if (lowerLoc == h) { isHeader = true; break; }
        if (isHeader) continue;

        std::string location = removeAllWhitespace(rawLocTrim);
        if (toUpper(location).rfind("HH", 0) == 0) {
            location = "HR" + location.substr(2);
        }

        std::string inventoryNumber = cleanStr(getCol(row, 1));
        std::string equipmentType  = cleanStr(getCol(row, 2));
        std::string pcNameColD     = cleanStr(getCol(row, 3));
        std::string userColE       = cleanStr(getCol(row, 4));

        std::string resolvedPcName = !pcNameColD.empty() ? pcNameColD : equipmentType;

        std::string resolvedUser;
        std::string status;
        if (!userColE.empty()) {
            resolvedUser = userColE;
            status = "active";
        } else if (!pcNameColD.empty()) {
            resolvedUser = "Unassigned";
            status = "available";
        } else {
            resolvedUser = "Unassigned";
            status = "empty";
        }

        Json monitors = Json::array();
        const int monitorCols[3][2] = {{12, 13}, {14, 15}, {16, 17}};
        for (auto& pr : monitorCols) {
            std::string model = cleanStr(getCol(row, static_cast<size_t>(pr[0])));
            std::string num = cleanStr(getCol(row, static_cast<size_t>(pr[1])));
            if (!model.empty() && !num.empty()) {
                monitors.push_back(Json::string(model + " (" + num + ")"));
            } else if (!model.empty()) {
                monitors.push_back(Json::string(model));
            } else if (!num.empty()) {
                monitors.push_back(Json::string(num));
            }
        }

        Json ep = Json::object();
        ep.set("id", Json::number(nextId));
        ep.set("location", Json::string(location));
        ep.set("inventory_number", Json::string(inventoryNumber));
        ep.set("pc_name", Json::string(resolvedPcName));
        ep.set("user", Json::string(resolvedUser));
        ep.set("user_name", Json::string(userColE));
        ep.set("full_name", Json::string(userColE));
        ep.set("ram_size", Json::string(cleanStr(getCol(row, 5))));
        ep.set("ram_mhz", Json::string(cleanStr(getCol(row, 6))));
        ep.set("ram_brand", Json::string(cleanStr(getCol(row, 7))));
        ep.set("cpu", Json::string(cleanStr(getCol(row, 8))));
        ep.set("motherboard", Json::string(cleanStr(getCol(row, 9))));
        ep.set("chipset", Json::string(cleanStr(getCol(row, 10))));
        ep.set("windows", Json::string(cleanStr(getCol(row, 11))));
        ep.set("os", Json::string(cleanStr(getCol(row, 11))));
        ep.set("monitors", monitors);
        ep.set("comment", Json::string(cleanStr(getCol(row, 18))));
        ep.set("mount", Json::string(cleanStr(getCol(row, 20))));
        ep.set("mount_type", Json::string(cleanStr(getCol(row, 21))));
        ep.set("chair", Json::string(cleanStr(getCol(row, 22))));
        ep.set("table", Json::string(cleanStr(getCol(row, 23))));
        ep.set("date_start", Json::string(cleanStr(getCol(row, 25))));
        ep.set("status", Json::string(status));
        ep.set("department", Json::string(departmentName));

        ZoneInfo zone = zoneForLocation(location);
        ep.set("zone", Json::string(zone.name));
        ep.set("zone_color", Json::string(zone.color));

        endpoints.push_back(ep);
        ++nextId;
    }

    return endpoints;
}

Json generateOfficeLayout() {
    Json layout = Json::object();
    layout.set("width", Json::number(1200));
    layout.set("height", Json::number(900));

    Json zones = Json::array();

    auto makeZone = [](const std::string& id, const std::string& name, int x, int y, int w, int h, const std::string& color) {
        Json z = Json::object();
        z.set("id", Json::string(id));
        z.set("name", Json::string(name));
        z.set("x", Json::number(x));
        z.set("y", Json::number(y));
        z.set("width", Json::number(w));
        z.set("height", Json::number(h));
        z.set("color", Json::string(color));
        z.set("desks", Json::array());
        return z;
    };

    Json la = makeZone("la", "Load Acquisition", 20, 20, 200, 160, "#4CAF50");
    Json sf = makeZone("sf", "Safety", 250, 100, 200, 80, "#2196F3");
    Json mn = makeZone("mn", "Maintenance", 480, 100, 100, 80, "#9C27B0");

    Json hr = makeZone("hr", "HR", 20, 210, 100, 160, "#E91E63");
    {
        Json& desks = hr["desks"];
        struct D { const char* id; int x; int y; int row; };
        const D hrDesks[] = {
            {"HR1-1", 25, 25, 1}, {"HR1-2", 25, 55, 1}, {"HR1-3", 25, 85, 1},
            {"HR2-1", 55, 25, 2}, {"HR2-2", 55, 55, 2},
            {"HR3-1", 85, 25, 3}, {"HR3-2", 85, 55, 3},
        };
        for (auto& d : hrDesks) {
            Json desk = Json::object();
            desk.set("id", Json::string(d.id));
            desk.set("x", Json::number(d.x));
            desk.set("y", Json::number(d.y));
            desk.set("row", Json::number(d.row));
            desks.push_back(desk);
        }
    }

    Json opZone = makeZone("op", "Operations", 140, 210, 750, 160, "#FF9800");
    {
        Json& desks = opZone["desks"];
        for (int col = 1; col <= 12; ++col) {
            for (int row = 1; row <= 7; ++row) {
                Json desk = Json::object();
                desk.set("id", Json::string("OP" + std::to_string(col) + "-" + std::to_string(row)));
                desk.set("x", Json::number(20 + (col - 1) * 60));
                desk.set("y", Json::number(20 + (row - 1) * 20));
                desk.set("row", Json::number(row));
                desks.push_back(desk);
            }
        }
    }

    Json mm = makeZone("mm", "Management", 340, 400, 150, 100, "#607D8B");

    Json ah = makeZone("ah", "After Hours", 530, 400, 180, 100, "#795548");
    {
        Json& desks = ah["desks"];
        struct D { const char* id; int x; int y; int row; };
        const D ahDesks[] = {
            {"AH1-1", 10, 15, 1}, {"AH1-2", 50, 15, 1}, {"AH1-3", 100, 15, 1}, {"AH1-4", 140, 15, 1},
            {"AH1-5", 10, 45, 2}, {"AH1-6", 50, 45, 2}, {"AH1-7", 100, 45, 2}, {"AH1-8", 140, 45, 2},
        };
        for (auto& d : ahDesks) {
            Json desk = Json::object();
            desk.set("id", Json::string(d.id));
            desk.set("x", Json::number(d.x));
            desk.set("y", Json::number(d.y));
            desk.set("row", Json::number(d.row));
            desks.push_back(desk);
        }
    }

    Json ac = makeZone("ac", "Accounting", 530, 510, 180, 200, "#F44336");
    {
        Json& desks = ac["desks"];
        for (int col = 1; col <= 3; ++col) {
            for (int row = 1; row <= 8; ++row) {
                Json desk = Json::object();
                desk.set("id", Json::string("AC" + std::to_string(col) + "-" + std::to_string(row)));
                desk.set("x", Json::number(20 + (col - 1) * 55));
                desk.set("y", Json::number(15 + (row - 1) * 22));
                desk.set("row", Json::number(row));
                desks.push_back(desk);
            }
        }
    }

    zones.push_back(la);
    zones.push_back(sf);
    zones.push_back(mn);
    zones.push_back(hr);
    zones.push_back(opZone);
    zones.push_back(mm);
    zones.push_back(ah);
    zones.push_back(ac);

    layout.set("zones", zones);
    return layout;
}

Json computeStats(const Json& endpointsArray) {
    long long total = 0, active = 0, available = 0, empty = 0, substituted = 0;
    for (auto& ep : endpointsArray.elements()) {
        ++total;
        std::string status = ep.get("status").asString();
        if (status == "active") ++active;
        else if (status == "available") ++available;
        else if (status == "empty") ++empty;
        else if (status == "substituted") ++substituted;
    }
    Json stats = Json::object();
    stats.set("total_endpoints", Json::number(total));
    stats.set("active", Json::number(active));
    stats.set("available", Json::number(available));
    stats.set("empty", Json::number(empty));
    stats.set("substituted", Json::number(substituted));
    return stats;
}

} // namespace op
