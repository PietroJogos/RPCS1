#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::ExceptionCode;
using cpu::StepResult;

namespace {
constexpr u32 base = Machine::code_base;
}

TEST_CASE("cpu/BEQ tomado executa o delay slot e salta") {
    Machine m({beq(zero, zero, 2), addiu(t0, zero, 1), addiu(t1, zero, 1), addiu(t2, zero, 1)});
    m.steps(3);
    CHECK_EQ(m.reg(t0), u32{1}); // delay slot
    CHECK_EQ(m.reg(t1), u32{0}); // pulada
    CHECK_EQ(m.reg(t2), u32{1}); // alvo = base + 4 + 2*4
    CHECK_EQ(m.cpu.pc(), base + 16);
}

TEST_CASE("cpu/BNE não tomado também executa o delay slot") {
    Machine m({bne(zero, zero, 2), addiu(t0, zero, 1), addiu(t1, zero, 1)});
    m.steps(3);
    CHECK_EQ(m.reg(t0), u32{1});
    CHECK_EQ(m.reg(t1), u32{1});
}

TEST_CASE("cpu/desvio para trás (offset negativo)") {
    // 0: addiu t0,t0,1 ; 1: bne t0,t1,-2 (volta para 0) ; 2: nop
    Machine m({addiu(t0, t0, 1), bne(t0, t1, -2), nop(), addiu(t2, zero, 9)});
    m.cpu.set_gpr(t1, 3);
    CHECK_EQ(m.run_until_stop(3 * 3 + 1), StepResult::Executed);
    CHECK_EQ(m.reg(t0), u32{3});
    CHECK_EQ(m.reg(t2), u32{9});
}

TEST_CASE("cpu/BLEZ, BGTZ, BLTZ e BGEZ") {
    struct Case {
        u32 (*encode)(u32, s32);
        s32 value;
        bool taken;
    };
    const Case cases[] = {
        {blez, -1, true},  {blez, 0, true},   {blez, 1, false}, {bgtz, -1, false},
        {bgtz, 0, false},  {bgtz, 1, true},   {bltz, -1, true}, {bltz, 0, false},
        {bltz, 1, false},  {bgez, -1, false}, {bgez, 0, true},  {bgez, 1, true},
        {blez, static_cast<s32>(0x80000000), true}, {bgtz, 0x7FFFFFFF, true},
    };
    for (const auto& c : cases) {
        Machine m({c.encode(t0, 2), nop(), addiu(t1, zero, 1), addiu(t2, zero, 1)});
        m.cpu.set_gpr(t0, static_cast<u32>(c.value));
        m.steps(3);
        CHECK_EQ(m.reg(t2) == 1 && m.reg(t1) == 0, c.taken);
    }
}

TEST_CASE("cpu/BLTZAL e BGEZAL gravam ra mesmo sem desvio") {
    {
        Machine m({bltzal(t0, 4), nop()});
        m.cpu.set_gpr(t0, 1); // não toma
        m.steps(2);
        CHECK_EQ(m.reg(ra), base + 8);
        CHECK_EQ(m.cpu.pc(), base + 8);
    }
    {
        Machine m({bgezal(t0, 4), nop()});
        m.cpu.set_gpr(t0, 0); // toma
        m.steps(2);
        CHECK_EQ(m.reg(ra), base + 8);
        CHECK_EQ(m.cpu.pc(), base + 4 + 16);
    }
}

TEST_CASE("cpu/BGEZAL com rs = ra compara o valor anterior ao link") {
    Machine m({bgezal(ra, 4), nop()});
    m.cpu.set_gpr(ra, 0xFFFFFFF0); // negativo: não toma, apesar de base+8 ser "positivo"
    m.steps(2);
    CHECK_EQ(m.reg(ra), base + 8);
    CHECK_EQ(m.cpu.pc(), base + 8);
}

TEST_CASE("cpu/BcondZ com rt não documentado: bit 0 decide, sem link") {
    // ⚠️ psx-spx só documenta rt = 00h, 01h, 10h, 11h (D-020).
    Machine a({bcondz_raw(t0, 0x02, 2), nop(), nop(), nop()});
    a.cpu.set_gpr(t0, static_cast<u32>(-1));
    a.steps(2);
    CHECK_EQ(a.cpu.pc(), base + 12); // como BLTZ, tomado
    CHECK_EQ(a.reg(ra), u32{0});

    Machine b({bcondz_raw(t0, 0x13, 2), nop(), nop(), nop()});
    b.cpu.set_gpr(t0, 5);
    b.steps(2);
    CHECK_EQ(b.cpu.pc(), base + 12); // como BGEZ, tomado
    CHECK_EQ(b.reg(ra), u32{0});
}

