#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::Cop0;
using cpu::ExceptionCode;
using cpu::StepResult;
namespace sr = rpcs1::cpu::sr;

namespace {
constexpr u32 base = Machine::code_base;
constexpr u32 vector = Machine::exception_vector;
constexpr u32 user_base = 0x00010000; // KUSEG (mesma RAM de 80010000h)

ExceptionCode code_after_step(Machine& m) {
    if (m.cpu.step() != StepResult::Exception || !m.cpu.last_exception()) {
        return static_cast<ExceptionCode>(0xFF);
    }
    return m.cpu.last_exception()->code;
}
} // namespace

TEST_CASE("cop0/valores de reset") {
    Cop0 c;
    CHECK_EQ(c.status(), u32{0x00400000}); // BEV = 1 (⚠️ D-025)
    CHECK_EQ(c.read(c0_prid, false), std::optional<u32>(0x00000002));
    CHECK_EQ(c.epc(), u32{0});
    CHECK_EQ(c.cause(false), u32{0});
    CHECK(!c.user_mode());
    CHECK(!c.cache_isolated());
}

TEST_CASE("cop0/MTC0 e MFC0 (com atraso de uma instrução)") {
    Machine m({lui(t0, 0x1000), ori(t0, t0, 0x0401), mtc0(t0, c0_sr), mfc0(t1, c0_sr),
               addu(t2, t1, zero), addu(t3, t1, zero)});
    m.steps(6);
    CHECK_EQ(m.cpu.cop0().status(), u32{0x10000401});
    CHECK_EQ(m.reg(t2), u32{0});          // delay slot do MFC0 vê o valor antigo
    CHECK_EQ(m.reg(t3), u32{0x10000401});
}

TEST_CASE("cop0/registradores de depuração e máscara do DCIC") {
    Machine m({mtc0(t0, c0_bpc), mtc0(t0, c0_bda), mtc0(t0, c0_dcic), mtc0(t0, c0_bdam),
               mtc0(t0, c0_bpcm), mfc0(t1, c0_bpc), mfc0(t2, c0_bda), mfc0(t3, c0_dcic),
               mfc0(t4, c0_bdam), mfc0(t5, c0_bpcm), nop()});
    m.cpu.set_gpr(t0, 0xFFFFFFFF);
    m.steps(11);
    CHECK_EQ(m.reg(t1), u32{0xFFFFFFFF});
    CHECK_EQ(m.reg(t2), u32{0xFFFFFFFF});
    CHECK_EQ(m.reg(t3), u32{0xFF80303F});
    CHECK_EQ(m.reg(t4), u32{0xFFFFFFFF});
    CHECK_EQ(m.reg(t5), u32{0xFFFFFFFF});
}

TEST_CASE("cop0/SR mantém zerados os bits 6–7, 23–24 e 26–27") {
    Cop0 c;
    c.write(c0_sr, 0xFFFFFFFF);
    CHECK_EQ(c.status(), u32{0xF27FFF3F});
}

TEST_CASE("cop0/registradores somente leitura ignoram MTC0; CAUSE só aceita bits 8–9") {
    Cop0 c;
    for (u32 reg : {c0_tar, c0_badvaddr, c0_epc, c0_prid}) {
        c.write(reg, 0x12345678);
    }
    CHECK_EQ(c.tar(), u32{0});
    CHECK_EQ(c.bad_vaddr(), u32{0});
    CHECK_EQ(c.epc(), u32{0});
    CHECK_EQ(c.read(c0_prid, false), std::optional<u32>(2));
    c.write(c0_cause, 0xFFFFFFFF);
    CHECK_EQ(c.cause(false), u32{0x00000300});
}

TEST_CASE("cop0/MFC0 de registrador inexistente gera RI; r16–r31 leem zero") {
    for (u32 reg : {0u, 1u, 2u, 4u, 10u}) {
        Machine m({mfc0(t0, reg)});
        CHECK(code_after_step(m) == ExceptionCode::ReservedInstruction);
    }
    for (u32 reg : {16u, 25u, 31u}) {
        Machine m({mfc0(t0, reg), nop()});
        m.cpu.set_gpr(t0, 0x55);
        m.steps(2);
        CHECK_EQ(m.reg(t0), u32{0});
    }
}

TEST_CASE("cop0/comandos TLB, comandos desconhecidos e CFC0 geram RI") {
    for (u32 word : {tlbr(), tlbp(), cop_command(0, 0x02), cop_command(0, 0x06), cfc0(t0, 12)}) {
        Machine m({word});
        CHECK(code_after_step(m) == ExceptionCode::ReservedInstruction);
    }
}

