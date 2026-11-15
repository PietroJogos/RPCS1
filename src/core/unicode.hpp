// Conversões de texto e caminhos. Internamente o RPCS1 usa UTF-8 em std::string;
// as conversões para UTF-16 ocorrem somente na fronteira com a API do Windows.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rpcs1::unicode {

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view utf8);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);

#ifdef _WIN32
// Sequências inválidas são substituídas por U+FFFD.
[[nodiscard]] std::string wide_to_utf8(std::wstring_view wide);
[[nodiscard]] std::wstring utf8_to_wide(std::string_view utf8);
#endif

} // namespace rpcs1::unicode
