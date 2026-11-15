#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::ExceptionCode;
using cpu::StepResult;

TEST_CASE("cpu/estado de reset") {
    bus::Bus b;
    cpu::Cpu c(b);
    CHECK_EQ(c.pc(), u32{0xBFC00000});
    for (u32 r = 0; r < 32; ++r) {
        CHECK_EQ(c.gpr(r), u32{0});
    }
    CHECK_EQ(c.hi(), u32{0});
    CHECK_EQ(c.lo(), u32{0});
    CHECK_EQ(c.cycles(), u64{0});
    CHECK(!c.halted());
}

TEST_CASE("cpu/executa a partir do vetor de reset (BIOS sintética)") {
    bus::Bus b;
    std::vector<u8> image(memory::BiosRom::size, 0);
    const u32 program[] = {addiu(t0, zero, 5), addiu(t1, t0, 1), lui(t2, 0xBFC0)};
    for (usize i = 0; i < 3; ++i) {
        for (usize k = 0; k < 4; ++k) {
            image[i * 4 + k] = static_cast<u8>(program[i] >> (8 * k));
        }
    }
    REQUIRE(b.bios().load(image).ok());
    cpu::Cpu c(b);
    REQUIRE(c.step() == StepResult::Executed);
    REQUIRE(c.step() == StepResult::Executed);
    REQUIRE(c.step() == StepResult::Executed);
    CHECK_EQ(c.gpr(t0), u32{5});
    CHECK_EQ(c.gpr(t1), u32{6});
    CHECK_EQ(c.gpr(t2), u32{0xBFC00000});
    CHECK_EQ(c.pc(), u32{0xBFC0000C});
}

TEST_CASE("cpu/LUI e ORI montam constantes de 32 bits") {
    Machine m({lui(t0, 0x1234), ori(t0, t0, 0x5678), lui(t1, 0xFFFF)});
    m.steps(3);
    CHECK_EQ(m.reg(t0), u32{0x12345678});
    CHECK_EQ(m.reg(t1), u32{0xFFFF0000});
}

TEST_CASE("cpu/ADDIU e ADDU estendem sinal e não geram overflow") {
    Machine m({addiu(t1, t0, 1), addiu(t2, zero, -1), addu(t3, t0, t0), subu(t4, zero, t2)});
    m.cpu.set_gpr(t0, 0x7FFFFFFF);
    m.steps(4);
    CHECK_EQ(m.reg(t1), u32{0x80000000});
    CHECK_EQ(m.reg(t2), u32{0xFFFFFFFF});
    CHECK_EQ(m.reg(t3), u32{0xFFFFFFFE});
    CHECK_EQ(m.reg(t4), u32{1});
}

TEST_CASE("cpu/ADD, ADDI e SUB sem overflow") {
    Machine m({add(t2, t0, t1), addi(t3, t0, -10), sub(t4, t1, t0)});
    m.cpu.set_gpr(t0, static_cast<u32>(-5));
    m.cpu.set_gpr(t1, 3);
    m.steps(3);
    CHECK_EQ(m.reg(t2), static_cast<u32>(-2));
    CHECK_EQ(m.reg(t3), static_cast<u32>(-15));
    CHECK_EQ(m.reg(t4), u32{8});
}

TEST_CASE("cpu/ADD, ADDI e SUB com overflow não alteram o destino") {
    struct Case {
        u32 instruction;
        u32 a;
        u32 b;
    };
    const Case cases[] = {
        {add(t2, t0, t1), 0x7FFFFFFF, 1},
        {add(t2, t0, t1), 0x80000000, 0xFFFFFFFF},
        {addi(t2, t0, 1), 0x7FFFFFFF, 0},
        {addi(t2, t0, -1), 0x80000000, 0},
        {sub(t2, t0, t1), 0x80000000, 1},
        {sub(t2, t0, t1), 0x7FFFFFFF, 0xFFFFFFFF},
    };
    for (const auto& c : cases) {
        Machine m({c.instruction});
        m.cpu.set_gpr(t0, c.a);
        m.cpu.set_gpr(t1, c.b);
        m.cpu.set_gpr(t2, 0xDEAD);
        CHECK(m.cpu.step() == StepResult::Exception);
        REQUIRE(m.cpu.last_exception().has_value());
        CHECK(m.cpu.last_exception()->code == ExceptionCode::Overflow);
        CHECK_EQ(m.cpu.last_exception()->epc, Machine::code_base);
        CHECK_EQ(m.reg(t2), u32{0xDEAD});
        CHECK_EQ(m.cpu.pc(), Machine::exception_vector); // desviou ao tratador
        CHECK_EQ(m.cpu.cop0().epc(), Machine::code_base);
        CHECK(!m.cpu.halted());
    }
}

