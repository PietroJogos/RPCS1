// Barramento de memória do PlayStation 1.
//
// Recebe endereços VIRTUAIS da CPU, aplica a tradução fixa de segmentos e
// despacha para RAM, scratchpad, BIOS, regiões de expansão, Memory Control e
// dispositivos de E/S. Não gera exceções diretamente: devolve um `BusFault`, que a
// CPU (Fases 3–4) converte em AdEL/AdES (desalinhamento) ou IBE/DBE (erro de barramento).
//
// Mapa físico (psx-spx, "Memory Map"):
//   00000000h  RAM: até 16 MiB decodificados conforme DRAM_CTRL (2 MiB de chips, espelhados)
//   1F000000h  Expansão 1 (DEV0), janela 1 << N (padrão 512 KiB, máx. 8 MiB)
//   1F800000h  Scratchpad 1 KiB (somente KUSEG/KSEG0; não executável)
//   1F801000h  E/S (4 KiB; lacunas entre dispositivos geram erro de barramento)
//   1F802000h  Expansão 2 (DEV8), janela padrão 128 bytes, somente 8 bits
//   1FA00000h  Expansão 3 (DEV1), janela padrão 1 byte
//   1FC00000h  BIOS ROM 512 KiB, janela 1 << N (padrão 512 KiB, máx. 4 MiB)
//   FFFE0130h  Controle de cache (KSEG2)
// Qualquer outro endereço gera erro de barramento.
#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "bus/address.hpp"
#include "bus/io_device.hpp"
#include "core/types.hpp"
#include "memory/bios_rom.hpp"
#include "memory/memory_block.hpp"
#include "memory/memory_control.hpp"

namespace rpcs1::bus {

enum class BusFault : u8 {
    None,
    AddressError, // acesso desalinhado → AdEL/AdES
    BusError,     // endereço inexistente → IBE (busca) / DBE (dados)
};

[[nodiscard]] std::string_view to_string(BusFault fault) noexcept;

struct ReadResult {
    u32 value = 0;
    BusFault fault = BusFault::None;
    [[nodiscard]] bool ok() const noexcept { return fault == BusFault::None; }
};

// Faixas de E/S ocupadas por periféricos (endereços físicos, fim exclusivo).
enum class IoSlot : u8 { Sio0, Sio1, Interrupts, Dma, Timers, Cdrom, Gpu, Mdec, Spu, Count };

struct IoSlotInfo {
    IoSlot slot;
    u32 begin;
    u32 end;
    std::string_view name;
    int phase; // fase do roadmap em que o dispositivo será implementado
};

[[nodiscard]] const IoSlotInfo& io_slot_info(IoSlot slot) noexcept;

class Bus {
public:
    // Regiões físicas.
    static constexpr u32 ram_space_size = 16 * MiB;
    static constexpr u32 expansion1_begin = 0x1F000000;
    static constexpr u32 expansion1_region = 8 * MiB;
    static constexpr u32 scratchpad_begin = 0x1F800000;
    static constexpr u32 io_begin = 0x1F801000;
    static constexpr u32 io_end = 0x1F802000;
    static constexpr u32 expansion2_begin = 0x1F802000;
    static constexpr u32 expansion2_region = 8 * KiB;
    static constexpr u32 post_offset = 0x41; // 1F802041h
    static constexpr u32 expansion3_begin = 0x1FA00000;
    static constexpr u32 expansion3_region = 2 * MiB;
    static constexpr u32 bios_begin = 0x1FC00000;
    static constexpr u32 bios_region = 4 * MiB;
    static constexpr u32 kseg2_control_begin = 0xFFFE0000;
    static constexpr u32 kseg2_control_end = 0xFFFE0200;

    // ⚠️ Valor lido de regiões de expansão sem dispositivo: não documentado no psx-spx.
    // O RPCS1 adota FFh (barramento sem dispositivo); ver DECISIONS.md, D-013.
    static constexpr u8 open_bus_byte = 0xFF;

    Bus();

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    // Ligar: zera RAM e scratchpad e restaura registradores. A BIOS carregada é mantida.
    void power_on();
    // Reset: restaura registradores; RAM e scratchpad preservam o conteúdo.
    void reset();

    [[nodiscard]] ReadResult read(AccessWidth width, u32 vaddr);
    [[nodiscard]] BusFault write(AccessWidth width, u32 vaddr, u32 value);
    // Busca de instrução (32 bits): igual a read, mas o scratchpad e o KSEG2 não são executáveis.
    [[nodiscard]] ReadResult fetch_instruction(u32 vaddr);

    [[nodiscard]] ReadResult read8(u32 vaddr) { return read(AccessWidth::Byte, vaddr); }
    [[nodiscard]] ReadResult read16(u32 vaddr) { return read(AccessWidth::Half, vaddr); }
    [[nodiscard]] ReadResult read32(u32 vaddr) { return read(AccessWidth::Word, vaddr); }
    [[nodiscard]] BusFault write8(u32 vaddr, u32 value) { return write(AccessWidth::Byte, vaddr, value); }
    [[nodiscard]] BusFault write16(u32 vaddr, u32 value) { return write(AccessWidth::Half, vaddr, value); }
    [[nodiscard]] BusFault write32(u32 vaddr, u32 value) { return write(AccessWidth::Word, vaddr, value); }

    // Conecta um dispositivo real no lugar do marcador "não implementado" da faixa.
    // O barramento não assume a posse do dispositivo.
    void attach_device(IoSlot slot, IoDevice& device) noexcept;
    [[nodiscard]] IoDevice& device(IoSlot slot) noexcept;

    [[nodiscard]] memory::MainRam& ram() noexcept { return ram_; }
    [[nodiscard]] memory::Scratchpad& scratchpad() noexcept { return scratchpad_; }
    [[nodiscard]] memory::BiosRom& bios() noexcept { return bios_; }
    [[nodiscard]] memory::MemoryControl& memory_control() noexcept { return memory_control_; }

    // Último código de progresso escrito pela BIOS no display POST (1F802041h).
    [[nodiscard]] std::optional<u8> post_code() const noexcept { return post_code_; }

private:
    enum class Access : u8 { Read, Write, Fetch };

    ReadResult access(Access kind, AccessWidth width, u32 vaddr, u32 value);
    ReadResult access_ram(Access kind, AccessWidth width, u32 phys, u32 value);
    ReadResult access_io(Access kind, AccessWidth width, u32 phys, u32 value);
    ReadResult access_expansion2(Access kind, AccessWidth width, u32 phys, u32 value);
    ReadResult access_bios(Access kind, AccessWidth width, u32 phys);
    ReadResult access_kseg2(Access kind, AccessWidth width, u32 vaddr, u32 value);
    ReadResult bus_error(Access kind, AccessWidth width, u32 vaddr);

    memory::MainRam ram_;
    memory::Scratchpad scratchpad_;
    memory::BiosRom bios_;
    memory::MemoryControl memory_control_;

    std::array<UnimplementedDevice, static_cast<usize>(IoSlot::Count)> placeholders_;
    std::array<IoDevice*, static_cast<usize>(IoSlot::Count)> devices_{};

    std::optional<u8> post_code_;
    bool warned_bios_write_ = false;
    bool warned_ram_bank1_ = false;
    bool warned_kseg2_ = false;
};

} // namespace rpcs1::bus
