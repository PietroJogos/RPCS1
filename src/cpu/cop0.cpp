#include "cpu/cop0.hpp"

#include "cpu/cpu.hpp"

namespace rpcs1::cpu {

namespace {

// DCIC: bits 0–5 (status), 12–13 (redirecionamento) e 23–31 (habilitações).
// ⚠️ Bits 14–15 "desconhecidos" são lidos como zero.
constexpr u32 dcic_write_mask = 0x3Fu | (3u << 12) | 0xFF800000u;

// CAUSE: só os bits 8–9 (interrupções de software) são graváveis.
constexpr u32 cause_sw_mask = 3u << 8;

} // namespace

void Cop0::reset() noexcept {
    sr_ = sr_reset;
    cause_ = 0;
    epc_ = 0;
    bad_vaddr_ = 0;
    tar_ = 0;
    bpc_ = 0;
    bda_ = 0;
    dcic_ = 0;
    bdam_ = 0;
    bpcm_ = 0;
}

std::optional<u32> Cop0::read(u32 reg, bool irq_line) const noexcept {
    switch (reg & 31) {
    case cop0reg::bpc: return bpc_;
    case cop0reg::bda: return bda_;
    case cop0reg::tar: return tar_;
    case cop0reg::dcic: return dcic_;
    case cop0reg::bad_vaddr: return bad_vaddr_;
    case cop0reg::bdam: return bdam_;
    case cop0reg::bpcm: return bpcm_;
    case cop0reg::sr: return sr_;
    case cop0reg::cause: return cause(irq_line);
    case cop0reg::epc: return epc_;
    case cop0reg::prid: return prid_value;
    case 0:
    case 1:
    case 2:
    case 4:
    case 10: return std::nullopt; // RI
    default:
        // r16–r31: valor imprevisível no hardware (⚠️ o RPCS1 lê zero).
        return 0u;
    }
}

void Cop0::write(u32 reg, u32 value) noexcept {
    switch (reg & 31) {
    case cop0reg::bpc: bpc_ = value; break;
    case cop0reg::bda: bda_ = value; break;
    case cop0reg::dcic: dcic_ = value & dcic_write_mask; break;
    case cop0reg::bdam: bdam_ = value; break;
    case cop0reg::bpcm: bpcm_ = value; break;
    case cop0reg::sr: sr_ = value & sr::write_mask; break;
    case cop0reg::cause: cause_ = (cause_ & ~cause_sw_mask) | (value & cause_sw_mask); break;
    default:
        // TAR, BadVaddr, EPC, PRID (somente leitura) e registradores inexistentes.
        break;
    }
}

bool Cop0::interrupt_pending(bool irq_line) const noexcept {
    if ((sr_ & sr::iec) == 0) {
        return false;
    }
    const u32 ip = (cause(irq_line) >> 8) & 0xFFu;
    const u32 im = (sr_ >> 8) & 0xFFu;
    return (ip & im) != 0;
}

u32 Cop0::enter_exception(const ExceptionRecord& record) noexcept {
    epc_ = record.epc;
    if (record.bad_vaddr) {
        bad_vaddr_ = *record.bad_vaddr;
    }
    // TAR só é atualizado em delay slot de desvio tomado/incondicional.
    if (record.branch_delay && record.branch_taken) {
        tar_ = record.branch_target;
    }

    u32 new_cause = cause_ & cause_sw_mask;
    new_cause |= (static_cast<u32>(record.code) & 0x1Fu) << 2;
    // ⚠️ CE só é documentado para CpU; nas demais exceções fica zero.
    if (record.code == ExceptionCode::CoprocessorUnusable) {
        new_cause |= (static_cast<u32>(record.coprocessor) & 3u) << 28;
    }
    if (record.branch_delay) {
        new_cause |= 1u << 31;
        if (record.branch_taken) {
            new_cause |= 1u << 30;
        }
    }
    cause_ = new_cause;

    // Empilha (KUc, IEc) → (KUp, IEp) → (KUo, IEo); entra em modo kernel sem interrupções.
    // ⚠️ O psx-spx descreve só o RFE; isto é o inverso dele, como na arquitetura R3000.
    sr_ = (sr_ & ~0x3Fu) | ((sr_ << 2) & 0x3Fu);

    return general_vector((sr_ & sr::bev) != 0);
}

void Cop0::rfe() noexcept {
    sr_ = (sr_ & ~0x0Fu) | ((sr_ >> 2) & 0x0Fu);
}

} // namespace rpcs1::cpu
