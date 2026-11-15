// Desmontador MIPS I (texto legível de uma instrução), para logs e depuração.
#pragma once

#include <string>
#include <string_view>

#include "core/types.hpp"

namespace rpcs1::cpu {

[[nodiscard]] std::string_view register_name(u32 index) noexcept;

// `address` é o endereço da instrução (usado para calcular alvos de desvio).
[[nodiscard]] std::string disassemble(u32 word, u32 address);

} // namespace rpcs1::cpu
