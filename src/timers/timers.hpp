// Timers 0, 1 e 2 (root counters) em 1F801100h–1F80112Fh (Fase 5).
//
// Fontes: psx-spx "Timers" e medições em SCPH-5501 do psx.wiki ("Root Counter Behavior",
// "Root Counter Mode Combinations", "Display Timing"). Cada timer: COUNT (+0), MODE (+4),
// TARGET (+8).
//   - Fontes de clock: T0 = sistema ou dotclock; T1 = sistema ou HBLANK; T2 = sistema ou /8.
//   - Reset no alvo (bit 3): com o clock do sistema o período é alvo + 2 ciclos (0 aparece por
//     2 ciclos); alvo 0 com reset deixa o contador preso em 0, com o flag de alvo ligado e sem IRQ.
//   - IRQ: pulso (bit 10 cai brevemente) ou alternância (bit 10 inverte; IRQ só na queda), uma
//     vez (até nova escrita em MODE ou COUNT) ou repetido.
//   - Flags 11/12: ligados ao atingir alvo/FFFFh; limpos por leitura de MODE (16/32 bits ou byte
//     alto), não por leitura do byte baixo, de COUNT ou de TARGET.
//   - Sincronização (bit 0, modos nos bits 1–2): T0 com HBLANK, T1 com VBLANK; T2: modos 0/3
//     param o contador, 1/2 contam livremente. Reset e liberação ocorrem no FIM do blank (D-032).
#pragma once

#include <array>
#include <string_view>

#include "bus/io_device.hpp"
#include "core/types.hpp"
#include "gpu/video_timing.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/scheduler.hpp"

namespace rpcs1::timers {

class Timers final : public bus::IoDevice, public gpu::BlankListener {
public:
    Timers(system::Scheduler& scheduler, interrupts::InterruptController& irq,
           const gpu::VideoTiming& video);

    void reset(u64 now);

    // Evento agendado do timer `index` (alvo/FFFFh previsto).
    void on_event(u32 index, u64 time);

    // gpu::BlankListener
    void sync_to(u64 time) override;
    void on_hblank(bool start, u64 time) override;
    void on_vblank(bool start, u64 time) override;

    // bus::IoDevice
    [[nodiscard]] std::string_view name() const noexcept override { return "timers"; }
    [[nodiscard]] u32 io_read(u32 offset, bus::AccessWidth width) override;
    void io_write(u32 offset, u32 value, bus::AccessWidth width) override;

    // Inspeção (testes e depuração): não altera flags.
    [[nodiscard]] u16 counter(u32 index, u64 now);
    [[nodiscard]] u32 peek_mode(u32 index, u64 now) const;
    [[nodiscard]] u64 irq_requests(u32 index) const noexcept { return timers_[index].irq_requests; }

private:
    struct Timer {
        u16 counter = 0;
        u16 target = 0;
        u16 mode = 0;           // bits 0–9 gravados
        bool reached_target = false;
        bool reached_ffff = false;
        bool intf = true;       // bit 10 em modo alternância (1 = sem pedido)
        u64 intf_low_until = 0; // modo pulso: bit 10 lê 0 até este ciclo
        bool armed = true;      // modo "uma vez"
        bool pending_reset = false;
        bool hold = false;
        bool waiting = false;   // sincronização modo 3
        u64 last_sync = 0;
        u64 irq_requests = 0;
    };

    void sync(u32 i, u64 now);
    void advance(u32 i, u64 ticks, u64 time);
    void fire(u32 i, u64 time);
    void reschedule(u32 i, u64 now);
    [[nodiscard]] bool paused(u32 i) const noexcept;
    [[nodiscard]] bool system_clock_source(u32 i) const noexcept;
    [[nodiscard]] bool hblank_source(u32 i) const noexcept;
    [[nodiscard]] u64 ticks_between(u32 i, u64 a, u64 b) const noexcept;
    [[nodiscard]] u64 time_after_ticks(u32 i, u64 from, u64 n) const noexcept;
    [[nodiscard]] u32 compose_mode(u32 i, u64 now) const noexcept;
    void blank_end(u32 i, u64 time);

    system::Scheduler& scheduler_;
    interrupts::InterruptController& irq_;
    const gpu::VideoTiming& video_;
    std::array<Timer, 3> timers_{};
};

} // namespace rpcs1::timers
