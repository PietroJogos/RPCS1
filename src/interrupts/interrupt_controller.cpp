#include "interrupts/interrupt_controller.hpp"

namespace rpcs1::interrupts {

std::string_view to_string(Irq irq) noexcept {
    switch (irq) {
    case Irq::VBlank: return "VBLANK";
    case Irq::Gpu: return "GPU";
    case Irq::Cdrom: return "CDROM";
    case Irq::Dma: return "DMA";
    case Irq::Timer0: return "TMR0";
    case Irq::Timer1: return "TMR1";
    case Irq::Timer2: return "TMR2";
    case Irq::ControllerMemoryCard: return "Controle/Memory Card";
    case Irq::Sio: return "SIO";
    case Irq::Spu: return "SPU";
    case Irq::Lightpen: return "Lightpen";
    }
    return "?";
}

void InterruptController::reset() noexcept {
    stat_ = 0;
    mask_ = 0;
}

void InterruptController::request(Irq irq) noexcept {
    stat_ |= (1u << static_cast<u32>(irq)) & implemented_bits;
}

u32 InterruptController::io_read(u32 offset, bus::AccessWidth width) {
    // ⚠️ Bits 16–31 são "lixo" no hardware; o RPCS1 os lê como zero.
    const u32 value = offset < 4 ? stat_ : mask_;
    return (value >> ((offset & 3u) * 8)) & bus::width_mask(width);
}

void InterruptController::io_write(u32 offset, u32 value, bus::AccessWidth /*width*/) {
    // O valor é o registrador inteiro da CPU, mesmo em SB/SH (D-014).
    if (offset < 4) {
        stat_ &= value | ~implemented_bits; // 0 reconhece (limpa); 1 mantém
        stat_ &= implemented_bits;
    } else {
        mask_ = value & implemented_bits;
    }
}

} // namespace rpcs1::interrupts
