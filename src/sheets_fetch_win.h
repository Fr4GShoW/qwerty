// sheets_fetch_win.h
// Windows-only declaration for the WinHTTP-based fetch implemented in
// sheets_fetch_win.cpp. Matches routes.h's FetchFn signature.
#pragma once

#ifdef _WIN32
#include <string>

namespace op {

bool winHttpFetch(const std::string& url, std::string& outBody, std::string& outError);

} // namespace op

#endif // _WIN32
