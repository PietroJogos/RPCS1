// Endereçamento do R3000A no PlayStation: segmentos virtuais e endereços físicos.
//
//   KUSEG  00000000h–7FFFFFFFh  usuário/kernel, com cache; só os primeiros 512 MiB existem
//   KSEG0  80000000h–9FFFFFFFh  kernel, com cache   → físico = endereço − 80000000h
//   KSEG1  A0000000h–BFFFFFFFh  kernel, sem cache   → físico = endereço − A0000000h
//   KSEG2  C0000000h–FFFFFFFFh  kernel; só FFFE0000h–FFFE01FFh (controle de cache)
//
// O PS1 não tem TLB: a tradução é fixa. Restrições de privilégio (modo usuário
// acessando KSEG0/1/2) são verificadas pela CPU (Fase 4), não pelo barramento.
#pragma once

#include <string_view>

#include "core/types.hpp"

namespace rpcs1::bus {

enum class Segment : u8 { Kuseg, Kseg0, Kseg1, Kseg2 };

[[nodiscard]] constexpr Segment segment_of(u32 vaddr) noexcept {
    if (vaddr < 0x80000000u) return Segment::Kuseg;
    if (vaddr < 0xA0000000u) return Segment::Kseg0;
    if (vaddr < 0xC0000000u) return Segment::Kseg1;
    return Segment::Kseg2;
}

// Endereço físico de 29 bits para KUSEG (primeiros 512 MiB), KSEG0 e KSEG1.
// Para KSEG2 e para KUSEG acima de 512 MiB não há tradução: retorna o próprio endereço.
[[nodiscard]] constexpr u32 to_physical(u32 vaddr) noexcept {
    switch (segment_of(vaddr)) {
    case Segment::Kuseg: return vaddr;
    case Segment::Kseg0: return vaddr - 0x80000000u;
    case Segment::Kseg1: return vaddr - 0xA0000000u;
    case Segment::Kseg2: return vaddr;
    }
    return vaddr;
}

// A I-cache só atua em KUSEG e KSEG0.
[[nodiscard]] constexpr bool is_cached(u32 vaddr) noexcept {
    const Segment s = segment_of(vaddr);
    return s == Segment::Kuseg || s == Segment::Kseg0;
}

enum class AccessWidth : u8 { Byte = 1, Half = 2, Word = 4 };

[[nodiscard]] constexpr u32 width_bytes(AccessWidth width) noexcept {
    return static_cast<u32>(width);
}

[[nodiscard]] constexpr u32 width_mask(AccessWidth width) noexcept {
    switch (width) {
    case AccessWidth::Byte: return 0xFFu;
    case AccessWidth::Half: return 0xFFFFu;
    case AccessWidth::Word: return 0xFFFFFFFFu;
    }
    return 0xFFFFFFFFu;
}

// LH/LHU/SH exigem alinhamento de 2 bytes; LW/SW de 4 bytes.
[[nodiscard]] constexpr bool is_aligned(u32 address, AccessWidth width) noexcept {
    return (address & (width_bytes(width) - 1u)) == 0;
}

[[nodiscard]] std::string_view to_string(Segment segment) noexcept;
[[nodiscard]] std::string_view to_string(AccessWidth width) noexcept;

} // namespace rpcs1::bus
