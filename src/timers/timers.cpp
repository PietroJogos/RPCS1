#include "timers/timers.hpp"

#include <algorithm>

namespace rpcs1::timers {

namespace {

namespace bit {
inline constexpr u16 sync_enable = 1u << 0;
inline constexpr u16 reset_at_target = 1u << 3;
inline constexpr u16 irq_target = 1u << 4;
inline constexpr u16 irq_ffff = 1u << 5;
inline constexpr u16 repeat = 1u << 6;
inline constexpr u16 toggle = 1u << 7;
inline constexpr u16 source = 1u << 8;   // T0: dotclock, T1: HBLANK
inline constexpr u16 prescale = 1u << 9; // T2: sistema / 8
} // namespace bit

constexpr u64 infinite = ~u64{0};

constexpr system::EventId event_for(u32 i) noexcept {
    return static_cast<system::EventId>(static_cast<u32>(system::EventId::Timer0) + i);
}

constexpr interrupts::Irq irq_for(u32 i) noexcept {
    return static_cast<interrupts::Irq>(static_cast<u32>(interrupts::Irq::Timer0) + i);
}

constexpr u32 sync_mode(u16 mode) noexcept {
    return (mode >> 1) & 3u;
}

} // namespace

Timers::Timers(system::Scheduler& scheduler, interrupts::InterruptController& irq,
               const gpu::VideoTiming& video)
    : scheduler_(scheduler), irq_(irq), video_(video) {}

void Timers::reset(u64 now) {
    for (u32 i = 0; i < 3; ++i) {
        timers_[i] = Timer{};
        timers_[i].last_sync = now;
        scheduler_.cancel(event_for(i));
    }
}

bool Timers::system_clock_source(u32 i) const noexcept {
    const u16 mode = timers_[i].mode;
    return i == 2 ? (mode & bit::prescale) == 0 : (mode & bit::source) == 0;
}

bool Timers::hblank_source(u32 i) const noexcept {
    return i == 1 && (timers_[1].mode & bit::source) != 0;
}

bool Timers::paused(u32 i) const noexcept {
    const Timer& t = timers_[i];
    if ((t.mode & bit::sync_enable) == 0) {
        return false;
    }
    const u32 sm = sync_mode(t.mode);
    if (i == 2) {
        return sm == 0 || sm == 3; // param o contador
    }
    const bool blank = i == 0 ? video_.in_hblank() : video_.in_vblank();
    switch (sm) {
    case 0: return blank;    // pausa durante o blank
    case 1: return false;    // reinicia no blank, conta sempre
    case 2: return !blank;   // reinicia no blank, conta só durante o blank
    default: return t.waiting; // espera um blank e depois conta livremente
    }
}

u64 Timers::ticks_between(u32 i, u64 a, u64 b) const noexcept {
    if (b <= a) {
        return 0;
    }
    const u16 mode = timers_[i].mode;
    switch (i) {
    case 0:
        return (mode & bit::source) ? video_.dots(b) - video_.dots(a) : b - a;
    case 1:
        return (mode & bit::source) ? 0 : b - a; // HBLANK: contado nas bordas
    default:
        // ⚠️ Fase do prescaler /8 em relação ao tempo absoluto: não documentada.
        return (mode & bit::prescale) ? b / 8 - a / 8 : b - a;
    }
}

u64 Timers::time_after_ticks(u32 i, u64 from, u64 n) const noexcept {
    const u16 mode = timers_[i].mode;
    if (i == 0 && (mode & bit::source)) {
        return video_.time_after_dots(from, n);
    }
    if (i == 2 && (mode & bit::prescale)) {
        return (from / 8 + n) * 8;
    }
    return from + n;
}

void Timers::sync(u32 i, u64 now) {
    Timer& t = timers_[i];
    if (now <= t.last_sync) {
        return;
    }
    const u64 ticks = paused(i) ? 0 : ticks_between(i, t.last_sync, now);
    t.last_sync = now;
    if (ticks > 0) {
        advance(i, ticks, now);
    }
}

void Timers::advance(u32 i, u64 ticks, u64 time) {
    Timer& t = timers_[i];
    const bool zret = (t.mode & bit::reset_at_target) != 0;
    const bool fast = system_clock_source(i);

    while (ticks > 0) {
        if (t.pending_reset) { // clock do sistema: o alvo fica visível 1 ciclo
            t.counter = 0;
            t.pending_reset = false;
            t.hold = true;
            --ticks;
            continue;
        }
        if (t.hold) { // ...e o 0 permanece por 2 ciclos
            t.hold = false;
            --ticks;
            continue;
        }
        if (zret && t.target == 0) {
            t.counter = 0;
            t.reached_target = true;
            return;
        }
        const u32 c = t.counter;
        const u32 target = t.target;
        const u32 to_ffff = c < 0xFFFFu ? 0xFFFFu - c : 0x10000u;
        const u32 to_target = c < target ? target - c : 0x10000u - c + target;
        const u64 d = std::min(to_ffff, to_target);
        if (ticks < d) {
            t.counter = static_cast<u16>((c + ticks) & 0xFFFFu);
            return;
        }
        ticks -= d;
        t.counter = static_cast<u16>((c + d) & 0xFFFFu);
        const bool hit_target = t.counter == target;
        const bool hit_ffff = t.counter == 0xFFFFu;
        if (hit_target) t.reached_target = true;
        if (hit_ffff) t.reached_ffff = true;
        // ⚠️ Alvo 0 nunca gera IRQ (medido com reset); FFFFh e alvo coincidentes geram um só.
        const bool irq = (hit_target && target != 0 && (t.mode & bit::irq_target)) ||
                         (hit_ffff && (t.mode & bit::irq_ffff));
        if (irq) {
            fire(i, time);
        }
        if (hit_target && zret) {
            if (fast) {
                t.pending_reset = true;
            } else {
                t.counter = 0; // fontes lentas: o reset ocorre 1 ciclo do sistema depois
            }
        }
    }
}

void Timers::fire(u32 i, u64 time) {
    Timer& t = timers_[i];
    const bool repeat = (t.mode & bit::repeat) != 0;
    if (!repeat && !t.armed) {
        return; // modo "uma vez": já disparou
    }
    if (!repeat) {
        t.armed = false;
    }
    if (t.mode & bit::toggle) {
        t.intf = !t.intf;
        if (t.intf) {
            return; // só a transição 1 → 0 pede interrupção
        }
    } else {
        t.intf_low_until = time + 2; // ⚠️ duração do pulso: "alguns ciclos"
    }
    ++t.irq_requests;
    irq_.request(irq_for(i));
}

void Timers::reschedule(u32 i, u64 now) {
    const Timer& t = timers_[i];
    const bool zret = (t.mode & bit::reset_at_target) != 0;
    const bool can_fire = (t.mode & bit::repeat) || t.armed;
    const bool want_target = (t.mode & bit::irq_target) && t.target != 0;
    const bool want_ffff = (t.mode & bit::irq_ffff) != 0;
    if (!can_fire || (!want_target && !want_ffff) || paused(i) || hblank_source(i) ||
        (zret && t.target == 0)) {
        scheduler_.cancel(event_for(i));
        return;
    }

    u64 lead = 0;
    u32 c = t.counter;
    if (t.pending_reset) {
        lead = 2;
        c = 0;
    } else if (t.hold) {
        lead = 1;
    }
    u64 d_target = infinite;
    if (want_target) {
        d_target = c < t.target ? t.target - c : 0x10000u - c + t.target;
    }
    u64 d_ffff = infinite;
    if (want_ffff && !(zret && c <= t.target && t.target != 0xFFFFu)) {
        d_ffff = c < 0xFFFFu ? 0xFFFFu - c : 0x10000u;
    }
    const u64 d = std::min(d_target, d_ffff);
    if (d == infinite) {
        scheduler_.cancel(event_for(i));
        return;
    }
    scheduler_.schedule(event_for(i), time_after_ticks(i, now, lead + d));
}

void Timers::on_event(u32 index, u64 time) {
    sync(index, time);
    reschedule(index, time);
}

void Timers::sync_to(u64 time) {
    for (u32 i = 0; i < 3; ++i) {
        sync(i, time);
    }
}

void Timers::blank_end(u32 i, u64 time) {
    Timer& t = timers_[i];
    if ((t.mode & bit::sync_enable) == 0) {
        return;
    }
    switch (sync_mode(t.mode)) {
    case 1:
    case 2:
        t.counter = 0;
        t.pending_reset = false;
        t.hold = false;
        break;
    case 3: t.waiting = false; break;
    default: break;
    }
    (void)time;
}

void Timers::on_hblank(bool start, u64 time) {
    if (start) {
        if (hblank_source(1) && !paused(1)) {
            advance(1, 1, time);
        }
    } else {
        blank_end(0, time);
    }
    reschedule(0, time);
    reschedule(1, time);
}

void Timers::on_vblank(bool start, u64 time) {
    if (!start) {
        blank_end(1, time);
    }
    reschedule(1, time);
}

u32 Timers::compose_mode(u32 i, u64 now) const noexcept {
    const Timer& t = timers_[i];
    const bool intf = (t.mode & bit::toggle) ? t.intf : now >= t.intf_low_until;
    return static_cast<u32>(t.mode & 0x3FFu) | (intf ? 1u << 10 : 0u) |
           (t.reached_target ? 1u << 11 : 0u) | (t.reached_ffff ? 1u << 12 : 0u);
}

u16 Timers::counter(u32 index, u64 now) {
    sync(index, now);
    return timers_[index].counter;
}

u32 Timers::peek_mode(u32 index, u64 now) const {
    return compose_mode(index, now);
}

u32 Timers::io_read(u32 offset, bus::AccessWidth width) {
    const u32 i = offset / 0x10;
    const u32 reg = (offset % 0x10) / 4;
    const u32 byte = offset & 3u;
    if (i > 2) {
        return 0;
    }
    const u64 now = scheduler_.now();
    sync(i, now);
    Timer& t = timers_[i];
    u32 value = 0;
    switch (reg) {
    case 0: value = t.counter; break;
    case 1:
        value = compose_mode(i, now);
        // Leitura de 16/32 bits ou do byte alto limpa os flags; a do byte baixo não (medido).
        if (width == bus::AccessWidth::Word || (width == bus::AccessWidth::Half && byte == 0) ||
            (width == bus::AccessWidth::Byte && byte == 1)) {
            t.reached_target = false;
            t.reached_ffff = false;
        }
        break;
    case 2: value = t.target; break;
    default: value = 0; break; // +Ch: não usado (⚠️ lido como zero)
    }
    // ⚠️ Bits 16–31 são "lixo" no hardware; o RPCS1 os lê como zero.
    return (value >> (byte * 8)) & bus::width_mask(width);
}

void Timers::io_write(u32 offset, u32 value, bus::AccessWidth /*width*/) {
    const u32 i = offset / 0x10;
    const u32 reg = (offset % 0x10) / 4;
    if (i > 2) {
        return;
    }
    const u64 now = scheduler_.now();
    sync(i, now);
    Timer& t = timers_[i];
    switch (reg) {
    case 0:
        // COUNT: o valor é o registrador inteiro da CPU (D-014); só 16 bits existem.
        t.counter = static_cast<u16>(value);
        t.pending_reset = false;
        t.hold = false;
        t.armed = true; // escrever o contador rearma o modo "uma vez" (medido com 0)
        if (t.counter == t.target) {
            t.reached_target = true; // medido: escrever o próprio alvo liga o flag
        }
        break;
    case 1:
        t.mode = static_cast<u16>(value & 0x3FFu);
        t.counter = 0;
        t.reached_target = false;
        t.reached_ffff = false;
        t.intf = true;
        t.intf_low_until = 0;
        t.armed = true;
        t.pending_reset = false;
        // psx-spx: escrever MODE mantém 0 por 2 ciclos (clock do sistema).
        t.hold = system_clock_source(i);
        t.waiting = i < 2 && (t.mode & bit::sync_enable) && sync_mode(t.mode) == 3;
        break;
    case 2: t.target = static_cast<u16>(value); break;
    default: break;
    }
    reschedule(i, now);
}

} // namespace rpcs1::timers
