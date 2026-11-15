#include "gpu/video_timing.hpp"

#include <algorithm>

namespace rpcs1::gpu {

namespace {

constexpr u32 line_length_for(VideoMode mode) noexcept {
    return mode == VideoMode::Pal ? 3406u : 3413u;
}

// Entrelaçado: NTSC alterna 263/262 e PAL 313/312 linhas (medido com o bit PAL ligado num
// console NTSC). ⚠️ Qual campo vem primeiro não é documentado: o par começa pelo maior.
constexpr u32 lines_for(VideoMode mode, bool interlaced, bool odd_field) noexcept {
    if (!interlaced) {
        return mode == VideoMode::Pal ? 314u : 263u;
    }
    const u32 longer = mode == VideoMode::Pal ? 313u : 263u;
    return odd_field ? longer - 1 : longer;
}

} // namespace

VideoTiming::VideoTiming(system::Scheduler& scheduler, interrupts::InterruptController& irq)
    : scheduler_(scheduler), irq_(irq) {}

void VideoTiming::reset(Region console_region, u64 now) {
    clock_ = system::ClockRatio(console_region == Region::Pal ? system::video_clock_pal_hz
                                                              : system::video_clock_ntsc_hz);
    has_pending_ = false;
    odd_field_ = false;
    apply_timing(DisplayTiming{});
    anchor_v_ = clock_.from_cpu(now);
    line_index_ = 0;
    vline_ = 0;
    frames_ = 0;
    hblank_starts_ = 0;
    vblank_starts_ = 0;

    // Estado no HSYNC da linha 0, como se as bordas anteriores já tivessem ocorrido.
    const DisplayTiming& t = timing_;
    if (t.x1 == t.x2) {
        hblank_ = true;
    } else {
        hblank_ = t.x1 < t.x2 ? t.x1 > 0 : false;
    }
    if (t.y1 == t.y2) {
        vblank_ = false;
    } else {
        vblank_ = t.y1 < t.y2 ? t.y1 > 0 : t.y2 == 0;
    }
    schedule_next_edge(0, true);
}

void VideoTiming::set_display_timing(const DisplayTiming& timing) noexcept {
    pending_ = timing;
    has_pending_ = true;
}

void VideoTiming::apply_timing(const DisplayTiming& timing) noexcept {
    timing_ = timing;
    if (timing_.dot_divider == 0) {
        timing_.dot_divider = 10;
    }
    line_length_ = line_length_for(timing_.mode);
    lines_in_frame_ = lines_for(timing_.mode, timing_.interlaced, odd_field_);
}

void VideoTiming::schedule_next_edge(u32 after_x, bool inclusive) {
    u32 best = line_length_;
    Edge edge = Edge::LineStart;
    const auto consider = [&](u32 x, Edge kind) {
        const bool after = inclusive ? x >= after_x : x > after_x;
        if (after && x < line_length_ && x < best) {
            best = x;
            edge = kind;
        }
    };
    if (timing_.x1 != timing_.x2) {
        consider(timing_.x1, Edge::HblankEnd);
        consider(timing_.x2, Edge::HblankStart);
    }
    next_edge_ = edge;
    next_edge_v_ = line_start_v() + best;
    scheduler_.schedule(system::EventId::Video, clock_.to_cpu_ceil(next_edge_v_));
}

void VideoTiming::on_event(u64 time) {
    if (listener_) {
        listener_->sync_to(time);
    }
    switch (next_edge_) {
    case Edge::LineStart:
        ++line_index_;
        handle_line_start(time);
        schedule_next_edge(0, true);
        break;
    case Edge::HblankEnd: {
        const u32 x = static_cast<u32>(next_edge_v_ - line_start_v());
        hblank_ = false;
        if (listener_) listener_->on_hblank(false, time);
        schedule_next_edge(x, false);
        break;
    }
    case Edge::HblankStart: {
        const u32 x = static_cast<u32>(next_edge_v_ - line_start_v());
        hblank_ = true;
        ++hblank_starts_;
        if (listener_) listener_->on_hblank(true, time);
        schedule_next_edge(x, false);
        break;
    }
    }
}

void VideoTiming::handle_line_start(u64 time) {
    if (has_pending_) {
        // Nova temporização a partir deste HSYNC; o dotclock recomeça a contagem aqui.
        anchor_v_ = line_start_v();
        line_index_ = 0;
        has_pending_ = false;
        apply_timing(pending_);
    }

    ++vline_;
    if (vline_ >= lines_in_frame_) {
        vline_ = 0;
        ++frames_;
        if (timing_.interlaced) {
            odd_field_ = !odd_field_;
        }
        lines_in_frame_ = lines_for(timing_.mode, timing_.interlaced, odd_field_);
    }

    // Y1 termina e Y2 inicia o VBLANK; com Y1 = Y2 o término prevalece (sem IRQ0, medido).
    if (vline_ == timing_.y1) {
        if (vblank_) {
            vblank_ = false;
            if (listener_) listener_->on_vblank(false, time);
        }
    } else if (vline_ == timing_.y2) {
        if (!vblank_) {
            vblank_ = true;
            ++vblank_starts_;
            irq_.request(interrupts::Irq::VBlank);
            if (listener_) listener_->on_vblank(true, time);
        }
    }
}

u64 VideoTiming::dots(u64 cpu_time) const noexcept {
    const u64 v = clock_.from_cpu(cpu_time);
    if (v <= anchor_v_) {
        return 0;
    }
    const u64 rel = v - anchor_v_;
    return (rel / line_length_) * dots_per_line() + (rel % line_length_) / timing_.dot_divider;
}

u64 VideoTiming::time_after_dots(u64 from, u64 n) const noexcept {
    const u64 per_line = dots_per_line();
    if (n == 0 || per_line == 0) {
        return from;
    }
    const u64 target = dots(from) + n;
    const u64 line = target / per_line;
    const u64 k = target % per_line;
    const u64 v = k == 0 ? anchor_v_ + (line - 1) * line_length_ + per_line * timing_.dot_divider
                         : anchor_v_ + line * line_length_ + k * timing_.dot_divider;
    return std::max(from, clock_.to_cpu_ceil(v));
}

} // namespace rpcs1::gpu
