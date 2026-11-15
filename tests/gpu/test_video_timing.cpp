// Temporização de vídeo. Valores "medidos" vêm dos testes em SCPH-5501 do psx.wiki
// ("Display Timing and GPUSTAT"); a tolerância de ±200 ciclos é a informada na página.
#include <cstdlib>
#include <vector>

#include "framework/rpcs1_test.hpp"
#include "system/timing_rig.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using gpu::DisplayTiming;
using gpu::VideoMode;

namespace {

std::vector<u64> frame_periods(TimingRig& rig, usize count) {
    std::vector<u64> periods;
    const auto& t = rig.vblank_irq_times;
    for (usize i = 1; i < t.size() && periods.size() < count; ++i) {
        periods.push_back(t[i] - t[i - 1]);
    }
    return periods;
}

bool within(u64 value, u64 reference, u64 tolerance) {
    return (value > reference ? value - reference : reference - value) <= tolerance;
}

} // namespace

TEST_CASE("video/quadro NTSC: 263 linhas × 3413 ciclos, como medido no SCPH-5501") {
    TimingRig rig;
    rig.advance_to(6 * 566'204);
    const auto periods = frame_periods(rig, 4);
    REQUIRE_EQ(periods.size(), usize{4});
    for (u64 p : periods) {
        CHECK(p == 566'203 || p == 566'204); // 897 619 ciclos de vídeo
        CHECK(within(p, 566'140, 200));      // medido: ≈566 140 (±200)
    }
    CHECK_EQ(rig.video.lines_in_frame(), u32{263});
    CHECK_EQ(rig.video.line_length(), u32{3413});
}

TEST_CASE("video/HBLANKs e VBLANK por quadro com os valores de GP1(00h)") {
    TimingRig rig;
    rig.advance_to(2 * 566'204);
    REQUIRE(rig.vblank_irq_times.size() >= 2);
    // HBLANKs entre dois IRQ0 consecutivos (mesma linha do tempo, determinística).
    TimingRig counter;
    counter.advance_to(rig.vblank_irq_times[0]);
    const u64 h0 = counter.video.hblank_starts();
    counter.advance_to(rig.vblank_irq_times[1]);
    CHECK_EQ(counter.video.hblank_starts() - h0, u64{263}); // medido: 263

    // VBLANK = linhas fora de [Y1, Y2) = [16, 256): 23 linhas (medido via GPUSTAT bit 31).
    // Amostra no meio de cada linha a partir do início do VBLANK (≈2152,87 ciclos/linha).
    u32 vblank_lines = 0;
    const u64 start = rig.vblank_irq_times[1];
    for (u64 i = 0; i < 263; ++i) {
        rig.advance_to(start + (i * 2'152'870 + 1'076'435) / 1000);
        if (rig.video.in_vblank()) ++vblank_lines;
    }
    CHECK_EQ(vblank_lines, u32{23});
}

TEST_CASE("video/modo PAL num console NTSC e console PAL") {
    {
        TimingRig rig; // bit PAL ligado num console NTSC (como na medição)
        DisplayTiming t;
        t.mode = VideoMode::Pal;
        rig.video.set_display_timing(t);
        rig.advance_to(6 * 680'000);
        const auto periods = frame_periods(rig, 3);
        REQUIRE_EQ(periods.size(), usize{3});
        for (u64 p : periods) {
            CHECK(p == 674'613 || p == 674'614); // 314 × 3406 = 1 069 484 ciclos de vídeo (NTSC)
            CHECK(within(p, 674'450, 200));      // medido: ≈674 450
        }
        CHECK_EQ(rig.video.lines_in_frame(), u32{314});
    }
    {
        TimingRig rig(Region::Pal); // console PAL: cristal 53,203425 MHz
        DisplayTiming t;
        t.mode = VideoMode::Pal;
        rig.video.set_display_timing(t);
        rig.advance_to(6 * 690'000);
        const auto periods = frame_periods(rig, 3);
        REQUIRE_EQ(periods.size(), usize{3});
        for (u64 p : periods) {
            const double hz = static_cast<double>(system::cpu_clock_hz) / static_cast<double>(p);
            // Modelo: 53 203 425 / (314 × 3406) = 49,747 Hz. ⚠️ O psx-spx cita 49,761 Hz, valor
            // que não fecha com o próprio cristal e ciclos que ele publica; sem medição PAL real.
            CHECK(hz > 49.74 && hz < 49.77);
        }
    }
}

TEST_CASE("video/entrelaçado alterna campos de 263 e 262 linhas") {
    TimingRig rig;
    DisplayTiming t;
    t.interlaced = true;
    rig.video.set_display_timing(t);
    rig.advance_to(8 * 566'204);
    const auto periods = frame_periods(rig, 6);
    REQUIRE_EQ(periods.size(), usize{6});
    u32 long_fields = 0;
    u32 short_fields = 0;
    for (u64 p : periods) {
        if (within(p, 566'150, 200)) ++long_fields;  // medido: ≈566 150
        if (within(p, 563'930, 200)) ++short_fields; // medido: ≈563 930
    }
    CHECK_EQ(long_fields, u32{3});
    CHECK_EQ(short_fields, u32{3});
}

TEST_CASE("video/IRQ0 conforme Y1/Y2 de GP1(07h) (casos medidos)") {
    struct Case {
        u32 y1;
        u32 y2;
        bool irq;
    };
    const Case cases[] = {
        {16, 256, true}, {40, 200, true}, {100, 110, true}, {8, 262, true},
        {200, 100, true},  // Y2 < Y1: IRQ0 normal
        {16, 300, false},  // Y2 fora do quadro
        {16, 1023, false},
        {100, 100, false}, // Y1 = Y2
        {0, 263, false},
    };
    for (const auto& c : cases) {
        TimingRig rig;
        DisplayTiming t;
        t.y1 = c.y1;
        t.y2 = c.y2;
        rig.video.set_display_timing(t);
        rig.advance_to(4 * 566'204);
        CHECK_EQ(!rig.vblank_irq_times.empty(), c.irq);
        if (c.irq) {
            const auto periods = frame_periods(rig, 2);
            for (u64 p : periods) {
                CHECK(p == 566'203 || p == 566'204); // período não depende de Y1/Y2
            }
        }
    }
}

TEST_CASE("video/HBLANK fora do intervalo X; intervalos inválidos não geram bordas") {
    struct Case {
        u32 x1;
        u32 x2;
        u64 starts_per_frame;
    };
    const Case cases[] = {
        {600, 3160, 263}, {608, 1888, 263}, {200, 3300, 263},
        {1000, 1000, 0},  // largura zero: HBLANK permanente
        {0x200, 4095, 0}, // X2 além da linha
        {0, 4095, 0},
    };
    for (const auto& c : cases) {
        TimingRig rig;
        DisplayTiming t;
        t.x1 = c.x1;
        t.x2 = c.x2;
        rig.video.set_display_timing(t);
        rig.advance_to(3000); // aplica no próximo HSYNC
        const u64 h0 = rig.video.hblank_starts();
        rig.advance_to(3000 + 263 * 2'153);
        const u64 starts = rig.video.hblank_starts() - h0;
        CHECK(starts >= c.starts_per_frame && starts <= c.starts_per_frame + 1);
    }
}

TEST_CASE("video/dotclock: pontos por linha medidos para cada largura") {
    struct Case {
        u32 divider;
        u32 dots;
    };
    const Case cases[] = {{10, 341}, {8, 426}, {7, 487}, {5, 682}, {4, 853}};
    for (const auto& c : cases) {
        TimingRig rig;
        DisplayTiming t;
        t.dot_divider = c.divider;
        rig.video.set_display_timing(t);
        rig.advance_to(2'200); // após o primeiro HSYNC a nova temporização vale
        CHECK_EQ(rig.video.dots_per_line(), c.dots);
        const u64 a = rig.now;
        const u64 d0 = rig.video.dots(a);
        // Um quadro (263 linhas) contém 263 × pontos-por-linha (± 1 pela fase dentro da linha).
        const u64 frame_dots = rig.video.dots(a + 566'204) - d0;
        CHECK(frame_dots + 1 >= 263ull * c.dots && frame_dots <= 263ull * c.dots + 1);
        // time_after_dots devolve o primeiro instante em que a contagem avança n pontos.
        for (u64 n : {u64{1}, u64{100}, u64{c.dots}, u64{5000}}) {
            const u64 when = rig.video.time_after_dots(a, n);
            CHECK(rig.video.dots(when) >= d0 + n);
            CHECK(rig.video.dots(when - 1) < d0 + n);
        }
    }
}

TEST_CASE("video/pontos por quadro conferem com a medição (256 e 320 pontos)") {
    struct Case {
        u32 divider;
        u64 measured; // psx.wiki, "Display Timing", Tabela 1
    };
    for (const auto& c : {Case{10, 89'687}, Case{8, 112'043}}) {
        TimingRig rig;
        DisplayTiming t;
        t.dot_divider = c.divider;
        rig.video.set_display_timing(t);
        rig.advance_to(3 * 566'204);
        REQUIRE(rig.vblank_irq_times.size() >= 2);
        const u64 dots = rig.video.dots(rig.vblank_irq_times[1]) - rig.video.dots(rig.vblank_irq_times[0]);
        CHECK(within(dots, c.measured, 10));
    }
}
