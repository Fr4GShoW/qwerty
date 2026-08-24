// csv.cpp
#include "csv.h"

namespace op {

std::vector<std::vector<std::string>> parseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;
    size_t n = text.size();

    auto endField = [&]() {
        row.push_back(field);
        field.clear();
    };
    auto endRow = [&]() {
        endField();
        rows.push_back(row);
        row.clear();
    };

    for (size_t i = 0; i < n; ++i) {
        char c = text[i];

        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') {
                    field += '"';
                    ++i; // consume the escaped quote pair
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
            continue;
        }

        if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            endField();
        } else if (c == '\r') {
            // Look ahead for \r\n; either way this ends the row.
            if (i + 1 < n && text[i + 1] == '\n') ++i;
            endRow();
        } else if (c == '\n') {
            endRow();
        } else {
            field += c;
        }
    }

    // Trailing field/row without a final newline.
    if (!field.empty() || !row.empty()) {
        endRow();
    }

    return rows;
}

} // namespace op
