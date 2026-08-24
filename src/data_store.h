// data_store.h
// Loads/saves endpoints.json and office_data.json in the persistent data
// directory (see paths.h), mirroring load_endpoints()/save_endpoints() from
// the original server.py: every save recomputes stats and keeps the
// combined office_data.json in sync with the endpoints array.
#pragma once

#include "json.h"
#include <string>

namespace op {

class DataStore {
public:
    explicit DataStore(std::string dataDir);

    // Reloads endpoints.json from disk into memory. Called once at startup
    // and again after a Sync-from-Sheets refresh.
    void load();

    // Current in-memory endpoints array (Json::Array of endpoint objects).
    Json& endpoints() { return endpoints_; }
    const Json& endpoints() const { return endpoints_; }

    // Persists endpoints_ to endpoints.json AND regenerates office_data.json
    // (endpoints + stats), matching save_endpoints()'s behavior.
    bool save();

    // Replaces endpoints_ wholesale (used by the Sync-from-Sheets refresh)
    // and immediately persists both files.
    bool replaceAll(Json newEndpoints);

    // The full combined object as returned by GET /api/endpoints:
    // { "endpoints": [...], "stats": {...} }. Recomputed from endpoints_
    // each call so it's always current even if save() hasn't run yet.
    Json combinedForApi() const;

    // Finds an endpoint by numeric id or by location string (server.py's
    // update route matches on either). Returns the index in endpoints_'s
    // array, or -1 if not found.
    long long findIndexById(long long id) const;
    long long findIndexByLocation(const std::string& location) const;

    long long nextEndpointId() const; // max existing id + 1, for create

    const std::string& dataDir() const { return dataDir_; }

private:
    std::string dataDir_;
    Json endpoints_;

    std::string endpointsPath() const;
    std::string officeDataPath() const;
};

} // namespace op
