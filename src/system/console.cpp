#include "system/console.hpp"

namespace rpcs1::system {

Console::Console(Region region)
    : region_(region), cpu_(bus_), video_(scheduler_, irq_), timers_(scheduler_, irq_, video_) {
    bus_.attach_device(bus::IoSlot::Interrupts, irq_);
    bus_.attach_device(bus::IoSlot::Timers, timers_);
    cpu_.set_interrupt_line(&irq_);
    scheduler_.set_time_source(&cpu_.cycle_counter());
    video_.set_listener(&timers_);
    reset_time_domain();
}

void Console::reset_time_domain() {
    // A CPU já recomeçou do ciclo 0: o vídeo e os timers recomeçam junto.
    scheduler_.reset();
    video_.reset(region_, cpu_.cycles());
    timers_.reset(cpu_.cycles());
}

void Console::power_on() {
    bus_.power_on();
    irq_.reset();
    cpu_.reset();
    reset_time_domain();
}

void Console::reset() {
    bus_.reset();
    irq_.reset();
    cpu_.reset();
    reset_time_domain();
}

Status Console::load_bios(const std::filesystem::path& path) {
    return bus_.bios().load_file(path);
}

void Console::dispatch_events() {
    const u64 now = cpu_.cycles();
    while (const auto event = scheduler_.pop_due(now)) {
        switch (event->id) {
        case EventId::Video: video_.on_event(event->time); break;
        case EventId::Timer0: timers_.on_event(0, event->time); break;
        case EventId::Timer1: timers_.on_event(1, event->time); break;
        case EventId::Timer2: timers_.on_event(2, event->time); break;
        case EventId::Count: break;
        }
    }
}

u64 Console::run(u64 max_steps) {
    const u64 start = cpu_.instructions();
    for (u64 i = 0; i < max_steps; ++i) {
        if (scheduler_.next_time() <= cpu_.cycles()) {
            dispatch_events();
        }
        const cpu::StepResult r = cpu_.step();
        if (r == cpu::StepResult::Unimplemented || r == cpu::StepResult::Halted) {
            break;
        }
    }
    return cpu_.instructions() - start;
}

} // namespace rpcs1::system
