#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::Cpu;
using cpu::StepResult;

namespace {

struct HiLo {
    u32 hi;
    u32 lo;
};

HiLo run_op(u32 instruction, u32 a, u32 b) {
    Machine m({instruction, mfhi(t2), mflo(t3)});
    m.cpu.set_gpr(t0, a);
    m.cpu.set_gpr(t1, b);
    m.steps(3);
    return {m.reg(t2), m.reg(t3)};
}

} // namespace

TEST_CASE("cpu/MULT e MULTU produzem 64 bits em HI:LO") {
    struct Case {
        bool is_signed;
        u32 a;
        u32 b;
        u32 hi;
        u32 lo;
    };
    const Case cases[] = {
        {true, 3, 4, 0, 12},
        {true, static_cast<u32>(-3), 4, 0xFFFFFFFF, 0xFFFFFFF4},
        {false, static_cast<u32>(-3), 4, 0x00000003, 0xFFFFFFF4},
        {true, 0x80000000, 0x80000000, 0x40000000, 0},
        {false, 0x80000000, 0x80000000, 0x40000000, 0},
        {true, 0xFFFFFFFF, 0xFFFFFFFF, 0, 1},
        {false, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 1},
        {true, 0x7FFFFFFF, 0x80000000, 0xC0000000, 0x80000000},
    };
    for (const auto& c : cases) {
        const HiLo r = run_op(c.is_signed ? mult(t0, t1) : multu(t0, t1), c.a, c.b);
        CHECK_EQ(r.hi, c.hi);
        CHECK_EQ(r.lo, c.lo);
    }
}

TEST_CASE("cpu/DIV e DIVU: quociente em LO, resto em HI") {
    struct Case {
        bool is_signed;
        u32 a;
        u32 b;
        u32 hi;
        u32 lo;
    };
    const Case cases[] = {
        {true, 7, 2, 1, 3},
        {true, static_cast<u32>(-7), 2, static_cast<u32>(-1), static_cast<u32>(-3)},
        {true, 7, static_cast<u32>(-2), 1, static_cast<u32>(-3)},
        {false, 0xFFFFFFFF, 2, 1, 0x7FFFFFFF},
        {false, 100, 7, 2, 14},
    };
    for (const auto& c : cases) {
        const HiLo r = run_op(c.is_signed ? div(t0, t1) : divu(t0, t1), c.a, c.b);
        CHECK_EQ(r.hi, c.hi);
        CHECK_EQ(r.lo, c.lo);
    }
}

TEST_CASE("cpu/divisão por zero e 80000000h / -1 não geram exceção") {
    struct Case {
        bool is_signed;
        u32 a;
        u32 b;
        u32 hi;
        u32 lo;
    };
    const Case cases[] = {
        {true, 5, 0, 5, 0xFFFFFFFF},
        {true, 0, 0, 0, 0xFFFFFFFF},
        {true, static_cast<u32>(-5), 0, static_cast<u32>(-5), 1},
        {false, 5, 0, 5, 0xFFFFFFFF},
        {false, 0xFFFFFFFF, 0, 0xFFFFFFFF, 0xFFFFFFFF},
        {true, 0x80000000, 0xFFFFFFFF, 0, 0x80000000},
    };
    for (const auto& c : cases) {
        const HiLo r = run_op(c.is_signed ? div(t0, t1) : divu(t0, t1), c.a, c.b);
        CHECK_EQ(r.hi, c.hi);
        CHECK_EQ(r.lo, c.lo);
    }
}

TEST_CASE("cpu/MTHI e MTLO") {
    Machine m({mthi(t0), mtlo(t1), mfhi(t2), mflo(t3)});
    m.cpu.set_gpr(t0, 0xAAAA0000);
    m.cpu.set_gpr(t1, 0x0000BBBB);
    m.steps(4);
    CHECK_EQ(m.cpu.hi(), u32{0xAAAA0000});
    CHECK_EQ(m.cpu.lo(), u32{0x0000BBBB});
    CHECK_EQ(m.reg(t2), u32{0xAAAA0000});
    CHECK_EQ(m.reg(t3), u32{0x0000BBBB});
}

TEST_CASE("cpu/latência de MULT depende só de rs") {
    // MULTU: 6 (< 800h), 9 (< 100000h), 13 (demais).
    CHECK_EQ(Cpu::mult_latency(0x00000000, false), u32{6});
    CHECK_EQ(Cpu::mult_latency(0x000007FF, false), u32{6});
    CHECK_EQ(Cpu::mult_latency(0x00000800, false), u32{9});
    CHECK_EQ(Cpu::mult_latency(0x000FFFFF, false), u32{9});
    CHECK_EQ(Cpu::mult_latency(0x00100000, false), u32{13});
    CHECK_EQ(Cpu::mult_latency(0xFFFFFFFF, false), u32{13});
    // MULT: faixas simétricas para valores negativos.
    CHECK_EQ(Cpu::mult_latency(0xFFFFFFFF, true), u32{6});
    CHECK_EQ(Cpu::mult_latency(0xFFFFF800, true), u32{6});
    CHECK_EQ(Cpu::mult_latency(0xFFFFF7FF, true), u32{9});
    CHECK_EQ(Cpu::mult_latency(0xFFF00000, true), u32{9});
    CHECK_EQ(Cpu::mult_latency(0xFFEFFFFF, true), u32{13});
    CHECK_EQ(Cpu::mult_latency(0x7FFFFFFF, true), u32{13});
    CHECK_EQ(Cpu::mult_latency(0x000007FF, true), u32{6});
}

TEST_CASE("cpu/MFLO logo após MULT/DIV bloqueia pela latência") {
    struct Case {
        u32 instruction;
        u32 rs;
        u64 expected_cycles; // MULT/DIV (1) + bloqueio + MFLO (1)
    };
    const Case cases[] = {
        {multu(t0, t1), 0x10, 1 + 6 + 1},
        {multu(t0, t1), 0x800, 1 + 9 + 1},
        {multu(t0, t1), 0x100000, 1 + 13 + 1},
        {mult(t0, t1), static_cast<u32>(-1), 1 + 6 + 1},
        {div(t0, t1), 1000, 1 + 36 + 1},
        {divu(t0, t1), 1000, 1 + 36 + 1},
    };
    for (const auto& c : cases) {
        Machine m({c.instruction, mflo(t2)});
        m.cpu.set_gpr(t0, c.rs);
        m.cpu.set_gpr(t1, 3);
        m.steps(2);
        CHECK_EQ(m.cpu.cycles(), c.expected_cycles);
    }
}

TEST_CASE("cpu/instruções entre MULT e MFLO escondem a latência") {
    // psx-spx: até seis operações cabem antes do MFLO sem atraso (MULTU rápido).
    {
        Machine m({multu(t0, t1), nop(), nop(), nop(), nop(), nop(), nop(), mflo(t2)});
        m.cpu.set_gpr(t0, 2);
        m.steps(8);
        CHECK_EQ(m.cpu.cycles(), u64{8}); // nenhum bloqueio
    }
    {
        Machine m({multu(t0, t1), nop(), nop(), nop(), mfhi(t2)});
        m.cpu.set_gpr(t0, 2);
        m.steps(5);
        CHECK_EQ(m.cpu.cycles(), u64{1 + 3 + 3 + 1}); // bloqueio de 3 ciclos
    }
    {
        Machine m({multu(t0, t1), nop(), nop()});
        m.steps(3);
        CHECK_EQ(m.cpu.cycles(), u64{3}); // sem leitura de HI/LO, sem bloqueio
    }
}
