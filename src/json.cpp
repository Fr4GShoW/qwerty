// json.cpp
#include "json.h"
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace op {

const Json& Json::nullRef() {
    static const Json n;
    return n;
}

bool Json::has(const std::string& key) const {
    for (auto& kv : obj_) if (kv.first == key) return true;
    return false;
}

const Json& Json::get(const std::string& key) const {
    for (auto& kv : obj_) if (kv.first == key) return kv.second;
    return nullRef();
}

Json& Json::operator[](const std::string& key) {
    type = JsonType::Object;
    for (auto& kv : obj_) if (kv.first == key) return kv.second;
    obj_.emplace_back(key, Json());
    return obj_.back().second;
}

void Json::set(const std::string& key, Json value) {
    type = JsonType::Object;
    for (auto& kv : obj_) {
        if (kv.first == key) { kv.second = std::move(value); return; }
    }
    obj_.emplace_back(key, std::move(value));
}

void Json::push_back(Json value) {
    type = JsonType::Array;
    arr_.push_back(std::move(value));
}

size_t Json::size() const {
    if (type == JsonType::Array) return arr_.size();
    if (type == JsonType::Object) return obj_.size();
    return 0;
}

const Json& Json::at(size_t index) const {
    if (index >= arr_.size()) return nullRef();
    return arr_[index];
}

Json& Json::at(size_t index) {
    if (index >= arr_.size()) throw std::out_of_range("Json::at(size_t): index out of range");
    return arr_[index];
}

void Json::erase(size_t index) {
    if (index < arr_.size()) arr_.erase(arr_.begin() + static_cast<long>(index));
}

std::string Json::asString(const std::string& def) const {
    if (type == JsonType::String) return str_;
    if (type == JsonType::Null) return def;
    // Be forgiving: numbers/bools coerce to a printable string too.
    if (type == JsonType::Number) {
        std::ostringstream oss;
        if (num_ == static_cast<long long>(num_)) oss << static_cast<long long>(num_);
        else oss << num_;
        return oss.str();
    }
    if (type == JsonType::Bool) return b_ ? "true" : "false";
    return def;
}

double Json::asDouble(double def) const {
    if (type == JsonType::Number) return num_;
    if (type == JsonType::String) {
        try { return std::stod(str_); } catch (...) { return def; }
    }
    return def;
}

long long Json::asInt(long long def) const {
    if (type == JsonType::Number) return static_cast<long long>(num_);
    if (type == JsonType::String) {
        try { return std::stoll(str_); } catch (...) { return def; }
    }
    return def;
}

bool Json::asBool(bool def) const {
    if (type == JsonType::Bool) return b_;
    if (type == JsonType::Number) return num_ != 0.0;
    return def;
}

// ---------------- serialization ----------------

static void appendEscaped(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

static void appendNumber(std::string& out, double v) {
    if (std::isfinite(v) && v == static_cast<long long>(v) &&
        std::fabs(v) < 1e15) {
        out += std::to_string(static_cast<long long>(v));
        return;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

void Json::dumpTo(std::string& out, bool pretty, int indent) const {
    auto pad = [&](int n) { if (pretty) out.append(static_cast<size_t>(n) * 2, ' '); };

    switch (type) {
        case JsonType::Null: out += "null"; break;
        case JsonType::Bool: out += (b_ ? "true" : "false"); break;
        case JsonType::Number: appendNumber(out, num_); break;
        case JsonType::String: appendEscaped(out, str_); break;
        case JsonType::Array: {
            if (arr_.empty()) { out += "[]"; break; }
            out += '[';
            if (pretty) out += '\n';
            for (size_t i = 0; i < arr_.size(); ++i) {
                pad(indent + 1);
                arr_[i].dumpTo(out, pretty, indent + 1);
                if (i + 1 < arr_.size()) out += ',';
                if (pretty) out += '\n';
            }
            pad(indent);
            out += ']';
            break;
        }
        case JsonType::Object: {
            if (obj_.empty()) { out += "{}"; break; }
            out += '{';
            if (pretty) out += '\n';
            for (size_t i = 0; i < obj_.size(); ++i) {
                pad(indent + 1);
                appendEscaped(out, obj_[i].first);
                out += pretty ? ": " : ":";
                obj_[i].second.dumpTo(out, pretty, indent + 1);
                if (i + 1 < obj_.size()) out += ',';
                if (pretty) out += '\n';
            }
            pad(indent);
            out += '}';
            break;
        }
    }
}

std::string Json::dump(bool pretty) const {
    std::string out;
    dumpTo(out, pretty, 0);
    return out;
}

// ---------------- parsing ----------------

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text), i_(0), n_(text.size()) {}

    Json parseValue() {
        skipWs();
        if (i_ >= n_) throw std::runtime_error("Unexpected end of JSON input");
        char c = s_[i_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Json::string(parseString());
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error(std::string("Unexpected character '") + c + "' at offset " + std::to_string(i_));
    }

private:
    const std::string& s_;
    size_t i_;
    size_t n_;

    void skipWs() {
        while (i_ < n_ && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    char peek() { skipWs(); return i_ < n_ ? s_[i_] : '\0'; }

    void expect(char c) {
        skipWs();
        if (i_ >= n_ || s_[i_] != c) {
            throw std::runtime_error(std::string("Expected '") + c + "' at offset " + std::to_string(i_));
        }
        ++i_;
    }

    Json parseObject() {
        Json obj = Json::object();
        expect('{');
        skipWs();
        if (peek() == '}') { ++i_; return obj; }
        while (true) {
            skipWs();
            std::string key = parseString();
            expect(':');
            Json val = parseValue();
            obj.set(key, std::move(val));
            skipWs();
            if (peek() == ',') { ++i_; continue; }
            expect('}');
            break;
        }
        return obj;
    }

    Json parseArray() {
        Json arr = Json::array();
        expect('[');
        skipWs();
        if (peek() == ']') { ++i_; return arr; }
        while (true) {
            arr.push_back(parseValue());
            skipWs();
            if (peek() == ',') { ++i_; continue; }
            expect(']');
            break;
        }
        return arr;
    }

    std::string parseString() {
        skipWs();
        if (i_ >= n_ || s_[i_] != '"') throw std::runtime_error("Expected string at offset " + std::to_string(i_));
        ++i_;
        std::string out;
        while (i_ < n_ && s_[i_] != '"') {
            char c = s_[i_++];
            if (c == '\\') {
                if (i_ >= n_) throw std::runtime_error("Unterminated escape sequence");
                char e = s_[i_++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (i_ + 4 > n_) throw std::runtime_error("Invalid \\u escape");
                        unsigned int cp = static_cast<unsigned int>(std::stoul(s_.substr(i_, 4), nullptr, 16));
                        i_ += 4;
                        // Minimal UTF-8 encode (handles BMP; good enough for this app's data)
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("Invalid escape character");
                }
            } else {
                out += c;
            }
        }
        if (i_ >= n_) throw std::runtime_error("Unterminated string");
        ++i_; // closing quote
        return out;
    }

    Json parseNumber() {
        size_t start = i_;
        if (i_ < n_ && s_[i_] == '-') ++i_;
        while (i_ < n_ && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        if (i_ < n_ && s_[i_] == '.') {
            ++i_;
            while (i_ < n_ && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        }
        if (i_ < n_ && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < n_ && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            while (i_ < n_ && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        }
        std::string numStr = s_.substr(start, i_ - start);
        return Json::number(std::stod(numStr));
    }

    Json parseBool() {
        if (s_.compare(i_, 4, "true") == 0) { i_ += 4; return Json::boolean(true); }
        if (s_.compare(i_, 5, "false") == 0) { i_ += 5; return Json::boolean(false); }
        throw std::runtime_error("Invalid literal at offset " + std::to_string(i_));
    }

    Json parseNull() {
        if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return Json::null(); }
        throw std::runtime_error("Invalid literal at offset " + std::to_string(i_));
    }
};

} // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    return p.parseValue();
}

} // namespace op
