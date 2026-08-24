#pragma once
#include <string>

namespace op {
bool curlFetch(const std::string& url, std::string& outBody, std::string& outError);
}
