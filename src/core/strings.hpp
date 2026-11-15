// Utilitarios de texto (ASCII) usados por configuracao e linha de comando.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/types.hpp"

namespace rpcs1::strings {

[[nodiscard]] std::string_view trim(std::string_view text) noexcept;
[[nodiscard]] std::string to_lower_ascii(std::string_view text);
[[nodiscard]] bool iequals(std::string_view a, std::string_view b) noexcept;
[[nodiscard]] bool starts_with(std::string_view text, std::string_view prefix) noexcept;

// Converte inteiro decimal (ou hexadecimal com prefixo 0x) sem sinal.
// Rejeita espacos, sinais, sufixos e valores fora de [min, max].
[[nodiscard]] std::optional<u64> parse_unsigned(std::string_view text, u64 min = 0,
                                                u64 max = UINT64_MAX) noexcept;

// Aceita: true/false, 1/0, yes/no, on/off, sim/nao/não (ASCII sem diferenciar maiúsculas).
[[nodiscard]] std::optional<bool> parse_bool(std::string_view text) noexcept;

} // namespace rpcs1::strings
