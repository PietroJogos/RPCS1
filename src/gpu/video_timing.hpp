// Gerador de temporização de vídeo da GPU (Fase 5): HSYNC, HBLANK, VBLANK, IRQ0 e dotclock.
//
// Fontes: psx-spx "GPU Timings"/"GP1"; medições em SCPH-5501 publicadas no psx.wiki
// ("Display Timing and GPUSTAT", "Root Counter Behavior").
//   - Ciclos de vídeo por linha: 3413 (modo NTSC) / 3406 (modo PAL); o cristal depende da
//     região do console (53,693175 / 53,203425 MHz), não do modo programado.
//   - Linhas por quadro: 263 / 314; entrelaçado alterna 262/263 e 312/313.
//   - HBLANK = parte da linha fora do intervalo X1..X2 de GP1(06h) (em ciclos de vídeo a partir
//     do HSYNC). Intervalo vazio (X1 = X2) = HBLANK permanente, sem bordas.
//   - VBLANK começa na linha Y2 e termina na linha Y1 de GP1(07h); o IRQ0 ocorre no início do
//     VBLANK. O período do IRQ0 não depende de Y1/Y2; com Y2 fora do quadro ou Y1 = Y2 não há IRQ0.
//   - Dotclock = clock de vídeo / divisor (10, 8, 7, 5, 4); a contagem reinicia a cada linha
//     (pontos por linha = parte inteira: 341, 426, 487, 682, 853 no NTSC).
// ⚠️ A posição exata dos eventos dentro da linha (ordem HSYNC × bordas, fase do dotclock) não é
// documentada; ver docs/DECISIONS.md, D-031.
#pragma once

#include "core/console_model.hpp"
#include "core/types.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/clock.hpp"
#include "system/scheduler.hpp"

namespace rpcs1::gpu {

enum class VideoMode : u8 { Ntsc, Pal }; // GP1(08h) bit 3

struct DisplayTiming {
    VideoMode mode = VideoMode::Ntsc;
    bool interlaced = false;
    u32 dot_divider = 10;              // 256 pontos (GP1(08h) = 0)
    u32 x1 = 0x200;                    // GP1(06h) após GP1(00h)
    u32 x2 = 0x200 + 256 * 10;
    u32 y1 = 0x010;                    // GP1(07h) após GP1(00h)
    u32 y2 = 0x010 + 240;
    bool operator==(const DisplayTiming&) const = default;
};

// Recebe as bordas de HBLANK/VBLANK (implementado pelos timers).
class BlankListener {
public:
    virtual ~BlankListener() = default;
    // Chamado antes de cada borda, para o ouvinte atualizar seu estado até `time`.
    virtual void sync_to(u64 time) = 0;
    virtual void on_hblank(bool start, u64 time) = 0;
    virtual void on_vblank(bool start, u64 time) = 0;
};

class VideoTiming {
public:
    VideoTiming(system::Scheduler& scheduler, interrupts::InterruptController& irq);

    // Reinicia no HSYNC da linha 0 (VSYNC) com os valores de GP1(00h).
    void reset(Region console_region, u64 now);
    void set_listener(BlankListener* listener) noexcept { listener_ = listener; }

    // Novos parâmetros (GPU, Fase 7): aplicados no próximo HSYNC.
    void set_display_timing(const DisplayTiming& timing) noexcept;
    [[nodiscard]] const DisplayTiming& display_timing() const noexcept { return timing_; }

    void on_event(u64 time);

    [[nodiscard]] u64 video_cycles(u64 cpu_time) const noexcept { return clock_.from_cpu(cpu_time); }
    [[nodiscard]] u64 video_clock_hz() const noexcept { return clock_.rate(); }
    [[nodiscard]] u32 line_length() const noexcept { return line_length_; }
    [[nodiscard]] u32 lines_in_frame() const noexcept { return lines_in_frame_; }
    [[nodiscard]] u32 dots_per_line() const noexcept { return (line_length_ - 1) / timing_.dot_divider; }

    [[nodiscard]] u32 vertical_line() const noexcept { return vline_; }
    [[nodiscard]] bool in_hblank() const noexcept { return hblank_; }
    [[nodiscard]] bool in_vblank() const noexcept { return vblank_; }
    [[nodiscard]] bool odd_field() const noexcept { return odd_field_; }
    [[nodiscard]] u64 frames() const noexcept { return frames_; }
    [[nodiscard]] u64 hblank_starts() const noexcept { return hblank_starts_; }
    [[nodiscard]] u64 vblank_starts() const noexcept { return vblank_starts_; }

    // Contagem do dotclock até `cpu_time` (desde a última âncora) e o primeiro instante em que
    // ela avança `n` pontos a partir de `from`.
    [[nodiscard]] u64 dots(u64 cpu_time) const noexcept;
    [[nodiscard]] u64 time_after_dots(u64 from, u64 n) const noexcept;

private:
    enum class Edge : u8 { LineStart, HblankEnd, HblankStart };

    void apply_timing(const DisplayTiming& timing) noexcept;
    void schedule_next_edge(u32 after_x, bool inclusive);
    void handle_line_start(u64 time);
    [[nodiscard]] u64 line_start_v() const noexcept { return anchor_v_ + line_index_ * line_length_; }

    system::Scheduler& scheduler_;
    interrupts::InterruptController& irq_;
    BlankListener* listener_ = nullptr;
    system::ClockRatio clock_{system::video_clock_ntsc_hz};

    DisplayTiming timing_{};
    DisplayTiming pending_{};
    bool has_pending_ = false;

    u32 line_length_ = 3413;
    u32 lines_in_frame_ = 263;

    u64 anchor_v_ = 0;   // ciclo de vídeo em que começou a linha de índice 0 da âncora
    u64 line_index_ = 0; // linhas desde a âncora (a atual está em andamento)
    u32 vline_ = 0;      // linha vertical no quadro (0 = VSYNC)
    bool hblank_ = true;
    bool vblank_ = true;
    bool odd_field_ = false;
    u64 frames_ = 0;
    u64 hblank_starts_ = 0;
    u64 vblank_starts_ = 0;

    Edge next_edge_ = Edge::LineStart;
    u64 next_edge_v_ = 0;
};

} // namespace rpcs1::gpu
