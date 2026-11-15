// Programas completos que combinam laços, delay slots, load delay e sub-rotinas.
#include <vector>

#include "cpu/disassembler.hpp"
#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::ExceptionCode;
using cpu::StepResult;

namespace {
constexpr u32 base = Machine::code_base;
}

TEST_CASE("cpu/programa: soma de 1 a 100") {
    Machine m({
        addiu(t0, zero, 100), // contador
        addu(t1, zero, zero), // soma
        addu(t1, t1, t0),     // laço:
        addiu(t0, t0, -1),
        bne(t0, zero, -3),    // volta ao laço
        nop(),
        break_(),
    });
    CHECK(m.run_until_stop() == StepResult::Exception);
    CHECK(m.cpu.last_exception()->code == ExceptionCode::Breakpoint);
    CHECK_EQ(m.reg(t1), u32{5050});
    CHECK_EQ(m.cpu.instructions(), u64{2 + 100 * 4});
}

TEST_CASE("cpu/programa: cópia de bytes usando o load delay slot") {
    constexpr u32 src = 0x80003000;
    constexpr u32 dst = 0x80004000;
    Machine m({
        lui(t0, src >> 16), ori(t0, t0, src & 0xFFFF),
        lui(t1, dst >> 16), ori(t1, t1, dst & 0xFFFF),
        addiu(t2, zero, 16),
        lbu(t3, 0, t0),     // laço:
        addiu(t0, t0, 1),   // load delay slot (não usa t3)
        sb(t3, 0, t1),      // t3 já disponível
        addiu(t2, t2, -1),
        bne(t2, zero, -5),
        addiu(t1, t1, 1),   // delay slot do desvio
        break_(),
    });
    for (u32 i = 0; i < 16; i += 4) {
        m.poke32(src + i, 0x03020100u + 0x04040404u * (i / 4));
    }
    CHECK(m.run_until_stop() == StepResult::Exception);
    for (u32 i = 0; i < 16; ++i) {
        CHECK_EQ(m.bus.read8(dst + i).value, i);
    }
    CHECK_EQ(m.bus.read8(dst + 16).value, u32{0}); // não passou do limite
}

TEST_CASE("cpu/programa: sub-rotina com JAL e JR") {
    const u32 sub = base + 0x100;
    Machine m({
        addiu(a0, zero, 20),
        jal(sub),
        addiu(a1, zero, 22), // delay slot: argumento
        addu(s0, v0, zero),
        break_(),
    });
    m.load({addu(v0, a0, a1), jr(ra), nop()}, sub);
    CHECK(m.run_until_stop() == StepResult::Exception);
    CHECK_EQ(m.reg(s0), u32{42});
    CHECK_EQ(m.reg(ra), base + 12);
}

TEST_CASE("cpu/programa: fatorial com MULTU e leitura de LO") {
    Machine m({
        addiu(t0, zero, 10), // n
        addiu(t1, zero, 1),  // resultado
        multu(t1, t0),       // laço:
        mflo(t1),
        addiu(t0, t0, -1),
        bgtz(t0, -4),
        nop(),
        break_(),
    });
    CHECK(m.run_until_stop() == StepResult::Exception);
    CHECK_EQ(m.reg(t1), u32{3628800});
}

TEST_CASE("disassembler/formatos principais") {
    CHECK_EQ(cpu::disassemble(nop(), base), std::string("nop"));
    CHECK_EQ(cpu::disassemble(addiu(t0, zero, 5), base), std::string("addiu t0, zero, 0x5"));
    CHECK_EQ(cpu::disassemble(addiu(t0, zero, -1), base), std::string("addiu t0, zero, -0x1"));
    CHECK_EQ(cpu::disassemble(lw(t1, 16, sp), base), std::string("lw t1, 0x10(sp)"));
    CHECK_EQ(cpu::disassemble(sb(a0, -2, gp), base), std::string("sb a0, -0x2(gp)"));
    CHECK_EQ(cpu::disassemble(beq(zero, zero, 2), base), std::string("beq zero, zero, 0x8001000C"));
    CHECK_EQ(cpu::disassemble(bgezal(ra, -1), base), std::string("bgezal ra, 0x80010000"));
    CHECK_EQ(cpu::disassemble(jal(0x80010100), base), std::string("jal 0x80010100"));
    CHECK_EQ(cpu::disassemble(jr(ra), base), std::string("jr ra"));
    CHECK_EQ(cpu::disassemble(addu(v0, a0, a1), base), std::string("addu v0, a0, a1"));
    CHECK_EQ(cpu::disassemble(sll(t0, t1, 4), base), std::string("sll t0, t1, 4"));
    CHECK_EQ(cpu::disassemble(lui(t0, 0x1F80), base), std::string("lui t0, 0x1F80"));
    CHECK_EQ(cpu::disassemble(syscall(1), base), std::string("syscall 0x1"));
    CHECK_EQ(cpu::disassemble(0xFC000000u, base), std::string("(inválida 3Fh)"));
    CHECK_EQ(cpu::disassemble(0x00000001u, base), std::string("(inválida SPECIAL 01h)"));
    CHECK_EQ(cpu::register_name(29), std::string_view("sp"));
    CHECK_EQ(cpu::disassemble(mfc0(t3, 12), base), std::string("mfc0 t3, $12"));
    CHECK_EQ(cpu::disassemble(0x40806000u, base), std::string("mtc0 zero, $12"));
    CHECK_EQ(cpu::disassemble(rfe(), base), std::string("rfe"));
    CHECK_EQ(cpu::disassemble(0x4A180001u, base), std::string("cop2 0x0180001"));
}