TEST_CASE("cpu/J e JAL usam os 4 bits altos do PC") {
    {
        Machine m({j(0x00010010), addiu(t0, zero, 1), nop(), nop(), addiu(t1, zero, 2)});
        m.steps(3);
        CHECK_EQ(m.reg(t0), u32{1});
        CHECK_EQ(m.reg(t1), u32{2}); // alvo 80010010h (bits altos de 8001xxxxh)
    }
    {
        Machine m({jal(base + 0x20), nop()});
        m.steps(2);
        CHECK_EQ(m.reg(ra), base + 8);
        CHECK_EQ(m.cpu.pc(), base + 0x20);
    }
}

TEST_CASE("cpu/JR e JALR") {
    {
        Machine m({jalr(t1, t0), nop()});
        m.cpu.set_gpr(t0, base + 0x40);
        m.steps(2);
        CHECK_EQ(m.reg(t1), base + 8);
        CHECK_EQ(m.cpu.pc(), base + 0x40);
    }
    {
        // rd == rs: o salto usa o valor antigo; o registrador recebe o link.
        Machine m({jalr(t0, t0), nop()});
        m.cpu.set_gpr(t0, base + 0x40);
        m.steps(2);
        CHECK_EQ(m.reg(t0), base + 8);
        CHECK_EQ(m.cpu.pc(), base + 0x40);
    }
    {
        Machine m({jr(ra), addiu(t0, zero, 3)});
        m.cpu.set_gpr(ra, base + 0x80);
        m.steps(2);
        CHECK_EQ(m.reg(t0), u32{3});
        CHECK_EQ(m.cpu.pc(), base + 0x80);
    }
}

TEST_CASE("cpu/desvio no load delay slot lê o valor antigo") {
    // 0: lw t0 ; 1: beq t0,zero (lê t0 antigo = 0 → toma) ; 2: addu t1,t0 (já vê o novo valor)
    Machine m({lw(t0, 0, t9), beq(t0, zero, 3), addu(t1, t0, zero), addiu(t2, zero, 1), nop(),
               addiu(t3, zero, 1)});
    m.cpu.set_gpr(t9, Machine::data_base);
    m.poke32(Machine::data_base, 0x77);
    m.steps(4);
    CHECK_EQ(m.reg(t1), u32{0x77}); // duas instruções após a carga
    CHECK_EQ(m.reg(t2), u32{0});    // pulada: o desvio foi tomado com t0 = 0
    CHECK_EQ(m.reg(t3), u32{1});    // alvo = endereço do beq + 4 + 3*4
}

TEST_CASE("cpu/salto para alvo desalinhado gera AdEL na busca do alvo") {
    Machine m({jr(t0), addiu(t1, zero, 1)});
    m.cpu.set_gpr(t0, base + 0x22);
    m.steps(2); // o salto e o delay slot executam normalmente
    CHECK_EQ(m.reg(t1), u32{1});
    CHECK(m.cpu.step() == StepResult::Exception);
    const auto& ex = m.cpu.last_exception();
    REQUIRE(ex.has_value());
    CHECK(ex->code == ExceptionCode::AddressErrorLoad);
    CHECK_EQ(ex->epc, base + 0x22);
    CHECK_EQ(ex->bad_vaddr, std::optional<u32>(base + 0x22));
    CHECK(!ex->branch_delay);
}

TEST_CASE("cpu/desvio dentro de delay slot segue o modelo de pipeline") {
    // ⚠️ Comportamento não documentado no psx-spx; resultado emergente do modelo (D-021).
    // 0: j A ; 1: j B (delay slot) ; A: addiu t0 ; B: addiu t1
    const u32 a_addr = base + 0x20;
    const u32 b_addr = base + 0x40;
    Machine m({j(a_addr), j(b_addr)});
    m.load({addiu(t0, zero, 1), addiu(t2, zero, 1)}, a_addr);
    m.load({addiu(t1, zero, 1)}, b_addr);
    m.steps(4);
    // Executa: j A, j B, instrução em A (delay slot do segundo j), instrução em B.
    CHECK_EQ(m.reg(t0), u32{1});
    CHECK_EQ(m.reg(t2), u32{0});
    CHECK_EQ(m.reg(t1), u32{1});
}
