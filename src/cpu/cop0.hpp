// COP0 — coprocessador de controle do sistema do R3000A no PlayStation (Fase 4).
//
// Fonte: psx-spx, "CPU Specifications" (COP0) e "Interrupts".
//
//   r3 BPC, r5 BDA, r6 TAR, r7 DCIC, r8 BadVaddr, r9 BDAM, r11 BPCM,
//   r12 SR, r13 CAUSE, r14 EPC, r15 PRID (= 00000002h)
//   r0–r2, r4, r10: leitura gera RI.  r16–r31: leitura sem exceção, valor imprevisível.
//
// CAUSE bit 10 não é armazenado: reflete ao vivo a linha de interrupção
// ((I_STAT AND I_MASK) != 0). Bits 8–9 são interrupções de software (L/E).
#pragma once

#include <optional>

#include "core/types.hpp"

namespace rpcs1::cpu {

struct ExceptionRecord;

namespace sr {
inline constexpr u32 iec = 1u << 0;   // interrupções habilitadas (atual)
inline constexpr u32 kuc = 1u << 1;   // modo usuário (atual)
inline constexpr u32 isc = 1u << 16;  // isolar cache
inline constexpr u32 bev = 1u << 22;  // vetores de exceção na ROM
inline constexpr u32 cu0 = 1u << 28;
inline constexpr u32 cu1 = 1u << 29;
inline constexpr u32 cu2 = 1u << 30;
inline constexpr u32 cu3 = 1u << 31;
// Bits sempre zero (psx-spx): 6–7, 23–24, 26–27.
inline constexpr u32 write_mask = ~((3u << 6) | (3u << 23) | (3u << 26));
} // namespace sr

namespace cop0reg {
inline constexpr u32 bpc = 3, bda = 5, tar = 6, dcic = 7, bad_vaddr = 8, bdam = 9, bpcm = 11,
                     sr = 12, cause = 13, epc = 14, prid = 15;
} // namespace cop0reg

class Cop0 {
public:
    static constexpr u32 prid_value = 0x00000002; // CXD8606CQ
    // ⚠️ Valor de SR após reset não documentado no psx-spx; o RPCS1 segue a arquitetura
    // R3000 (BEV = 1, kernel, interrupções desligadas). Ver DECISIONS.md, D-025.
    static constexpr u32 sr_reset = sr::bev;

    Cop0() { reset(); }
    void reset() noexcept;

    // MFC0: std::nullopt significa que o acesso gera RI (registradores inexistentes).
    [[nodiscard]] std::optional<u32> read(u32 reg, bool irq_line) const noexcept;
    // MTC0: escritas em registradores somente leitura ou inexistentes são ignoradas (⚠️).
    void write(u32 reg, u32 value) noexcept;

    [[nodiscard]] u32 status() const noexcept { return sr_; }
    [[nodiscard]] u32 cause(bool irq_line) const noexcept {
        return cause_ | (irq_line ? (1u << 10) : 0u);
    }
    [[nodiscard]] u32 epc() const noexcept { return epc_; }
    [[nodiscard]] u32 bad_vaddr() const noexcept { return bad_vaddr_; }
    [[nodiscard]] u32 tar() const noexcept { return tar_; }

    [[nodiscard]] bool user_mode() const noexcept { return (sr_ & sr::kuc) != 0; }
    [[nodiscard]] bool cache_isolated() const noexcept { return (sr_ & sr::isc) != 0; }
    [[nodiscard]] bool coprocessor_usable(u32 n) const noexcept {
        return (sr_ & (sr::cu0 << (n & 3))) != 0;
    }

    // Interrupção pendente: IEc = 1 e algum bit de CAUSE.IP (8–15) habilitado em SR.Im.
    [[nodiscard]] bool interrupt_pending(bool irq_line) const noexcept;

    // Entrada em exceção: EPC, CAUSE (ExcCode, BD, BT, CE), BadVaddr (só AdEL/AdES),
    // TAR (só em delay slot com desvio tomado) e empilhamento de modo/interrupção em SR.
    // Retorna o endereço do vetor.
    [[nodiscard]] u32 enter_exception(const ExceptionRecord& record) noexcept;

    // RFE: SR bits 2–3 → 0–1 e 4–5 → 2–3; bits 4–5 inalterados.
    void rfe() noexcept;

    [[nodiscard]] static u32 general_vector(bool bev) noexcept {
        return bev ? 0xBFC00180u : 0x80000080u;
    }

private:
    u32 sr_ = sr_reset;
    u32 cause_ = 0; // sem o bit 10 (ao vivo)
    u32 epc_ = 0;
    u32 bad_vaddr_ = 0;
    u32 tar_ = 0;
    u32 bpc_ = 0;
    u32 bda_ = 0;
    u32 dcic_ = 0;
    u32 bdam_ = 0;
    u32 bpcm_ = 0;
};

} // namespace rpcs1::cpu
