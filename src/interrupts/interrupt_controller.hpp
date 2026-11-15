// Controlador de interrupções: I_STAT (1F801070h) e I_MASK (1F801074h). (Fase 4)
//
// Fonte: psx-spx, "Interrupts".
//   - I_STAT: leitura = pedidos pendentes; escrita: bit 0 limpa, bit 1 mantém.
//   - I_MASK: habilitação (L/E). Só os bits 0–10 existem.
//   - Pedidos são disparados por borda: os dispositivos chamam request() na transição.
//   - (I_STAT AND I_MASK) != 0 liga CAUSE bit 10 na CPU (não é um latch).
#pragma once

#include <string_view>

#include "bus/io_device.hpp"
#include "core/types.hpp"
#include "cpu/interrupt_line.hpp"

namespace rpcs1::interrupts {

enum class Irq : u8 {
    VBlank = 0,
    Gpu = 1,
    Cdrom = 2,
    Dma = 3,
    Timer0 = 4,
    Timer1 = 5,
    Timer2 = 6,
    ControllerMemoryCard = 7,
    Sio = 8,
    Spu = 9,
    Lightpen = 10,
};

[[nodiscard]] std::string_view to_string(Irq irq) noexcept;

class InterruptController final : public bus::IoDevice, public cpu::InterruptLine {
public:
    static constexpr u32 implemented_bits = 0x7FF; // bits 0–10

    void reset() noexcept;

    // Borda de subida de uma fonte de interrupção.
    void request(Irq irq) noexcept;

    [[nodiscard]] u32 status() const noexcept { return stat_; }
    [[nodiscard]] u32 mask() const noexcept { return mask_; }
    [[nodiscard]] bool irq_asserted() const noexcept override { return (stat_ & mask_) != 0; }

    // Interface de E/S: deslocamento 0–3 = I_STAT, 4–7 = I_MASK.
    [[nodiscard]] std::string_view name() const noexcept override {
        return "controlador de interrupções";
    }
    [[nodiscard]] u32 io_read(u32 offset, bus::AccessWidth width) override;
    void io_write(u32 offset, u32 value, bus::AccessWidth width) override;

private:
    u32 stat_ = 0;
    u32 mask_ = 0;
};

} // namespace rpcs1::interrupts
