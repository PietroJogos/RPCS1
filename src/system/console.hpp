// O console emulado: possui e interliga os componentes já implementados.
//
// Base de tempo única: o contador de ciclos da CPU. Antes de cada instrução, os eventos
// vencidos do scheduler (bordas de vídeo, timers) são despachados em ordem de tempo.
#pragma once

#include <filesystem>

#include "bus/bus.hpp"
#include "core/console_model.hpp"
#include "core/error.hpp"
#include "cpu/cpu.hpp"
#include "dma/dma_controller.hpp"
#include "gpu/video_timing.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/scheduler.hpp"
#include "timers/timers.hpp"

namespace rpcs1::system {

class Console {
public:
    // A região define o cristal de vídeo (NTSC/PAL) do modelo de console.
    explicit Console(Region region = Region::NtscUC);

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // Ligar: zera RAM, registradores e o tempo; mantém a BIOS carregada.
    void power_on();
    // Botão de reset: reinicia CPU, periféricos e o tempo; a RAM preserva o conteúdo.
    void reset();

    [[nodiscard]] Status load_bios(const std::filesystem::path& path);

    // Executa até `max_steps` passos da CPU, despachando eventos; para se a CPU parar.
    // Retorna o número de instruções executadas.
    u64 run(u64 max_steps);
    // Despacha os eventos vencidos até o ciclo atual da CPU.
    void dispatch_events();

    [[nodiscard]] Region region() const noexcept { return region_; }
    [[nodiscard]] bus::Bus& bus() noexcept { return bus_; }
    [[nodiscard]] cpu::Cpu& cpu() noexcept { return cpu_; }
    [[nodiscard]] interrupts::InterruptController& interrupts() noexcept { return irq_; }
    [[nodiscard]] Scheduler& scheduler() noexcept { return scheduler_; }
    [[nodiscard]] gpu::VideoTiming& video() noexcept { return video_; }
    [[nodiscard]] timers::Timers& timers() noexcept { return timers_; }
    [[nodiscard]] dma::DmaController& dma() noexcept { return dma_; }

private:
    void reset_time_domain();

    Region region_;
    bus::Bus bus_;
    interrupts::InterruptController irq_;
    cpu::Cpu cpu_;
    Scheduler scheduler_;
    gpu::VideoTiming video_;
    timers::Timers timers_;
    dma::DmaController dma_;
};

} // namespace rpcs1::system
