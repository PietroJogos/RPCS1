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

TEST_CASE("cpu/SYSCALL e BREAK") {
    {
        Machine m({nop(), syscall(0x12345)});
        m.steps(1);
        CHECK(m.cpu.step() == StepResult::Exception);
        REQUIRE(m.cpu.last_exception().has_value());
        CHECK(m.cpu.last_exception()->code == ExceptionCode::Syscall);
        CHECK_EQ(m.cpu.last_exception()->epc, base + 4);
        CHECK(!m.cpu.last_exception()->bad_vaddr.has_value());
    }
    {
        Machine m({break_(7)});
        CHECK(m.cpu.step() == StepResult::Exception);
        CHECK(m.cpu.last_exception()->code == ExceptionCode::Breakpoint);
    }
}

TEST_CASE("cpu/exceção no delay slot: BD, EPC, BT e TAR") {
    {
        Machine m({beq(zero, zero, 3), syscall()});
        m.steps(1);
        CHECK(m.cpu.step() == StepResult::Exception);
        const auto& ex = m.cpu.last_exception();
        REQUIRE(ex.has_value());
        CHECK(ex->branch_delay);
        CHECK_EQ(ex->epc, base); // aponta para o desvio
        CHECK_EQ(ex->instruction_address, base + 4);
        CHECK(ex->branch_taken);
        CHECK_EQ(ex->branch_target, base + 4 + 12);
    }
    {
        Machine m({bne(zero, zero, 3), break_()});
        m.steps(1);
        CHECK(m.cpu.step() == StepResult::Exception);
        const auto& ex = m.cpu.last_exception();
        REQUIRE(ex.has_value());
        CHECK(ex->branch_delay);
        CHECK(!ex->branch_taken);
        CHECK_EQ(ex->epc, base);
    }
}

TEST_CASE("cpu/opcodes reservados geram RI") {
    std::vector<u32> primary;
    for (u32 o = 0x14; o <= 0x1F; ++o) primary.push_back(o);
    for (u32 o : {0x27u, 0x2Cu, 0x2Du, 0x2Fu}) primary.push_back(o);
    for (u32 o = 0x34; o <= 0x37; ++o) primary.push_back(o);
    for (u32 o = 0x3C; o <= 0x3F; ++o) primary.push_back(o);
    for (u32 o : primary) {
        Machine m({o << 26});
        CHECK(m.cpu.step() == StepResult::Exception);
        if (m.cpu.last_exception()) {
            CHECK(m.cpu.last_exception()->code == ExceptionCode::ReservedInstruction);
        }
    }

    std::vector<u32> special;
    for (u32 f : {0x01u, 0x05u, 0x0Au, 0x0Bu, 0x0Eu, 0x0Fu}) special.push_back(f);
    for (u32 f = 0x14; f <= 0x17; ++f) special.push_back(f);
    for (u32 f = 0x1C; f <= 0x1F; ++f) special.push_back(f);
    for (u32 f : {0x28u, 0x29u}) special.push_back(f);
    for (u32 f = 0x2C; f <= 0x3F; ++f) special.push_back(f);
    for (u32 f : special) {
        Machine m({f});
        CHECK(m.cpu.step() == StepResult::Exception);
        if (m.cpu.last_exception()) {
            CHECK(m.cpu.last_exception()->code == ExceptionCode::ReservedInstruction);
        }
    }
}

TEST_CASE("cpu/bits não usados não geram exceção") {
    // JR com campos rt/rd preenchidos; BLEZ com rt != 0; SLL com rs != 0.
    Machine m({jr(t0) | (0x1Fu << 11) | (0x1Fu << 16), nop()});
    m.cpu.set_gpr(t0, base + 0x10);
    m.steps(2);
    CHECK_EQ(m.cpu.pc(), base + 0x10);

    // blez (tomado) → delay slot → alvo (índice 2) → sll no índice 3.
    Machine n({blez(zero, 1) | (5u << 16), nop(), nop(), sll(t0, t1, 1) | (3u << 21)});
    n.cpu.set_gpr(t1, 2);
    n.steps(4);
    CHECK_EQ(n.reg(t0), u32{4});
}

TEST_CASE("cpu/instruções de coprocessador param como não implementadas") {
    struct Case {
        u32 word;
        int phase;
    };
    const Case cases[] = {
        {mfc0(t0, 12), 4},
        {rfe(), 4},
        {0x4A180001u, 9}, // COP2 (comando da GTE)
        {(0x32u << 26), 9}, // LWC2
        {(0x3Au << 26), 9}, // SWC2
        {(0x11u << 26), 4}, // COP1
        {(0x38u << 26), 4}, // SWC0
    };
    for (const auto& c : cases) {
        Machine m({nop(), c.word});
        m.steps(1);
        CHECK(m.cpu.step() == StepResult::Unimplemented);
        REQUIRE(m.cpu.last_unimplemented().has_value());
        CHECK_EQ(m.cpu.last_unimplemented()->phase, c.phase);
        CHECK_EQ(m.cpu.last_unimplemented()->word, c.word);
        CHECK_EQ(m.cpu.pc(), base + 4);
        CHECK(m.cpu.halted());
        CHECK_EQ(m.cpu.cycles(), u64{1}); // só o nop consumiu ciclo
    }
}

TEST_CASE("cpu/CPU parada não executa mais nada") {
    Machine m({syscall(), addiu(t0, zero, 1)});
    CHECK(m.cpu.step() == StepResult::Exception);
    const u64 cycles = m.cpu.cycles();
    CHECK(m.cpu.step() == StepResult::Halted);
    CHECK(m.cpu.step() == StepResult::Halted);
    CHECK_EQ(m.reg(t0), u32{0});
    CHECK_EQ(m.cpu.cycles(), cycles);
    CHECK_EQ(m.cpu.pc(), base);
    // reset() libera a CPU.
    m.cpu.reset();
    CHECK(!m.cpu.halted());
    CHECK_EQ(m.cpu.pc(), u32{0xBFC00000});
}

TEST_CASE("cpu/busca de instrução no scratchpad ou em endereço inexistente gera IBE") {
    for (u32 address : {0x1F800000u, 0x9F800010u, 0x00800000u, 0xBFC80000u}) {
        bus::Bus b;
        cpu::Cpu c(b);
        c.set_pc(address);
        CHECK(c.step() == StepResult::Exception);
        REQUIRE(c.last_exception().has_value());
        CHECK(c.last_exception()->code == ExceptionCode::BusErrorInstruction);
        CHECK_EQ(c.last_exception()->epc, address);
        CHECK(!c.last_exception()->bad_vaddr.has_value());
    }
}
