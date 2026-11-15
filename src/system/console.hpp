// O console emulado: possui e interliga os componentes já implementados.
#pragma once

#include <filesystem>

#include "bus/bus.hpp"
#include "core/error.hpp"
#include "cpu/cpu.hpp"
#include "interrupts/interrupt_controller.hpp"

namespace rpcs1::system {

class Console {
public:
    Console();

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // Ligar: zera RAM e registradores de todos os componentes; mantém a BIOS carregada.
    void power_on();
    // Botão de reset: reinicia CPU e periféricos; a RAM preserva o conteúdo.
    void reset();

    [[nodiscard]] Status load_bios(const std::filesystem::path& path);

    [[nodiscard]] bus::Bus& bus() noexcept { return bus_; }
    [[nodiscard]] cpu::Cpu& cpu() noexcept { return cpu_; }
    [[nodiscard]] interrupts::InterruptController& interrupts() noexcept { return irq_; }

private:
    bus::Bus bus_;
    interrupts::InterruptController irq_;
    cpu::Cpu cpu_;
};

} // namespace rpcs1::system
