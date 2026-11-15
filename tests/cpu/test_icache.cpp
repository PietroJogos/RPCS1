#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::InstructionCache;
using cpu::StepResult;
namespace bcc = rpcs1::cpu::bcc;
namespace sr = rpcs1::cpu::sr;

namespace {

constexpr u32 base = Machine::code_base;
constexpr u32 cache_on = bcc::is1 | (1u << bcc::iblksz_shift); // IS1, IBLKSZ = 4 palavras

void set_bcc(Machine& m, u32 value) {
    m.bus.memory_control().set_cache_control(value);
}

} // namespace

TEST_CASE("icache/desligada: a busca sempre lê a memória") {
    Machine m({addiu(t0, t0, 1)});
    m.steps(1);
    m.poke32(base, addiu(t0, t0, 100));
    m.cpu.set_pc(base);
    m.steps(1);
    CHECK_EQ(m.reg(t0), u32{101});
    CHECK_EQ(m.cpu.icache().misses(), u64{0});
}

TEST_CASE("icache/ligada: código alterado na RAM continua obsoleto; KSEG1 não usa a cache") {
    Machine m({addiu(t0, t0, 1)});
    set_bcc(m, cache_on);
    m.steps(1); // falha: preenche a linha
    m.poke32(base, addiu(t0, t0, 100));
    m.cpu.set_pc(base);
    m.steps(1); // acerto: executa a instrução antiga
    CHECK_EQ(m.reg(t0), u32{2});
    m.cpu.set_pc(0xA0000000u + (base & 0x1FFFFFFFu)); // mesmo endereço físico via KSEG1
    m.steps(1);
    CHECK_EQ(m.reg(t0), u32{102});
    CHECK_EQ(m.cpu.icache().misses(), u64{1});
    CHECK_EQ(m.cpu.icache().hits(), u64{1});
}

TEST_CASE("icache/preenche da palavra acessada até o fim da linha") {
    const u8 expected[] = {0xF, 0xE, 0xC, 0x8};
    for (u32 word = 0; word < 4; ++word) {
        Machine m({nop(), nop(), nop(), nop()});
        set_bcc(m, cache_on);
        m.cpu.set_pc(base + word * 4);
        m.steps(1);
        const auto& line = m.cpu.icache().line(InstructionCache::index_of(base));
        CHECK_EQ(line.valid, expected[word]);
        CHECK_EQ(line.tag, u32{0x00010000});
    }
    // IBLKSZ = 0: falha na palavra 0 preenche só duas palavras.
    Machine m({nop(), nop(), nop(), nop()});
    set_bcc(m, bcc::is1);
    m.steps(1);
    CHECK_EQ(m.cpu.icache().line(InstructionCache::index_of(base)).valid, u8{0x3});
}

TEST_CASE("icache/KUSEG e KSEG0 compartilham a tag física") {
    Machine m({addiu(t0, t0, 1)});
    set_bcc(m, cache_on);
    m.steps(1);
    m.poke32(base, addiu(t0, t0, 100));
    m.cpu.set_pc(base & 0x1FFFFFFFu); // KUSEG
    m.steps(1);
    CHECK_EQ(m.reg(t0), u32{2}); // acerto com a palavra antiga
}

TEST_CASE("icache/isolamento: IS1 grava palavras na cache sem tocar a RAM") {
    // Código em KSEG1 (sem cache), como a rotina de limpeza da BIOS.
    Machine m({sw(t1, 0x100, zero), lw(t2, 0x100, zero), nop()}, 0xA0010000);
    m.poke32(0x80000100, 0xAAAAAAAA);
    m.cpu.set_gpr(t1, 0x12345678);
    set_bcc(m, bcc::is1);
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::isc);
    m.steps(3);
    CHECK_EQ(m.word(0x80000100), u32{0xAAAAAAAA}); // RAM intocada
    CHECK_EQ(m.cpu.icache().line(0x10).words[0], u32{0x12345678});
    CHECK_EQ(m.reg(t2), u32{0x12345678}); // load isolado lê a cache
}

TEST_CASE("icache/isolamento: TAG grava tag e validade; load devolve validade e acerto") {
    Machine m({sw(t1, 0x200, zero), lw(t2, 0x200, zero), lw(t3, 0x1200, zero), nop()}, 0xA0010000);
    m.cpu.set_gpr(t1, 0xFFFFFFF5); // só os 4 bits baixos importam
    set_bcc(m, bcc::is1 | bcc::tag_test);
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::isc);
    m.steps(4);
    const auto& line = m.cpu.icache().line(0x20);
    CHECK_EQ(line.tag, u32{0});
    CHECK_EQ(line.valid, u8{0x5});
    CHECK_EQ(m.reg(t2), u32{0x15}); // validade + bit 4 (tag coincide)
    CHECK_EQ(m.reg(t3), u32{0x05}); // mesma linha, outra tag: sem bit 4
}

TEST_CASE("icache/rotina de limpeza no estilo da BIOS invalida a cache sem apagar a RAM") {
    Machine m({
        lui(t0, 0xFFFE), ori(t0, t0, 0x0130),
        ori(t1, zero, bcc::is1 | bcc::tag_test), sw(t1, 0, t0), // BCC = IS1 | TAG
        lui(t2, sr::isc >> 16), mtc0(t2, cpu::cop0reg::sr),     // SR.IsC = 1
        addu(t3, zero, zero), ori(t4, zero, 0x1000),
        sw(zero, 0, t3),                                        // laço: zera tag/validade
        addiu(t3, t3, 16),
        bne(t3, t4, -3),
        nop(),
        mtc0(zero, cpu::cop0reg::sr),                           // IsC = 0
        nop(),
        ori(t1, zero, cache_on), sw(t1, 0, t0),                 // restaura BCC
        break_(),
    }, 0xA0020000);
    // Cache previamente preenchida por código em KSEG0 e RAM baixa com dados.
    m.poke32(0x80000040, 0x12345678);
    for (u32 i = 0; i < 16; ++i) {
        m.poke32(base + i * 4, nop());
    }
    set_bcc(m, cache_on);
    m.cpu.set_pc(base);
    m.steps(16);
    REQUIRE(m.cpu.icache().line(InstructionCache::index_of(base)).valid != 0);

    m.cpu.set_pc(0xA0020000);
    CHECK(m.run_until_stop() == StepResult::Exception); // BREAK no fim da rotina
    for (u32 i = 0; i < InstructionCache::line_count; ++i) {
        if (m.cpu.icache().line(i).valid != 0) {
            CHECK(false);
            break;
        }
    }
    CHECK_EQ(m.word(0x80000040), u32{0x12345678}); // a RAM não foi zerada
    CHECK(!m.cpu.cop0().cache_isolated());
    CHECK_EQ(m.bus.memory_control().cache_control(), cache_on);
}
