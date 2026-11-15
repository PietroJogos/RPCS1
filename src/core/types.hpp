// Tipos inteiros de largura fixa usados em todo o RPCS1.
#pragma once

#include <cstddef>
#include <cstdint>

namespace rpcs1 {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using usize = std::size_t;

inline constexpr usize KiB = 1024;
inline constexpr usize MiB = 1024 * KiB;

} // namespace rpcs1
