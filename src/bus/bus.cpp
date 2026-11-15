#include "bus/bus.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "core/assert.hpp"
#include "core/log.hpp"

namespace rpcs1::bus {

namespace {

using log::Category;
using memory::DelayDevice;

// Faixas conforme o mapa de E/S do psx-spx. Lacunas entre elas geram erro de barramento.
constexpr std::array<IoSlotInfo, static_cast<usize>(IoSlot::Count)> io_slots = {{
    {IoSlot::Sio0, 0x1F801040, 0x1F801050, "SIO0 (controles/Memory Cards)", 18},
    {IoSlot::Sio1, 0x1F801050, 0x1F801060, "SIO1 (porta serial)", 18},
    {IoSlot::Interrupts, 0x1F801070, 0x1F801078, "controlador de interrupções", 4},
    {IoSlot::Dma, 0x1F801080, 0x1F801100, "DMA", 6},
    {IoSlot::Timers, 0x1F801100, 0x1F801130, "timers", 5},
    {IoSlot::Cdrom, 0x1F801800, 0x1F801804, "CD-ROM", 12},
    {IoSlot::Gpu, 0x1F801810, 0x1F801818, "GPU", 7},
    {IoSlot::Mdec, 0x1F801820, 0x1F801828, "MDEC", 10},
    {IoSlot::Spu, 0x1F801C00, 0x1F802000, "SPU", 11},
}};

template <usize... I>
std::array<UnimplementedDevice, sizeof...(I)> make_placeholders(std::index_sequence<I...>) {
    return {UnimplementedDevice(std::string(io_slots[I].name), io_slots[I].phase)...};
}

template <typename Block>
u32 read_block(const Block& block, u32 offset, AccessWidth width) noexcept {
    switch (width) {
    case AccessWidth::Byte: return block.read8(offset);
    case AccessWidth::Half: return block.read16(offset);
    case AccessWidth::Word: return block.read32(offset);
    }
    return 0;
}

template <typename Block>
void write_block(Block& block, u32 offset, AccessWidth width, u32 value) noexcept {
    switch (width) {
    case AccessWidth::Byte: block.write8(offset, static_cast<u8>(value)); break;
    case AccessWidth::Half: block.write16(offset, static_cast<u16>(value)); break;
    case AccessWidth::Word: block.write32(offset, value); break;
    }
}

constexpr u32 open_bus_value(AccessWidth width) noexcept {
    return 0x01010101u * Bus::open_bus_byte & width_mask(width);
}

// Janela endereçável de uma região de expansão/BIOS, limitada ao tamanho da região.
u32 window(const memory::MemoryControl& mc, DelayDevice device, u32 region_size) noexcept {
    return static_cast<u32>(std::min<u64>(mc.window_size(device), region_size));
}

constexpr bool in_range(u32 address, u32 begin, u32 size) noexcept {
    return address >= begin && address - begin < size;
}

} // namespace

std::string_view to_string(BusFault fault) noexcept {
    switch (fault) {
    case BusFault::None: return "nenhuma";
    case BusFault::AddressError: return "erro de endereço (desalinhado)";
    case BusFault::BusError: return "erro de barramento";
    }
    return "?";
}

const IoSlotInfo& io_slot_info(IoSlot slot) noexcept {
    const auto index = static_cast<usize>(slot);
    RPCS1_ASSERT(index < io_slots.size(), "IoSlot inválido");
    return io_slots[index];
}

Bus::Bus() : placeholders_(make_placeholders(std::make_index_sequence<io_slots.size()>{})) {
    for (usize i = 0; i < placeholders_.size(); ++i) {
        devices_[i] = &placeholders_[i];
    }
    power_on();
}

void Bus::power_on() {
    // ⚠️ O conteúdo da RAM ao ligar é indefinido no hardware; o RPCS1 usa zeros para ser
    // determinístico (testes e save states reprodutíveis).
    ram_.fill(0);
    scratchpad_.fill(0);
    reset();
}

void Bus::reset() {
    memory_control_.reset();
    post_code_.reset();
}

void Bus::attach_device(IoSlot slot, IoDevice& device) noexcept {
    devices_[static_cast<usize>(slot)] = &device;
}

IoDevice& Bus::device(IoSlot slot) noexcept {
    return *devices_[static_cast<usize>(slot)];
}

ReadResult Bus::read(AccessWidth width, u32 vaddr) {
    return access(Access::Read, width, vaddr, 0);
}

BusFault Bus::write(AccessWidth width, u32 vaddr, u32 value) {
    return access(Access::Write, width, vaddr, value).fault;
}

ReadResult Bus::fetch_instruction(u32 vaddr) {
    return access(Access::Fetch, AccessWidth::Word, vaddr, 0);
}

ReadResult Bus::bus_error(Access kind, AccessWidth width, u32 vaddr) {
    log::debug(Category::Bus, "Erro de barramento: {} de {} em {:08X}h",
               kind == Access::Write ? "escrita" : (kind == Access::Fetch ? "busca" : "leitura"),
               to_string(width), vaddr);
    return ReadResult{0, BusFault::BusError};
}

ReadResult Bus::access(Access kind, AccessWidth width, u32 vaddr, u32 value) {
    if (!is_aligned(vaddr, width)) {
        return ReadResult{0, BusFault::AddressError};
    }

    const Segment segment = segment_of(vaddr);
    if (segment == Segment::Kseg2) {
        return access_kseg2(kind, width, vaddr, value);
    }
    // KUSEG só tem os primeiros 512 MiB; o restante gera exceção.
    if (segment == Segment::Kuseg && vaddr >= 0x20000000u) {
        return bus_error(kind, width, vaddr);
    }

    const u32 phys = to_physical(vaddr);

    if (phys < ram_space_size) {
        return access_ram(kind, width, phys, value);
    }

    if (in_range(phys, expansion1_begin, expansion1_region)) {
        // Nenhum dispositivo de expansão é emulado: a região existe dentro da janela
        // configurada, lê como barramento vazio e descarta escritas.
        if (phys - expansion1_begin >= window(memory_control_, DelayDevice::Expansion1,
                                              expansion1_region)) {
            return bus_error(kind, width, vaddr);
        }
        return ReadResult{kind == Access::Write ? 0u : open_bus_value(width), BusFault::None};
    }

    if (in_range(phys, scratchpad_begin, static_cast<u32>(memory::Scratchpad::size))) {
        // O scratchpad só existe em KUSEG e KSEG0 e não é executável.
        if (segment == Segment::Kseg1 || kind == Access::Fetch) {
            return bus_error(kind, width, vaddr);
        }
        if (kind == Access::Write) {
            write_block(scratchpad_, phys - scratchpad_begin, width, value);
            return {};
        }
        return ReadResult{read_block(scratchpad_, phys - scratchpad_begin, width)};
    }

    if (phys >= io_begin && phys < io_end) {
        return access_io(kind, width, phys, value);
    }

    if (in_range(phys, expansion2_begin, expansion2_region)) {
        return access_expansion2(kind, width, phys, value);
    }

    if (in_range(phys, expansion3_begin, expansion3_region)) {
        if (phys - expansion3_begin >= window(memory_control_, DelayDevice::Expansion3,
                                              expansion3_region)) {
            return bus_error(kind, width, vaddr);
        }
        return ReadResult{kind == Access::Write ? 0u : open_bus_value(width), BusFault::None};
    }

    if (in_range(phys, bios_begin, bios_region)) {
        if (phys - bios_begin >= window(memory_control_, DelayDevice::Bios, bios_region)) {
            return bus_error(kind, width, vaddr);
        }
        return access_bios(kind, width, phys);
    }

    return bus_error(kind, width, vaddr);
}

ReadResult Bus::access_ram(Access kind, AccessWidth width, u32 phys, u32 value) {
    const memory::RamLayout layout = memory_control_.ram_layout();
    if (phys < layout.bank0_size) {
        // Os chips têm 2 MiB: bits de endereço acima disso são ignorados (espelhos).
        if (kind == Access::Write) {
            write_block(ram_, phys, width, value);
            return {};
        }
        return ReadResult{read_block(ram_, phys, width)};
    }
    if (phys < layout.mapped_size()) {
        // ⚠️ Banco /RAS1 habilitado, mas consoles de varejo não têm chips nele.
        // Comportamento não documentado: o RPCS1 lê zero e descarta escritas.
        if (!warned_ram_bank1_) {
            warned_ram_bank1_ = true;
            log::warn(Category::Memory,
                      "Acesso ao banco de RAM /RAS1 ({:08X}h), que não tem chips no PS1 de varejo",
                      phys);
        }
        return {};
    }
    return bus_error(kind, width, phys);
}

ReadResult Bus::access_io(Access kind, AccessWidth width, u32 phys, u32 value) {
    if (memory::MemoryControl::contains(phys)) {
        if (kind == Access::Write) {
            memory_control_.write(phys, value, width);
            return {};
        }
        return ReadResult{memory_control_.read(phys, width)};
    }
    for (const auto& slot : io_slots) {
        if (phys >= slot.begin && phys < slot.end) {
            IoDevice& dev = *devices_[static_cast<usize>(slot.slot)];
            if (kind == Access::Write) {
                dev.io_write(phys - slot.begin, value, width);
                return {};
            }
            return ReadResult{dev.io_read(phys - slot.begin, width) & width_mask(width)};
        }
    }
    // Lacuna na área de E/S: erro de barramento (psx-spx, "Memory Exceptions").
    return bus_error(kind, width, phys);
}

ReadResult Bus::access_expansion2(Access kind, AccessWidth width, u32 phys, u32 value) {
    // Qualquer base diferente de 1F802000h parece desabilitar a região (psx-spx).
    if (memory_control_.expansion2_base() != memory::MemoryControl::default_expansion2_base) {
        return bus_error(kind, width, phys);
    }
    const u32 offset = phys - expansion2_begin;
    if (offset >= window(memory_control_, DelayDevice::Expansion2, expansion2_region)) {
        return bus_error(kind, width, phys);
    }
    // ⚠️ psx-spx: "só acessos de 8 bits parecem ser suportados"; maiores parecem gerar exceção.
    if (width != AccessWidth::Byte) {
        return bus_error(kind, width, phys);
    }
    if (kind == Access::Write) {
        if (offset == post_offset) {
            post_code_ = static_cast<u8>(value & 0x0F);
            log::info(Category::Bios, "POST (1F802041h) = {:X}h", *post_code_);
        }
        return {};
    }
    return ReadResult{open_bus_value(width)};
}

ReadResult Bus::access_bios(Access kind, AccessWidth width, u32 phys) {
    if (kind == Access::Write) {
        // ⚠️ Escrita em ROM não documentada; o RPCS1 a descarta sem exceção.
        if (!warned_bios_write_) {
            warned_bios_write_ = true;
            log::warn(Category::Memory, "Escrita na BIOS ROM ({:08X}h) ignorada", phys);
        }
        return {};
    }
    // Janelas maiores que 512 KiB espelham a ROM (linhas de endereço não conectadas).
    return ReadResult{read_block(bios_, phys - bios_begin, width)};
}

ReadResult Bus::access_kseg2(Access kind, AccessWidth width, u32 vaddr, u32 value) {
    // KSEG2 não contém código executável.
    if (kind == Access::Fetch || vaddr < kseg2_control_begin || vaddr >= kseg2_control_end) {
        return bus_error(kind, width, vaddr);
    }
    if ((vaddr & ~3u) == memory::MemoryControl::bcc_address) {
        if (kind == Access::Write) {
            memory_control_.set_cache_control(value);
            return {};
        }
        const u32 shift = (vaddr & 3u) * 8u;
        return ReadResult{(memory_control_.cache_control() >> shift) & width_mask(width)};
    }
    // ⚠️ Demais endereços do bloco FFFE0000h–FFFE01FFh não documentados: lê 0, descarta escrita.
    if (!warned_kseg2_) {
        warned_kseg2_ = true;
        log::warn(Category::Memory, "Acesso a registrador de controle não documentado {:08X}h",
                  vaddr);
    }
    return {};
}

} // namespace rpcs1::bus
