#include "system/scheduler.hpp"

namespace rpcs1::system {

std::string_view to_string(EventId id) noexcept {
    switch (id) {
    case EventId::Video: return "vídeo";
    case EventId::Timer0: return "timer 0";
    case EventId::Timer1: return "timer 1";
    case EventId::Timer2: return "timer 2";
    case EventId::Count: break;
    }
    return "?";
}

void Scheduler::reset() noexcept {
    times_.fill(never);
    next_ = never;
}

void Scheduler::schedule(EventId id, u64 time) noexcept {
    times_[static_cast<usize>(id)] = time;
    recompute_next();
}

void Scheduler::cancel(EventId id) noexcept {
    times_[static_cast<usize>(id)] = never;
    recompute_next();
}

std::optional<u64> Scheduler::scheduled(EventId id) const noexcept {
    const u64 t = times_[static_cast<usize>(id)];
    if (t == never) {
        return std::nullopt;
    }
    return t;
}

std::optional<DueEvent> Scheduler::pop_due(u64 time) noexcept {
    if (next_ > time) {
        return std::nullopt;
    }
    usize best = count;
    for (usize i = 0; i < count; ++i) {
        if (times_[i] <= time && (best == count || times_[i] < times_[best])) {
            best = i;
        }
    }
    if (best == count) {
        return std::nullopt;
    }
    const DueEvent event{static_cast<EventId>(best), times_[best]};
    times_[best] = never;
    recompute_next();
    return event;
}

void Scheduler::recompute_next() noexcept {
    u64 next = never;
    for (const u64 t : times_) {
        if (t < next) {
            next = t;
        }
    }
    next_ = next;
}

} // namespace rpcs1::system
