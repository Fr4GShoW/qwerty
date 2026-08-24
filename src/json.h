// json.h
// A small, dependency-free JSON value type with parsing and serialization.
// Written from scratch so the whole project has zero third-party library
// downloads required (no nlohmann::json, no internet needed to fetch it) --
// only the JSON support this specific app actually needs.
//
// Supports: null, bool, number (stored as double, integers print without
// a trailing ".0"), string, array, object (insertion-order preserved).
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <cstdint>

namespace op {

enum class JsonType { Null, Bool, Number, String, Array, Object };

class Json {
public:
    JsonType type = JsonType::Null;

    Json() = default;

    static Json null() { return Json(); }
    static Json boolean(bool v) { Json j; j.type = JsonType::Bool; j.b_ = v; return j; }
    static Json number(double v) { Json j; j.type = JsonType::Number; j.num_ = v; return j; }
    static Json number(int v) { return number(static_cast<double>(v)); }
    static Json number(long long v) { return number(static_cast<double>(v)); }
    static Json string(std::string v) { Json j; j.type = JsonType::String; j.str_ = std::move(v); return j; }
    static Json array() { Json j; j.type = JsonType::Array; return j; }
    static Json object() { Json j; j.type = JsonType::Object; return j; }

    bool isNull() const { return type == JsonType::Null; }
    bool isObject() const { return type == JsonType::Object; }
    bool isArray() const { return type == JsonType::Array; }
    bool isString() const { return type == JsonType::String; }

    // ---- object access ----
    bool has(const std::string& key) const;
    // Read-only lookup; returns a null Json if missing (never throws).
    const Json& get(const std::string& key) const;
    // Write access; creates the key with a null value if it doesn't exist.
    Json& operator[](const std::string& key);
    void set(const std::string& key, Json value); // insert or overwrite, preserves original position

    // ---- array access ----
    void push_back(Json value);
    size_t size() const;
    const Json& at(size_t index) const;
    Json& at(size_t index); // mutable, for in-place field updates on an array element

    // ---- typed getters with defaults (never throw) ----
    std::string asString(const std::string& def = "") const;
    double asDouble(double def = 0.0) const;
    long long asInt(long long def = 0) const;
    bool asBool(bool def = false) const;

    const std::vector<std::pair<std::string, Json>>& items() const { return obj_; }
    const std::vector<Json>& elements() const { return arr_; }
    std::vector<Json>& elements() { type = JsonType::Array; return arr_; } // mutable, for filtering/removal
    void erase(size_t index);

    // ---- serialization ----
    // pretty = true -> 2-space indented, human-readable (matches the style
    // the original Python app used when writing its JSON files).
    std::string dump(bool pretty = false) const;

    // ---- parsing ----
    // Throws std::runtime_error with a description on malformed input.
    static Json parse(const std::string& text);

private:
    void dumpTo(std::string& out, bool pretty, int indent) const;

    bool b_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::string, Json>> obj_;

    static const Json& nullRef();
};

} // namespace op
