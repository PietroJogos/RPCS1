// Timers 0–2. Valores "medidos" vêm dos testes em SCPH-5501 do psx.wiki
// ("Root Counter Behavior", "Root Counter Mode Combinations", "Display Timing").
#include <vector>

#include "cpu/mips_assembler.hpp"
#include "framework/rpcs1_test.hpp"
#include "system/console.hpp"
#include "system/timing_rig.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using bus::AccessWidth;

namespace {

// Deslocamentos dentro de 1F801100h.
constexpr u32 count_of(u32 timer) { return timer * 0x10 + 0x0; }
constexpr u32 mode_of(u32 timer) { return timer * 0x10 + 0x4; }
constexpr u32 target_of(u32 timer) { return timer * 0x10 + 0x8; }

u64 irqs_in_140k(u32 mode, u32 target) {
    TimingRig rig;
    rig.write(target_of(2), target);
    rig.write(mode_of(2), mode);
    rig.advance_to(140'000);
    return rig.timers.irq_requests(2);
}

} // namespace

TEST_CASE("timers/valores de reset") {
    TimingRig rig;
    for (u32 i = 0; i < 3; ++i) {
        CHECK_EQ(rig.read(count_of(i)), u32{0});
        CHECK_EQ(rig.read(mode_of(i)), u32{0x0400}); // bit 10 = sem pedido
        CHECK_EQ(rig.read(target_of(i)), u32{0});
    }
}

TEST_CASE("timers/clock do sistema e prescaler /8") {
    {
        TimingRig rig;
        rig.write(mode_of(2), 0x0000);
        rig.advance_to(1000);
        CHECK_EQ(rig.read(count_of(2)), u32{999}); // 0 mantido por 2 ciclos após escrever MODE
    }
    {
        TimingRig rig;
        rig.write(mode_of(2), 0x0200); // sistema / 8
        rig.advance_to(1000);
        CHECK_EQ(rig.read(count_of(2)), u32{125});
        rig.advance_to(5000);
        CHECK_EQ(rig.read(count_of(2)), u32{625}); // medido: 626–627 com o custo da leitura
    }
}

TEST_CASE("timers/número de IRQs por modo em 140 000 ciclos (timer 2, alvo 0100h)") {
    CHECK_EQ(irqs_in_140k(0x0010, 0x100), u64{1});   // medido: 1 (uma vez)
    CHECK_EQ(irqs_in_140k(0x0050, 0x100), u64{3});   // medido: 3, ~65 500 ciclos de intervalo
    CHECK_EQ(irqs_in_140k(0x0058, 0x100), u64{542}); // medido: 542 (período alvo + 2)
    CHECK_EQ(irqs_in_140k(0x0020, 0x100), u64{1});   // medido: 1, em FFFFh
    CHECK_EQ(irqs_in_140k(0x00D8, 0x100), u64{271}); // alternância: a cada dois eventos
}

TEST_CASE("timers/reset no alvo: período alvo + 2 com 0 por dois ciclos") {
    TimingRig rig;
    rig.write(target_of(2), 3);
    rig.write(mode_of(2), 0x0008);
    std::vector<u32> seen;
    for (u64 t = 1; t <= 21; ++t) {
        rig.advance_to(t);
        seen.push_back(rig.read(count_of(2)));
    }
    // 0 (retido pela escrita), 1, 2, 3, 0, 0, 1, 2, 3, 0, 0, ...
    const std::vector<u32> expected = {0, 1, 2, 3, 0, 0, 1, 2, 3, 0, 0, 1, 2, 3, 0, 0, 1, 2, 3, 0, 0};
    CHECK(seen == expected);
}

TEST_CASE("timers/alvo 0 com reset: preso em 0, flag ligado e sem IRQ") {
    TimingRig rig;
    rig.write(target_of(2), 0);
    rig.write(mode_of(2), 0x0058);
    rig.advance_to(100'000);
    CHECK_EQ(rig.read(count_of(2)), u32{0});
    CHECK((rig.timers.peek_mode(2, rig.now) & (1u << 11)) != 0);
    CHECK_EQ(rig.timers.irq_requests(2), u64{0});
}

TEST_CASE("timers/IRQ só em FFFFh com reset em alvo menor nunca dispara") {
    TimingRig rig;
    rig.write(target_of(2), 0x100);
    rig.write(mode_of(2), 0x0068);
    rig.advance_to(300'000);
    CHECK_EQ(rig.timers.irq_requests(2), u64{0});
}

TEST_CASE("timers/flags 11 e 12: quais leituras os limpam") {
    TimingRig rig;
    rig.write(target_of(2), 10);
    rig.write(mode_of(2), 0x0000);
    rig.advance_to(70'000); // passou do alvo e de FFFFh
    const u32 flags = (1u << 11) | (1u << 12);
    CHECK_EQ(rig.timers.peek_mode(2, rig.now) & flags, flags);
    (void)rig.read(count_of(2));
    (void)rig.read(target_of(2));
    (void)rig.read(mode_of(2), AccessWidth::Byte); // byte baixo: não limpa
    CHECK_EQ(rig.timers.peek_mode(2, rig.now) & flags, flags);
    CHECK_EQ(rig.read(mode_of(2) + 1, AccessWidth::Byte), u32{0x1C}); // byte alto: lê e limpa
    CHECK_EQ(rig.timers.peek_mode(2, rig.now) & flags, u32{0});

    TimingRig half;
    half.write(target_of(2), 10);
    half.write(mode_of(2), 0x0000);
    half.advance_to(20);
    CHECK_EQ(half.read(mode_of(2), AccessWidth::Half), u32{0x0C00});
    CHECK_EQ(half.timers.peek_mode(2, half.now) & flags, u32{0});
}

TEST_CASE("timers/escrita de MODE: zera o contador, liga o bit 10 e ignora os bits 10–12") {
    TimingRig rig;
    rig.write(mode_of(2), 0x0000);
    rig.advance_to(500);
    rig.write(mode_of(2), 0x1C48); // bits 10–12 não são graváveis
    CHECK_EQ(rig.read(count_of(2)), u32{0});
    CHECK_EQ(rig.timers.peek_mode(2, rig.now), u32{0x0448});
}

TEST_CASE("timers/escrever o próprio alvo no contador liga o flag 11") {
    TimingRig rig;
    rig.write(target_of(2), 500);
    rig.write(mode_of(2), 0x0001); // parado (sincronização 0 do timer 2)
    rig.write(count_of(2), 500);
    CHECK_EQ(rig.timers.peek_mode(2, rig.now), u32{0x0C01}); // medido: 0C01h
    rig.write(count_of(2), 501);
    rig.advance_to(1000);
    CHECK_EQ(rig.read(count_of(2)), u32{501}); // parado
}

TEST_CASE("timers/modo uma vez: rearmado por escrita no contador") {
    TimingRig rig;
    rig.write(target_of(2), 100);
    rig.write(mode_of(2), 0x0010);
    rig.advance_to(70'000); // dispara no alvo e não repete após dar a volta
    CHECK_EQ(rig.timers.irq_requests(2), u64{1});
    rig.write(count_of(2), 0);
    rig.advance_to(70'200);
    CHECK_EQ(rig.timers.irq_requests(2), u64{2});
}

TEST_CASE("timers/modo pulso: bit 10 cai brevemente; IRQ chega ao I_STAT") {
    TimingRig rig;
    rig.write(target_of(1), 100);
    rig.write(mode_of(1), 0x0050);
    rig.advance_to(101); // 1 ciclo retido + 100
    CHECK_EQ(rig.timers.irq_requests(1), u64{1});
    CHECK_EQ(rig.timers.peek_mode(1, rig.now) & (1u << 10), u32{0});
    rig.advance_to(104);
    CHECK((rig.timers.peek_mode(1, rig.now) & (1u << 10)) != 0);
    CHECK((rig.irq.status() & (1u << 5)) != 0); // IRQ5 = timer 1
}

TEST_CASE("timers/modo alternância uma vez: bit 10 permanece em 0") {
    TimingRig rig;
    rig.write(target_of(0), 50);
    rig.write(mode_of(0), 0x0098); // alternância, uma vez, IRQ no alvo, reset
    rig.advance_to(10'000);
    CHECK_EQ(rig.timers.irq_requests(0), u64{1});
    CHECK_EQ(rig.timers.peek_mode(0, rig.now) & (1u << 10), u32{0});
    CHECK((rig.irq.status() & (1u << 4)) != 0); // IRQ4 = timer 0
}

TEST_CASE("timers/timer 0 com dotclock acompanha o gerador de vídeo") {
    TimingRig rig;
    rig.write(mode_of(0), 0x0100);
    for (u64 t : {u64{2'153}, u64{100'000}, u64{566'204}}) {
        rig.advance_to(t);
        CHECK_EQ(rig.read(count_of(0)), static_cast<u32>(rig.video.dots(t) & 0xFFFF));
    }
}

TEST_CASE("timers/timer 1 com HBLANK conta 263 linhas por quadro") {
    TimingRig rig;
    rig.write(mode_of(1), 0x0100);
    rig.advance_to(3 * 566'204);
    REQUIRE(rig.vblank_irq_times.size() >= 2);
    // Contagem entre dois IRQ0 = 263 (medido).
    TimingRig r2;
    r2.write(mode_of(1), 0x0100);
    r2.advance_to(rig.vblank_irq_times[0]);
    const u32 a = r2.read(count_of(1));
    r2.advance_to(rig.vblank_irq_times[1]);
    CHECK_EQ(r2.read(count_of(1)) - a, u32{263});
}

TEST_CASE("timers/timer 1 HBLANK pausado no VBLANK conta as linhas visíveis") {
    TimingRig rig;
    rig.write(mode_of(1), 0x0101); // HBLANK, sincronização 0 (pausa no VBLANK)
    rig.advance_to(3 * 566'204);
    REQUIRE(rig.vblank_irq_times.size() >= 3);
    TimingRig r2;
    r2.write(mode_of(1), 0x0101);
    r2.advance_to(rig.vblank_irq_times[1]);
    const u32 a = r2.read(count_of(1));
    r2.advance_to(rig.vblank_irq_times[2]);
    // Modelo: 240 linhas (Y2 − Y1). ⚠️ Medido: 239 — diferença de uma linha (D-033).
    CHECK_EQ(r2.read(count_of(1)) - a, u32{240});
}

TEST_CASE("timers/timer 0 sincronizado ao HBLANK (X = 600..3160, 320 pontos)") {
    gpu::DisplayTiming t;
    t.dot_divider = 8;
    t.x1 = 600;
    t.x2 = 3160;
    {
        // Modo 0: pausa no HBLANK → conta a parte visível de cada linha (medido 1613–1614).
        TimingRig rig;
        rig.video.set_display_timing(t);
        rig.advance_to(5'000);
        rig.write(mode_of(0), 0x0001);
        rig.advance_to_next_hblank_start();
        const u32 a = rig.read(count_of(0));
        for (int line = 0; line < 10; ++line) {
            rig.advance_to_next_hblank_start(); // 10 linhas completas
        }
        const u32 per_ten = rig.read(count_of(0)) - a;
        CHECK(per_ten >= 16'140 && per_ten <= 16'150); // 10 × 1614,8
    }
    {
        // Modo 1: zera no fim do HBLANK → no início do HBLANK vale a parte visível
        // (medido 1623–1634, incluindo o custo das instruções de leitura).
        TimingRig rig;
        rig.video.set_display_timing(t);
        rig.advance_to(5'000);
        rig.write(mode_of(0), 0x0003);
        rig.advance_to_next_hblank_start();
        rig.advance_to_next_hblank_start();
        const u32 v = rig.read(count_of(0));
        CHECK(v >= 1'612 && v <= 1'617);
    }
    {
        // Modo 2: zera no fim e conta só no HBLANK → 0 no início do HBLANK (medido 9–21).
        TimingRig rig;
        rig.video.set_display_timing(t);
        rig.advance_to(5'000);
        rig.write(mode_of(0), 0x0005);
        rig.advance_to_next_hblank_start();
        rig.advance_to_next_hblank_start();
        CHECK_EQ(rig.read(count_of(0)), u32{0});
    }
}

TEST_CASE("timers/timer 2: sincronização 0 e 3 param, 1 e 2 contam") {
    for (u32 sm = 0; sm < 4; ++sm) {
        TimingRig rig;
        rig.write(mode_of(2), 0x0001 | (sm << 1));
        rig.advance_to(1000);
        const u32 value = rig.read(count_of(2));
        if (sm == 0 || sm == 3) {
            CHECK_EQ(value, u32{0});
        } else {
            CHECK_EQ(value, u32{999});
        }
    }
}

TEST_CASE("timers/sincronização 3 espera o fim do blank e depois conta livremente") {
    TimingRig rig;
    rig.advance_to(600'000); // dentro do VBLANK do primeiro quadro
    REQUIRE(rig.video.in_vblank());
    rig.write(mode_of(1), 0x0107); // HBLANK, sincronização 3
    rig.advance_to(rig.now + 300); // o VBLANK termina em ≈600 650
    CHECK_EQ(rig.read(count_of(1)), u32{0}); // ainda esperando
    rig.advance_to(rig.vblank_irq_times.back() + 566'204 - 1'000);
    CHECK(rig.read(count_of(1)) >= 239); // contando desde o fim do VBLANK
}

TEST_CASE("timers/acessados pela CPU no Console com tempo coerente") {
    using namespace rpcs1::test::asm_;
    system::Console c;
    std::vector<u32> program = {lui(t0, 0x1F80), sw(zero, 0x1124, t0)}; // T2 MODE = 0
    for (int i = 0; i < 98; ++i) program.push_back(nop());
    program.push_back(lw(t1, 0x1120, t0)); // T2 COUNT
    program.push_back(nop());
    for (usize i = 0; i < program.size(); ++i) {
        REQUIRE_EQ(c.bus().write32(0x80010000u + static_cast<u32>(i * 4), program[i]),
                   bus::BusFault::None);
    }
    c.cpu().set_pc(0x80010000);
    c.run(program.size());
    // MODE escrito no ciclo 1; leitura no ciclo 100: 99 ciclos, 1 retido.
    CHECK_EQ(c.cpu().gpr(t1), u32{98});
}

TEST_CASE("console/VBLANK chega à CPU: laço que conta quadros por I_STAT") {
    using namespace rpcs1::test::asm_;
    system::Console c;
    const std::vector<u32> program = {
        lui(t0, 0x1F80),
        addu(s0, zero, zero),
        lw(t1, 0x1070, t0), // laço: I_STAT
        nop(),
        andi(t1, t1, 1),
        beq(t1, zero, -4),
        nop(),
        addiu(t2, zero, -2),
        sw(t2, 0x1070, t0), // reconhece o IRQ0
        j(0x80010008),
        addiu(s0, s0, 1),
    };
    for (usize i = 0; i < program.size(); ++i) {
        REQUIRE_EQ(c.bus().write32(0x80010000u + static_cast<u32>(i * 4), program[i]),
                   bus::BusFault::None);
    }
    c.cpu().set_pc(0x80010000);
    c.run(1'700'000); // ~3 quadros de 566 204 ciclos (1 ciclo por instrução)
    CHECK_EQ(c.video().vblank_starts(), u64{3});
    CHECK_EQ(c.cpu().gpr(s0), u32{3});
}
