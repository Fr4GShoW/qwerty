// data_store.cpp
#include "data_store.h"
#include "paths.h"
#include "sheets_logic.h"

namespace op {

DataStore::DataStore(std::string dataDir) : dataDir_(std::move(dataDir)) {
    endpoints_ = Json::array();
}

std::string DataStore::endpointsPath() const { return joinPath(dataDir_, "endpoints.json"); }
std::string DataStore::officeDataPath() const { return joinPath(dataDir_, "office_data.json"); }

void DataStore::load() {
    bool ok = false;
    std::string text = readFile(endpointsPath(), ok);
    if (ok) {
        try {
            Json parsed = Json::parse(text);
            if (parsed.isArray()) {
                endpoints_ = parsed;
                return;
            }
        } catch (...) {
            // fall through to empty array on parse failure
        }
    }
    endpoints_ = Json::array();
}

bool DataStore::save() {
    bool ok1 = writeFile(endpointsPath(), endpoints_.dump(true));

    Json combined = Json::object();
    combined.set("endpoints", endpoints_);
    combined.set("stats", computeStats(endpoints_));
    // Preserve a layout block for API parity with the original backend
    // (current frontend doesn't consume it, but keep the shape consistent).
    combined.set("layout", generateOfficeLayout());

    bool ok2 = writeFile(officeDataPath(), combined.dump(true));
    return ok1 && ok2;
}

bool DataStore::replaceAll(Json newEndpoints) {
    endpoints_ = std::move(newEndpoints);
    return save();
}

Json DataStore::combinedForApi() const {
    Json combined = Json::object();
    combined.set("endpoints", endpoints_);
    combined.set("stats", computeStats(endpoints_));
    combined.set("layout", generateOfficeLayout());
    return combined;
}

long long DataStore::findIndexById(long long id) const {
    const auto& arr = endpoints_.elements();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].get("id").asInt() == id) return static_cast<long long>(i);
    }
    return -1;
}

long long DataStore::findIndexByLocation(const std::string& location) const {
    const auto& arr = endpoints_.elements();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].get("location").asString() == location) return static_cast<long long>(i);
    }
    return -1;
}

long long DataStore::nextEndpointId() const {
    long long maxId = 0;
    for (auto& e : endpoints_.elements()) {
        long long id = e.get("id").asInt();
        if (id > maxId) maxId = id;
    }
    return maxId + 1;
}

} // namespace op
