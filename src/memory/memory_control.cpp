#include "memory/memory_control.hpp"

#include "core/log.hpp"

namespace rpcs1::memory {

namespace {

constexpr usize register_count = 9;

// Valores usuais (psx-spx, Memory Control). Índices = (endereço − 1F801000h) / 4.
constexpr std::array<u32, register_count> reset_values = {
    MemoryControl::default_expansion1_base, // 1F801000h base DEV0
    MemoryControl::default_expansion2_base, // 1F801004h base DEV8
    0x0013243F,                             // 1F801008h DEV0 (Expansão 1)
    0x00003022,                             // 1F80100Ch DEV1 (Expansão 3)
    0x0013243F,                             // 1F801010h DEV2 (BIOS ROM)
    0x200931E1,                             // 1F801014h DEV4 (SPU)
    0x00020843,                             // 1F801018h DEV5 (CD-ROM)
    0x00070777,                             // 1F80101Ch DEV8 (Expansão 2)
    0x00031125,                             // 1F801020h COM_DELAY
};

constexpr usize delay_index(DelayDevice device) noexcept {
    return 2 + static_cast<usize>(device);
}

// Bits 21–23 dos registradores Delay/Size são sempre zero; o bit 28 é um flag de erro
// de endereço que se limpa escrevendo 1 (o RPCS1 ainda não o liga em nenhuma situação).
constexpr u32 delay_size_write_mask = ~((0x7u << 21) | (1u << 28));

} // namespace

void MemoryControl::reset() noexcept {
    regs_ = reset_values;
    dram_ctrl_ = default_dram_ctrl;
    // ⚠️ Valor de reset do BCC não documentado; a BIOS grava 0001E988h logo no início.
    bcc_ = 0;
}

bool MemoryControl::contains(u32 phys) noexcept {
    return (phys >= mc1_begin && phys < mc1_end) ||
           (phys >= dram_ctrl_address && phys < dram_ctrl_address + 4);
}

u32 MemoryControl::read(u32 phys, bus::AccessWidth width) const noexcept {
    const u32 aligned = phys & ~3u;
    const u32 value = aligned == dram_ctrl_address ? dram_ctrl_
                                                   : regs_[(aligned - mc1_begin) / 4];
    // Leituras estreitas devolvem a faixa de bytes correspondente do registrador de 32 bits.
    // ⚠️ Regra geral para leituras de 8/16 bits em registradores de E/S não documentada.
    const u32 shift = (phys & 3u) * 8u;
    return (value >> shift) & bus::width_mask(width);
}

void MemoryControl::write(u32 phys, u32 value, bus::AccessWidth width) noexcept {
    // Em SB/SH a CPU coloca os 32 bits do registrador no barramento (psx-spx, "CPU
    // Specifications"). ⚠️ Não está documentado se o Memory Control respeita os sinais
    // de byte; o RPCS1 trata escritas estreitas como escrita do valor completo.
    if (width != bus::AccessWidth::Word) {
        log::debug(log::Category::Memory,
                   "Escrita de {} em registrador de Memory Control {:08X}h tratada como 32 bits",
                   bus::to_string(width), phys);
    }
    const u32 aligned = phys & ~3u;
    if (aligned == dram_ctrl_address) {
        // Bits 16–31 não usados (lixo na leitura); o RPCS1 os lê como zero.
        dram_ctrl_ = value & 0x0000FFFFu;
        const RamLayout layout = decode_dram_ctrl(dram_ctrl_);
        log::debug(log::Category::Memory, "DRAM_CTRL = {:04X}h (banco 0: {} KiB, banco 1: {} KiB)",
                   dram_ctrl_, layout.bank0_size / 1024, layout.bank1_size / 1024);
        return;
    }
    const usize index = (aligned - mc1_begin) / 4;
    switch (index) {
    case 0: // base DEV0
        if (value != default_expansion1_base) {
            log::warn(log::Category::Memory,
                      "Base da Expansão 1 alterada para {:08X}h; realocação não emulada", value);
        }
        regs_[index] = value;
        break;
    case 1: // base DEV8
        regs_[index] = value;
        break;
    case 8: // COM_DELAY: bits 16–31 sempre lidos como zero
        regs_[index] = value & 0x0000FFFFu;
        break;
    default: // Delay/Size
        regs_[index] = value & delay_size_write_mask;
        break;
    }
}

u32 MemoryControl::delay_size(DelayDevice device) const noexcept {
    return regs_[delay_index(device)];
}

u64 MemoryControl::window_size(DelayDevice device) const noexcept {
    const u32 bits = (delay_size(device) >> 16) & 0x1Fu;
    return u64{1} << bits;
}

RamLayout MemoryControl::ram_layout() const noexcept {
    return decode_dram_ctrl(dram_ctrl_);
}

RamLayout MemoryControl::decode_dram_ctrl(u32 value) noexcept {
    // psx-spx: RAS0 = 1MB << ((bit9 << 1) | bit11); RAS1 = 1MB << ((bit8 << 1) | bit11),
    // habilitado pelo bit 10. RAS1 vem logo depois de RAS0 no espaço de endereços.
    const u32 bit8 = (value >> 8) & 1u;
    const u32 bit9 = (value >> 9) & 1u;
    const u32 bit10 = (value >> 10) & 1u;
    const u32 bit11 = (value >> 11) & 1u;
    RamLayout layout;
    layout.bank0_size = static_cast<u32>(MiB) << ((bit9 << 1) | bit11);
    layout.bank1_size = bit10 ? static_cast<u32>(MiB) << ((bit8 << 1) | bit11) : 0u;
    return layout;
}

} // namespace rpcs1::memory
