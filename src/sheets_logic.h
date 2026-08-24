// sheets_logic.h
// Portable re-implementation of data_parser.py's row -> endpoint mapping and
// zone/status rules. Deliberately has NO networking code in it (that lives
// in sheets_fetch_win.cpp / sheets_fetch_stub.cpp) so this half -- the part
// that actually encodes the business rules and is easy to get subtly wrong
// -- can be compiled and unit-tested on any platform.
#pragma once

#include <string>
#include <vector>
#include "json.h"

namespace op {

// One Google Sheet tab to pull, mirroring DEPARTMENTS in data_parser.py.
struct DepartmentSheet {
    std::string name;
    std::string gid;
};

extern const std::string kSheetId;
extern const std::vector<DepartmentSheet> kDepartments;

std::string sheetCsvUrl(const std::string& gid);

// { zoneName, zoneColor } for a location's prefix (LA/SF/MN/OP/AC/AH/HR/MM/other).
struct ZoneInfo {
    std::string name;
    std::string color;
};
ZoneInfo zoneForLocation(const std::string& location);

// Converts the raw CSV rows of a single department tab into endpoint JSON
// objects, exactly matching data_parser.py's column mapping and
// active/available/empty status rules. `nextId` is read as the starting id
// and updated in place as rows are consumed (mirrors global_id in Python).
std::vector<Json> parseDepartmentRows(
    const std::vector<std::vector<std::string>>& rows,
    const std::string& departmentName,
    long long& nextId);

// Static floor-plan geometry, kept for API parity with the original backend
// (the current frontend computes its own layout from `location` strings and
// doesn't consume this, but other API consumers might).
Json generateOfficeLayout();

// Recomputes stats (total/active/available/empty/substituted) from an
// endpoints array, matching the shape server.py wrote into office_data.json.
Json computeStats(const Json& endpointsArray);

} // namespace op
