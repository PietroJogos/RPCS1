// Base de tempo do console emulado (Fase 5).
//
// Todo o sistema conta tempo em ciclos da CPU (relógio do sistema, 33,8688 MHz =
// 44 100 × 768). O vídeo usa um cristal próprio por região (psx-spx, "GPU Timings"):
//   NTSC 53,693175 MHz, PAL 53,203425 MHz.
// Medição em um SCPH-5501 (psx.wiki, "Display Timing"): IRQ0 a cada ≈566 140 ciclos da CPU,
// o que confere com 263 linhas × 3413 ciclos de vídeo no cristal NTSC (e não com CPU × 11/7).
// As conversões são inteiras e exatas: não acumulam erro ao longo do tempo.
#pragma once

#include "core/types.hpp"

namespace rpcs1::system {

inline constexpr u64 cpu_clock_hz = 33'868'800;
inline constexpr u32 cpu_cycles_per_audio_sample = 768; // 44,1 kHz (SPU, Fase 11)

inline constexpr u64 video_clock_ntsc_hz = 53'693'175;
inline constexpr u64 video_clock_pal_hz = 53'203'425;

// Converte entre ciclos da CPU e ciclos de um relógio de `rate_hz`, sem estouro em 64 bits
// para qualquer duração realista (as multiplicações intermediárias ficam abaixo de 2^52).
class ClockRatio {
public:
    explicit constexpr ClockRatio(u64 rate_hz) noexcept : rate_(rate_hz) {}

    [[nodiscard]] constexpr u64 rate() const noexcept { return rate_; }

    // floor(cpu × rate / cpu_clock)
    [[nodiscard]] constexpr u64 from_cpu(u64 cpu_cycles) const noexcept {
        const u64 q = cpu_cycles / cpu_clock_hz;
        const u64 r = cpu_cycles % cpu_clock_hz;
        return q * rate_ + (r * rate_) / cpu_clock_hz;
    }

    // Menor ciclo da CPU t com from_cpu(t) >= cycles.
    [[nodiscard]] constexpr u64 to_cpu_ceil(u64 cycles) const noexcept {
        const u64 q = cycles / rate_;
        const u64 r = cycles % rate_;
        return q * cpu_clock_hz + (r * cpu_clock_hz + rate_ - 1) / rate_;
    }

private:
    u64 rate_;
};

} // namespace rpcs1::system
