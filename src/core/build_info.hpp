// Informações sobre o build atual (compilador, configuração, plataforma).
#pragma once

#include <string>

namespace rpcs1::build_info {

[[nodiscard]] std::string compiler();
[[nodiscard]] const char* configuration() noexcept; // "Debug" ou "Release"
[[nodiscard]] const char* platform() noexcept;
[[nodiscard]] std::string full_version_string();    // "RPCS1 0.0.1 (...)"

} // namespace rpcs1::build_info