TEST_CASE("cop0/RFE desempilha modo e interrupção") {
    struct Case {
        u32 before;
        u32 after;
    };
    // bits 2–3 → 0–1, 4–5 → 2–3, 4–5 inalterados.
    const Case cases[] = {{0x3C, 0x3F}, {0x24, 0x29}, {0x04, 0x01}, {0x30, 0x3C}};
    for (const auto& c : cases) {
        Machine m({rfe()});
        m.cpu.cop0().write(c0_sr, c.before);
        m.steps(1);
        CHECK_EQ(m.cpu.cop0().status() & 0x3Fu, c.after);
    }
}

TEST_CASE("cop0/entrada em exceção empilha SR e preenche CAUSE e EPC") {
    struct Case {
        u32 before;
        u32 after;
    };
    const Case cases[] = {{0x01, 0x04}, {0x0D, 0x34}, {0x05, 0x14}};
    for (const auto& c : cases) {
        Machine m({syscall()});
        m.cpu.cop0().write(c0_sr, c.before);
        CHECK(code_after_step(m) == ExceptionCode::Syscall);
        CHECK_EQ(m.cpu.cop0().status() & 0x3Fu, c.after);
        CHECK_EQ(m.cpu.cop0().cause(false), u32{0x08 << 2});
        CHECK_EQ(m.cpu.cop0().epc(), base);
        CHECK_EQ(m.cpu.pc(), vector);
    }
}

TEST_CASE("cop0/BEV = 1 usa o vetor BFC00180h") {
    Machine m({syscall()});
    m.cpu.cop0().write(c0_sr, sr::bev);
    CHECK(code_after_step(m) == ExceptionCode::Syscall);
    CHECK_EQ(m.cpu.pc(), u32{0xBFC00180});
}

TEST_CASE("cop0/exceção em delay slot: BD, BT e TAR; desvio não tomado preserva TAR") {
    {
        Machine m({beq(zero, zero, 3), syscall()});
        m.steps(1);
        CHECK(code_after_step(m) == ExceptionCode::Syscall);
        const u32 cause = m.cpu.cop0().cause(false);
        CHECK((cause >> 31) == 1u);
        CHECK(((cause >> 30) & 1u) == 1u);
        CHECK_EQ(m.cpu.cop0().epc(), base);
        CHECK_EQ(m.cpu.cop0().tar(), base + 4 + 12);
    }
    {
        Machine m({bne(zero, zero, 3), syscall()});
        m.steps(1);
        CHECK(code_after_step(m) == ExceptionCode::Syscall);
        const u32 cause = m.cpu.cop0().cause(false);
        CHECK((cause >> 31) == 1u);
        CHECK(((cause >> 30) & 1u) == 0u);
        CHECK_EQ(m.cpu.cop0().tar(), u32{0});
    }
}

TEST_CASE("cop0/BadVaddr só muda em AdEL/AdES") {
    Machine m({lw(t0, 2, t9)});
    m.load({lw(t1, 0, t8)}, vector); // tratador gera DBE
    m.cpu.set_gpr(t9, Machine::data_base);
    m.cpu.set_gpr(t8, 0x1F801024);   // lacuna de E/S
    CHECK(code_after_step(m) == ExceptionCode::AddressErrorLoad);
    CHECK_EQ(m.cpu.cop0().bad_vaddr(), Machine::data_base + 2);
    CHECK(code_after_step(m) == ExceptionCode::BusErrorData);
    CHECK_EQ(m.cpu.cop0().bad_vaddr(), Machine::data_base + 2); // inalterado
}

TEST_CASE("cop0/CpU informa o coprocessador em CAUSE.CE") {
    struct Case {
        u32 word;
        u32 ce;
    };
    const Case cases[] = {
        {gte_rtps(), 2},          {mfc2(t0, 0), 2}, {0x11u << 26, 1}, {0x13u << 26, 3},
        {0x30u << 26, 0} /*LWC0*/, {0x38u << 26, 0} /*SWC0*/, {0x33u << 26, 3} /*LWC3*/,
        {0x3Au << 26, 2} /*SWC2*/,
    };
    for (const auto& c : cases) {
        Machine m({c.word});
        CHECK(code_after_step(m) == ExceptionCode::CoprocessorUnusable);
        CHECK_EQ((m.cpu.cop0().cause(false) >> 28) & 3u, c.ce);
    }
}

