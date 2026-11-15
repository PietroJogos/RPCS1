// Leitura/escrita little-endian em buffers de bytes (o PS1 é little-endian).
// Implementado com deslocamentos para ser portátil; o compilador gera um único acesso.
#pragma once

#include "core/types.hpp"

namespace rpcs1::memory {

[[nodiscard]] inline u8 load_le8(const u8* p) noexcept {
    return p[0];
}

[[nodiscard]] inline u16 load_le16(const u8* p) noexcept {
    return static_cast<u16>(p[0] | (p[1] << 8));
}

[[nodiscard]] inline u32 load_le32(const u8* p) noexcept {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
           (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
}

inline void store_le8(u8* p, u8 value) noexcept {
    p[0] = value;
}

inline void store_le16(u8* p, u16 value) noexcept {
    p[0] = static_cast<u8>(value);
    p[1] = static_cast<u8>(value >> 8);
}

inline void store_le32(u8* p, u32 value) noexcept {
    p[0] = static_cast<u8>(value);
    p[1] = static_cast<u8>(value >> 8);
    p[2] = static_cast<u8>(value >> 16);
    p[3] = static_cast<u8>(value >> 24);
}

} // namespace rpcs1::memory
