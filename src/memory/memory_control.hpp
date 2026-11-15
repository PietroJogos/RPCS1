// Memory Control: registradores 1F801000h–1F801023h (Memory Control 1),
// 1F801060h (DRAM_CTRL) e FFFE0130h (controle de cache, BCC).
//
// Fonte: psx-spx, https://psx-spx.consoledev.net/memorycontrol/
//
// Nesta fase o RPCS1 usa estes registradores para decidir QUAIS endereços existem:
//   - o campo de tamanho (bits 16–20) de cada Delay/Size define a janela 1 << N bytes
//     de cada região de expansão e da BIOS; acessos além dela geram erro de barramento;
//   - DRAM_CTRL define o tamanho dos bancos de RAM (e, portanto, o espelhamento).
// Os campos de atraso (temporização) são armazenados, mas só terão efeito na Fase 5.
// Os bits do BCC (habilitar I-cache, scratchpad, modo de teste de tags) terão efeito na Fase 4.
#pragma once

#include <array>

#include "bus/address.hpp"
#include "core/types.hpp"

namespace rpcs1::memory {

// Dispositivos com registrador Delay/Size.
enum class DelayDevice : u8 {
    Expansion1 = 0, // DEV0, 1F801008h
    Expansion3 = 1, // DEV1, 1F80100Ch
    Bios = 2,       // DEV2, 1F801010h
    Spu = 3,        // DEV4, 1F801014h
    Cdrom = 4,      // DEV5, 1F801018h
    Expansion2 = 5, // DEV8, 1F80101Ch
};

struct RamLayout {
    u32 bank0_size = 0; // /RAS0
    u32 bank1_size = 0; // /RAS1 (0 se desabilitado)
    [[nodiscard]] u32 mapped_size() const noexcept { return bank0_size + bank1_size; }
};

class MemoryControl {
public:
    // Endereços físicos.
    static constexpr u32 mc1_begin = 0x1F801000;
    static constexpr u32 mc1_end = 0x1F801024; // exclusivo
    static constexpr u32 dram_ctrl_address = 0x1F801060;
    static constexpr u32 bcc_address = 0xFFFE0130; // KSEG2 (endereço virtual)

    // Valores usuais gravados pela BIOS (psx-spx). ⚠️ Os valores reais de power-on não
    // são documentados; o RPCS1 adota estes como estado de reset (ver DECISIONS.md, D-012).
    static constexpr u32 default_expansion1_base = 0x1F000000;
    static constexpr u32 default_expansion2_base = 0x1F802000;
    static constexpr u32 default_dram_ctrl = 0x00000B88;

    MemoryControl() { reset(); }

    void reset() noexcept;

    // Acesso a registradores pelo endereço físico (Memory Control 1 e DRAM_CTRL).
    // Pré-condição: `contains(phys)`.
    [[nodiscard]] static bool contains(u32 phys) noexcept;
    [[nodiscard]] u32 read(u32 phys, bus::AccessWidth width) const noexcept;
    void write(u32 phys, u32 value, bus::AccessWidth width) noexcept;

    // Registrador de controle de cache (KSEG2 FFFE0130h).
    [[nodiscard]] u32 cache_control() const noexcept { return bcc_; }
    void set_cache_control(u32 value) noexcept { bcc_ = value; }

    // Valores brutos e decodificados.
    [[nodiscard]] u32 expansion1_base() const noexcept { return regs_[0]; }
    [[nodiscard]] u32 expansion2_base() const noexcept { return regs_[1]; }
    [[nodiscard]] u32 delay_size(DelayDevice device) const noexcept;
    [[nodiscard]] u32 com_delay() const noexcept { return regs_[8]; }
    [[nodiscard]] u32 dram_ctrl() const noexcept { return dram_ctrl_; }

    // Janela endereçável do dispositivo: 1 << N bytes, N = bits 16–20.
    [[nodiscard]] u64 window_size(DelayDevice device) const noexcept;
    [[nodiscard]] RamLayout ram_layout() const noexcept;

    [[nodiscard]] static RamLayout decode_dram_ctrl(u32 value) noexcept;

private:
    // regs_[0..8] = 1F801000h, 1004h, 1008h, 100Ch, 1010h, 1014h, 1018h, 101Ch, 1020h.
    std::array<u32, 9> regs_{};
    u32 dram_ctrl_ = default_dram_ctrl;
    u32 bcc_ = 0;
};

} // namespace rpcs1::memory