TEST_CASE("cop0/COP1 e COP3 com CU1/CU3 ligados não têm efeito") {
    Machine m({0x11u << 26, 0x13u << 26, 0x31u << 26, 0x3Bu << 26, addiu(t0, zero, 1)});
    m.cpu.cop0().write(c0_sr, sr::cu1 | sr::cu3);
    m.steps(5);
    CHECK_EQ(m.reg(t0), u32{1});
}

TEST_CASE("cop0/modo usuário: COP0 exige CU0 (exceto ler r16–r31)") {
    {
        Machine m({mfc0(t0, c0_sr)}, user_base);
        m.cpu.cop0().write(c0_sr, sr::kuc);
        CHECK(code_after_step(m) == ExceptionCode::CoprocessorUnusable);
        CHECK_EQ((m.cpu.cop0().cause(false) >> 28) & 3u, u32{0});
        CHECK(!m.cpu.cop0().user_mode()); // a exceção entra em modo kernel
    }
    {
        Machine m({mfc0(t0, c0_sr), nop()}, user_base);
        m.cpu.cop0().write(c0_sr, sr::kuc | sr::cu0);
        m.steps(2);
        CHECK_EQ(m.reg(t0), sr::kuc | sr::cu0);
    }
    {
        Machine m({mfc0(t0, 20), nop()}, user_base);
        m.cpu.cop0().write(c0_sr, sr::kuc);
        m.steps(2);
    }
    {
        Machine m({rfe()}, user_base);
        m.cpu.cop0().write(c0_sr, sr::kuc);
        CHECK(code_after_step(m) == ExceptionCode::CoprocessorUnusable);
    }
}

TEST_CASE("cop0/modo usuário: acesso fora do KUSEG gera AdEL/AdES") {
    struct Case {
        u32 word;
        ExceptionCode code;
    };
    const Case cases[] = {
        {lw(t0, 0, t9), ExceptionCode::AddressErrorLoad},
        {lb(t0, 1, t9), ExceptionCode::AddressErrorLoad},
        {lwl(t0, 3, t9), ExceptionCode::AddressErrorLoad},
        {sw(t0, 0, t9), ExceptionCode::AddressErrorStore},
        {swr(t0, 1, t9), ExceptionCode::AddressErrorStore},
    };
    for (const auto& c : cases) {
        Machine m({c.word}, user_base);
        m.cpu.cop0().write(c0_sr, sr::kuc);
        m.cpu.set_gpr(t9, 0x80002000);
        CHECK(code_after_step(m) == c.code);
        CHECK(m.cpu.cop0().bad_vaddr() >= 0x80002000u);
    }
    // No KUSEG funciona normalmente.
    Machine ok({lw(t0, 0, t9), sw(t0, 4, t9), nop()}, user_base);
    ok.cpu.cop0().write(c0_sr, sr::kuc);
    ok.cpu.set_gpr(t9, 0x00002000);
    ok.steps(3);
    // Busca de instrução fora do KUSEG em modo usuário.
    Machine jump({jr(t0), nop()}, user_base);
    jump.cpu.cop0().write(c0_sr, sr::kuc);
    jump.cpu.set_gpr(t0, base);
    jump.steps(2);
    CHECK(code_after_step(jump) == ExceptionCode::AddressErrorLoad);
    CHECK_EQ(jump.cpu.cop0().epc(), base);
    CHECK_EQ(jump.cpu.cop0().bad_vaddr(), base);
}

TEST_CASE("cop0/tratador completo: overflow, retorno para EPC+4 com RFE") {
    Machine m({lui(t0, 0x7FFF), ori(t0, t0, 0xFFFF), add(t1, t0, t0), addiu(s0, zero, 1), nop()});
    m.load({mfc0(k0, c0_epc), nop(), addiu(k0, k0, 4), addiu(s1, zero, 7), jr(k0), rfe()}, vector);
    m.cpu.cop0().write(c0_sr, sr::iec);
    for (int i = 0; i < 10; ++i) {
        (void)m.cpu.step();
    }
    CHECK_EQ(m.reg(s1), u32{7});
    CHECK_EQ(m.reg(s0), u32{1});
    CHECK_EQ(m.reg(t1), u32{0});
    CHECK_EQ(m.cpu.cop0().status() & 0x3Fu, sr::iec); // restaurado pelo RFE
    CHECK_EQ(m.cpu.pc(), base + 16);
    CHECK_EQ(m.cpu.exceptions_taken(), u64{1});
}
