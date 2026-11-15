// Cache de instruções (I-cache) do R3000A no PlayStation (Fase 4).
//
// Fonte: psx-spx ("Memory Map", "Memory Control", "CPU Specifications").
//   - 4 KiB, mapeamento direto, 256 linhas de 4 palavras; índice = bits 11–4 do endereço.
//   - Tag = endereço físico bits 31–12 + 4 bits de validade (um por palavra).
//   - Ativa só em KUSEG e KSEG0 e só com BCC.IS1 (bit 11); KSEG1 passa direto à memória.
//   - Falha: preenche da palavra acessada até o fim da linha, sem dar a volta. Com
//     IBLKSZ = 0 (BCC bits 8–9), uma falha na palavra 0 preenche só as palavras 0–1.
//   - Escritas na memória NÃO atualizam a cache (código automodificado fica obsoleto).
//   - Com SR.IsC (cache isolada), loads/stores de dados vão para a cache:
//       BCC.TAG + IS1 → store grava a tag (endereço) e validade (bits 3–0 do dado);
//                       load devolve validade (3–0) e bit 4 = tag coincide;
//       só IS1        → store grava a palavra de código; load a devolve.
// Temporização (custos de acerto/falha) fica para a Fase 5.
#pragma once

#include <array>

#include "bus/bus.hpp"
#include "core/types.hpp"

namespace rpcs1::cpu {

namespace bcc {
inline constexpr u32 tag_test = 1u << 2;
inline constexpr u32 iblksz_shift = 8;
inline constexpr u32 is1 = 1u << 11;
} // namespace bcc

class InstructionCache {
public:
    static constexpr u32 line_count = 256;

    struct Line {
        u32 tag = 0;   // endereço físico & FFFFF000h
        u8 valid = 0;  // bit n = palavra n válida
        std::array<u32, 4> words{};
    };

    void reset() noexcept;

    // Busca de instrução pela cache. Pré-condição: endereço alinhado em KUSEG/KSEG0.
    [[nodiscard]] bus::ReadResult fetch(bus::Bus& bus, u32 vaddr, u32 bcc_value);

    // Acessos de dados com SR.IsC = 1.
    void isolated_store(u32 vaddr, u32 value, u32 bcc_value) noexcept;
    [[nodiscard]] u32 isolated_load(u32 vaddr, u32 bcc_value) const noexcept;

    [[nodiscard]] const Line& line(u32 index) const noexcept { return lines_[index & 0xFF]; }
    [[nodiscard]] u64 hits() const noexcept { return hits_; }
    [[nodiscard]] u64 misses() const noexcept { return misses_; }

    [[nodiscard]] static constexpr u32 index_of(u32 address) noexcept { return (address >> 4) & 0xFF; }
    [[nodiscard]] static constexpr u32 word_of(u32 address) noexcept { return (address >> 2) & 3; }
    [[nodiscard]] static constexpr u32 tag_of(u32 vaddr) noexcept {
        return bus::to_physical(vaddr) & 0xFFFFF000u;
    }

private:
    std::array<Line, line_count> lines_{};
    u64 hits_ = 0;
    u64 misses_ = 0;
};

} // namespace rpcs1::cpu