TEST_CASE("cpu/operações lógicas; imediatos com extensão de zero") {
    Machine m({and_(t2, t0, t1), or_(t3, t0, t1), xor_(t4, t0, t1), nor(t5, t0, t1),
               ori(t6, zero, 0x8000), andi(t7, t8, 0xFFFF), xori(s0, t8, 0x00FF)});
    m.cpu.set_gpr(t0, 0xF0F0F0F0);
    m.cpu.set_gpr(t1, 0xFF00FF00);
    m.cpu.set_gpr(t8, 0xFFFFFFFF);
    m.steps(7);
    CHECK_EQ(m.reg(t2), u32{0xF000F000});
    CHECK_EQ(m.reg(t3), u32{0xFFF0FFF0});
    CHECK_EQ(m.reg(t4), u32{0x0FF00FF0});
    CHECK_EQ(m.reg(t5), u32{0x000F000F});
    CHECK_EQ(m.reg(t6), u32{0x00008000});
    CHECK_EQ(m.reg(t7), u32{0x0000FFFF});
    CHECK_EQ(m.reg(s0), u32{0xFFFFFF00});
}

TEST_CASE("cpu/comparações com e sem sinal") {
    Machine m({slt(t2, t0, t1), sltu(t3, t0, t1), slti(t4, t0, -4), slti(t5, t0, -6),
               sltiu(t6, t1, -1), sltiu(t7, t0, -1), sltiu(s0, t1, 2)});
    m.cpu.set_gpr(t0, static_cast<u32>(-5)); // FFFFFFFBh
    m.cpu.set_gpr(t1, 1);
    m.steps(7);
    CHECK_EQ(m.reg(t2), u32{1}); // -5 < 1
    CHECK_EQ(m.reg(t3), u32{0}); // FFFFFFFBh < 1 (sem sinal)? não
    CHECK_EQ(m.reg(t4), u32{1}); // -5 < -4
    CHECK_EQ(m.reg(t5), u32{0}); // -5 < -6? não
    CHECK_EQ(m.reg(t6), u32{1}); // 1 < FFFFFFFFh (imediato estendido, comparação sem sinal)
    CHECK_EQ(m.reg(t7), u32{1}); // FFFFFFFBh < FFFFFFFFh
    CHECK_EQ(m.reg(s0), u32{1}); // 1 < 2
}

TEST_CASE("cpu/deslocamentos usam os 5 bits baixos de rs") {
    Machine m({sll(t2, t0, 31), srl(t3, t1, 31), sra(t4, t1, 31), sra(t5, t1, 4), sllv(t6, t0, s0),
               srlv(t7, t1, s1), srav(s2, t1, s0), sll(s3, t0, 0)});
    m.cpu.set_gpr(t0, 1);
    m.cpu.set_gpr(t1, 0x80000000);
    m.cpu.set_gpr(s0, 33); // 33 & 31 = 1
    m.cpu.set_gpr(s1, 32); // 32 & 31 = 0
    m.steps(8);
    CHECK_EQ(m.reg(t2), u32{0x80000000});
    CHECK_EQ(m.reg(t3), u32{1});
    CHECK_EQ(m.reg(t4), u32{0xFFFFFFFF});
    CHECK_EQ(m.reg(t5), u32{0xF8000000});
    CHECK_EQ(m.reg(t6), u32{2});
    CHECK_EQ(m.reg(t7), u32{0x80000000});
    CHECK_EQ(m.reg(s2), u32{0xC0000000});
    CHECK_EQ(m.reg(s3), u32{1});
}

TEST_CASE("cpu/r0 permanece zero") {
    Machine m({addiu(zero, zero, 5), ori(zero, t0, 0xFFFF), lui(zero, 0x1234), addu(t1, zero, zero),
               lw(zero, 0, t9), nop(), addu(t2, zero, zero)});
    m.cpu.set_gpr(t0, 0xFFFFFFFF);
    m.cpu.set_gpr(t9, Machine::data_base);
    m.poke32(Machine::data_base, 0x12345678);
    m.cpu.set_gpr(zero, 99); // ignorado
    m.steps(7);
    CHECK_EQ(m.reg(zero), u32{0});
    CHECK_EQ(m.reg(t1), u32{0});
    CHECK_EQ(m.reg(t2), u32{0});
}

TEST_CASE("cpu/um ciclo por instrução sem bloqueios") {
    Machine m(std::vector<u32>(10, nop()));
    m.steps(10);
    CHECK_EQ(m.cpu.cycles(), u64{10});
    CHECK_EQ(m.cpu.instructions(), u64{10});
    CHECK_EQ(m.cpu.pc(), Machine::code_base + 40);
}
