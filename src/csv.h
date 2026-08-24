// csv.h
// Minimal RFC4180-style CSV parser: handles quoted fields, embedded commas,
// embedded quotes ("" escape), and embedded newlines inside quoted fields --
// exactly the shape Google Sheets produces via its CSV export endpoint.
#pragma once

#include <string>
#include <vector>

namespace op {

// Parses `text` into rows of fields. Line endings \n, \r\n, and \r are all
// accepted as row separators outside of quotes.
std::vector<std::vector<std::string>> parseCsv(const std::string& text);

} // namespace op
