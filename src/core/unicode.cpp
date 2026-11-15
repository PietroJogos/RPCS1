#include "core/unicode.hpp"

#include <climits>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rpcs1::unicode {

std::filesystem::path path_from_utf8(std::string_view utf8) {
    std::u8string text(utf8.size(), u8'\0');
    for (std::size_t i = 0; i < utf8.size(); ++i) {
        text[i] = static_cast<char8_t>(utf8[i]);
    }
    return std::filesystem::path(text);
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    std::string out(text.size(), '\0');
    for (std::size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<char>(text[i]);
    }
    return out;
}

#ifdef _WIN32

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty() || wide.size() > static_cast<std::size_t>(INT_MAX)) {
        return {};
    }
    const int length = static_cast<int>(wide.size());
    const int needed =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty() || utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return {};
    }
    const int length = static_cast<int>(utf8.size());
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, out.data(), needed);
    return out;
}

#endif

} // namespace rpcs1::unicode
