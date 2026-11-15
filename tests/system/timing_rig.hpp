// Ambiente de teste da base de tempo: scheduler + vídeo + timers, com relógio controlado.
#pragma once

#include <vector>

#include "gpu/video_timing.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/scheduler.hpp"
#include "timers/timers.hpp"

namespace rpcs1::test {

class TimingRig {
public:
    explicit TimingRig(Region region = Region::NtscUC) {
        scheduler.set_time_source(&now);
        video.set_listener(&timers);
        video.reset(region, 0);
        timers.reset(0);
    }

    // Avança o tempo até `t`, despachando os eventos em ordem e registrando os IRQ0.
    void advance_to(u64 t) {
        while (scheduler.next_time() <= t) {
            const auto event = scheduler.pop_due(t);
            if (!event) {
                break;
            }
            now = event->time;
            dispatch(event->id, event->time);
        }
        now = t;
    }

    // Avança até a próxima borda de início de HBLANK; retorna o horário.
    u64 advance_to_next_hblank_start() {
        const u64 before = video.hblank_starts();
        while (video.hblank_starts() == before) {
            const auto event = scheduler.pop_due(scheduler.next_time());
            now = event->time;
            dispatch(event->id, event->time);
        }
        return now;
    }

    u32 read(u32 offset, bus::AccessWidth width = bus::AccessWidth::Word) {
        return timers.io_read(offset, width);
    }
    void write(u32 offset, u32 value) { timers.io_write(offset, value, bus::AccessWidth::Word); }

    u64 now = 0;
    system::Scheduler scheduler;
    interrupts::InterruptController irq;
    gpu::VideoTiming video{scheduler, irq};
    timers::Timers timers{scheduler, irq, video};
    std::vector<u64> vblank_irq_times;

private:
    void dispatch(system::EventId id, u64 time) {
        switch (id) {
        case system::EventId::Video:
            video.on_event(time);
            if (irq.status() & 1u) {
                vblank_irq_times.push_back(time);
                irq.io_write(0, ~1u, bus::AccessWidth::Word); // reconhece o IRQ0
            }
            break;
        case system::EventId::Timer0: timers.on_event(0, time); break;
        case system::EventId::Timer1: timers.on_event(1, time); break;
        case system::EventId::Timer2: timers.on_event(2, time); break;
        default: break;
        }
    }
};

} // namespace rpcs1::test
