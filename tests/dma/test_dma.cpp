// DMA. Valores "medidos" vêm dos testes em SCPH-5501 do psx.wiki ("dma2", "dma-otc",
// "dmatime", "dma-chain-looping").
#include <memory>
#include <vector>

#include "bus/bus.hpp"
#include "core/log.hpp"
#include "cpu/mips_assembler.hpp"
#include "dma/dma_controller.hpp"
#include "framework/rpcs1_test.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/console.hpp"
#include "system/scheduler.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using bus::AccessWidth;
using dma::Channel;
using dma::DmaController;

namespace {

constexpr u32 madr(u32 n) { return n * 0x10 + 0x0; }
constexpr u32 bcr(u32 n) { return n * 0x10 + 0x4; }
constexpr u32 chcr(u32 n) { return n * 0x10 + 0x8; }
constexpr u32 dpcr = 0x70;
constexpr u32 dicr = 0x74;

class TestPort final : public dma::DmaPort {
public:
    bool dma_request() const noexcept override { return dreq; }
    u32 dma_read() override { return next_read++; }
    void dma_write(u32 word) override { written.push_back(word); }
    bool dreq = true;
    u32 next_read = 0xA0000000;
    std::vector<u32> written;
};

// Liga DREQ após um número de palavras lidas/escritas (para testar a pausa do modo slice).
class LimitedPort final : public dma::DmaPort {
public:
    bool dma_request() const noexcept override { return budget > 0; }
    u32 dma_read() override { return 0; }
    void dma_write(u32 word) override {
        written.push_back(word);
        if (budget > 0) --budget;
    }
    u32 budget = 0;
    std::vector<u32> written;
};

struct DmaRig {
    u64 now = 0;
    bus::Bus b;
    system::Scheduler scheduler;
    interrupts::InterruptController irq;
    DmaController dma{b, scheduler, irq};
    u64 dma_events = 0;

    DmaRig() { scheduler.set_time_source(&now); }

    void write(u32 offset, u32 value, AccessWidth w = AccessWidth::Word) {
        dma.io_write(offset, value, w);
    }
    u32 read(u32 offset, AccessWidth w = AccessWidth::Word) { return dma.io_read(offset, w); }
    void enable(u32 n, u32 prio = 3) {
        u32 v = read(dpcr);
        v &= ~(0xFu << (4 * n));
        v |= (8u | (prio & 7u)) << (4 * n);
        write(dpcr, v);
    }
    void advance_to(u64 t) {
        while (scheduler.next_time() <= t) {
            const auto e = scheduler.pop_due(t);
            if (!e) break;
            now = e->time;
            ++dma_events;
            dma.on_event(e->time);
        }
        now = t;
    }
    u32 ram(u32 address) { return b.read32(address).value; }
    void poke(u32 address, u32 value) {
        if (b.write32(address, value) != bus::BusFault::None) throw std::runtime_error("poke");
    }
};

} // namespace

TEST_CASE("dma/valores de reset") {
    DmaRig r;
    CHECK_EQ(r.read(dpcr), u32{0x07654321});
    CHECK_EQ(r.read(dicr), u32{0});
    for (u32 n = 0; n < 6; ++n) {
        CHECK_EQ(r.read(chcr(n)), u32{0});
    }
    CHECK_EQ(r.read(chcr(6)), u32{0x00000002}); // bit 1 do OTC sempre 1
    CHECK_EQ(r.read(0x7C), u32{0x00FFFFFF});    // 1F8010FCh (medido)
}

TEST_CASE("dma/máscaras de MADR e CHCR; espelho do CHCR em +Ch") {
    DmaRig r; // DPCR padrão: nenhum canal habilitado, então nada começa
    r.write(madr(2), 0xFFFFFFFF);
    CHECK_EQ(r.read(madr(2)), u32{0x00FFFFFF});
    r.write(chcr(2), 0xFFFFFFFF);
    CHECK_EQ(r.read(chcr(2)), u32{0x71770703}); // medido
    CHECK_EQ(r.read(chcr(2) + 4), u32{0x71770703});
    r.write(chcr(6), 0xFFFFFFFF);
    CHECK_EQ(r.read(chcr(6)), u32{0x51000002}); // medido
    r.write(chcr(4) + 4, 0x00000201);           // escrita pelo espelho
    CHECK_EQ(r.read(chcr(4)), u32{0x00000201});
}

TEST_CASE("dma/SB e SH alteram só os próprios bytes de BCR e CHCR") {
    DmaRig r;
    r.write(bcr(1), 0x11223344);
    r.write(bcr(1) + 2, 0x0000AAAA, AccessWidth::Half);
    CHECK_EQ(r.read(bcr(1)), u32{0xAAAA3344});
    r.write(bcr(1) + 1, 0x000000FF, AccessWidth::Byte);
    CHECK_EQ(r.read(bcr(1)), u32{0xAAAAFF44});
    CHECK_EQ(r.read(bcr(1) + 2, AccessWidth::Half), u32{0xAAAA});
}

TEST_CASE("dma/DICR: bits graváveis, flags com 1 limpa e bit 31 somente leitura") {
    DmaRig r;
    r.write(dicr, 0x80000000);
    CHECK_EQ(r.read(dicr), u32{0}); // medido: escrever o bit 31 não tem efeito
    r.write(dicr, 0x00008000);
    CHECK_EQ(r.read(dicr), u32{0x80008000}); // medido: bit 15 força o bit 31 e o IRQ3
    CHECK((r.irq.status() & (1u << 3)) != 0);
    r.write(dicr, 0xFFFFFFFF);
    CHECK_EQ(r.read(dicr), u32{0x80FF803F}); // bit 6 lê 0 (medido); flags não existem
}

TEST_CASE("dma/OTC escreve a tabela de ordenação para baixo") {
    DmaRig r;
    r.enable(6);
    r.write(madr(6), 0x80001000);
    r.write(bcr(6), 4);
    r.write(chcr(6), 0x11000002);
    CHECK_EQ(r.ram(0x1000), u32{0x00000FFC});
    CHECK_EQ(r.ram(0x0FFC), u32{0x00000FF8});
    CHECK_EQ(r.ram(0x0FF8), u32{0x00000FF4});
    CHECK_EQ(r.ram(0x0FF4), u32{0x00FFFFFF}); // última entrada: marcador de fim
    CHECK_EQ(r.read(madr(6)), u32{0x00001000}); // MADR e BCR não mudam (medido)
    CHECK_EQ(r.read(bcr(6)), u32{4});
    CHECK_EQ(r.read(chcr(6)), u32{0x01000002}); // ocupado; bit 28 limpo ao começar
    r.advance_to(4);
    CHECK_EQ(r.read(chcr(6)), u32{0x00000002}); // concluído
}

TEST_CASE("dma/OTC exige o bit 28 e a habilitação no DPCR") {
    {
        DmaRig r;
        r.enable(6);
        r.poke(0x2000, 0x12345678);
        r.write(madr(6), 0x2000);
        r.write(bcr(6), 1);
        r.write(chcr(6), 0x01000002); // sem bit 28: não começa
        r.advance_to(10'000);
        CHECK_EQ(r.ram(0x2000), u32{0x12345678});
        CHECK_EQ(r.read(chcr(6)), u32{0x01000002});
    }
    {
        DmaRig r; // DPCR desligado: nada acontece; habilitar depois inicia o canal armado
        r.poke(0x2000, 0x12345678);
        r.write(madr(6), 0x2000);
        r.write(bcr(6), 1);
        r.write(chcr(6), 0x11000002);
        CHECK_EQ(r.ram(0x2000), u32{0x12345678});
        r.enable(6);
        CHECK_EQ(r.ram(0x2000), u32{0x00FFFFFF});
    }
}

TEST_CASE("dma/OTC com BCR 0 escreve 65536 entradas") {
    DmaRig r;
    r.enable(6);
    r.write(madr(6), 0x40000);
    r.write(bcr(6), 0);
    r.write(chcr(6), 0x11000002);
    CHECK_EQ(r.ram(0x40000), u32{0x3FFFC});
    CHECK_EQ(r.ram(0x8), u32{0x4});
    CHECK_EQ(r.ram(0x4), u32{0x00FFFFFF});
    r.advance_to(65'535);
    CHECK(r.dma.active(6));
    r.advance_to(65'536); // medido: 69 586 ciclos com o custo da leitura
    CHECK(!r.dma.active(6));
}

TEST_CASE("dma/OTC passando do endereço 0 não grava fora da RAM (medido)") {
    DmaRig r;
    r.enable(6);
    r.poke(0x1FFFFC, 0xDEADBEEF);
    r.write(madr(6), 4);
    r.write(bcr(6), 3);
    r.write(chcr(6), 0x11000002);
    CHECK_EQ(r.ram(4), u32{0});
    CHECK_EQ(r.ram(0), u32{0x00FFFFFC});
    CHECK_EQ(r.ram(0x1FFFFC), u32{0xDEADBEEF});
    // ⚠️ Escolha (psx-spx): o acesso fora da RAM liga o bit 15 do DICR.
    CHECK((r.read(dicr) & 0x8000u) != 0);
}

TEST_CASE("dma/flag do DICR liga com a habilitação do canal, sem o bit 23 (medido)") {
    struct Case {
        u32 dicr_before;
        u32 dicr_after;
        bool irq;
    };
    const Case cases[] = {
        {0x00400000, 0x40400000, false}, // só a habilitação do canal 6
        {0x00C00000, 0xC0C00000, true},  // habilitação + bit 23
        {0x00800000, 0x00800000, false}, // só o bit 23
    };
    for (const auto& c : cases) {
        DmaRig r;
        r.enable(6);
        r.write(dicr, c.dicr_before);
        r.write(madr(6), 0x1000);
        r.write(bcr(6), 1);
        r.write(chcr(6), 0x11000002);
        r.advance_to(100);
        CHECK_EQ(r.read(dicr), c.dicr_after);
        CHECK_EQ((r.irq.status() & (1u << 3)) != 0, c.irq);
    }
}

TEST_CASE("dma/IRQ3 só na borda do bit 31; ligar o bit 23 depois também dispara") {
    DmaRig r;
    r.enable(6);
    r.write(dicr, 0x00400000);
    r.write(madr(6), 0x1000);
    r.write(bcr(6), 1);
    r.write(chcr(6), 0x11000002);
    r.advance_to(100);
    CHECK_EQ(r.irq.status() & (1u << 3), u32{0});
    r.write(dicr, 0x00C00000); // liga o bit 23 com o flag já presente
    CHECK((r.irq.status() & (1u << 3)) != 0);
    r.irq.io_write(0, ~(1u << 3), AccessWidth::Word);
    // Segundo término com o bit 31 já ligado: sem nova borda, sem IRQ.
    r.write(chcr(6), 0x11000002);
    r.advance_to(200);
    CHECK_EQ(r.irq.status() & (1u << 3), u32{0});
    // Escrever 1 no flag o limpa e derruba o bit 31.
    r.write(dicr, 0x40C00000);
    CHECK_EQ(r.read(dicr), u32{0x00C00000});
}

TEST_CASE("dma/burst RAM → dispositivo e dispositivo → RAM") {
    {
        DmaRig r;
        TestPort port;
        r.dma.attach_port(Channel::Gpu, port);
        r.enable(2);
        for (u32 i = 0; i < 4; ++i) r.poke(0x3000 + 4 * i, 0x100 + i);
        r.write(madr(2), 0x3000);
        r.write(bcr(2), 4);
        r.write(chcr(2), 0x11000001); // burst, para o dispositivo, +4
        CHECK(port.written == (std::vector<u32>{0x100, 0x101, 0x102, 0x103}));
        CHECK_EQ(r.read(madr(2)), u32{0x3000}); // sem chopping: MADR/BCR inalterados
        CHECK_EQ(r.read(bcr(2)), u32{4});
    }
    {
        DmaRig r;
        TestPort port;
        r.dma.attach_port(Channel::Gpu, port);
        r.enable(2);
        for (u32 i = 0; i < 3; ++i) r.poke(0x3008 - 4 * i, 0x200 + i);
        r.write(madr(2), 0x3008);
        r.write(bcr(2), 3);
        r.write(chcr(2), 0x11000003); // passo −4
        CHECK(port.written == (std::vector<u32>{0x200, 0x201, 0x202}));
    }
    {
        DmaRig r;
        TestPort port;
        r.dma.attach_port(Channel::Cdrom, port);
        r.enable(3);
        r.write(madr(3), 0x4000);
        r.write(bcr(3), 0x00010003);
        r.write(chcr(3), 0x11000100); // dispositivo → RAM, com chopping
        CHECK_EQ(r.ram(0x4000), u32{0xA0000000});
        CHECK_EQ(r.ram(0x4008), u32{0xA0000002});
        CHECK_EQ(r.read(madr(3)), u32{0x400C}); // com chopping MADR avança e BCR zera (medido)
        CHECK_EQ(r.read(bcr(3)), u32{0x00010000});
        r.advance_to(3 * DmaController::cycles_per_word(3));
        CHECK(!r.dma.active(3));
    }
}

TEST_CASE("dma/burst sem bit 28 espera o DREQ do dispositivo") {
    DmaRig r;
    TestPort port;
    port.dreq = false;
    r.dma.attach_port(Channel::Gpu, port);
    r.enable(2);
    r.write(madr(2), 0x3000);
    r.write(bcr(2), 2);
    r.write(chcr(2), 0x01000001);
    r.advance_to(10'000);
    CHECK(port.written.empty());
    CHECK((r.read(chcr(2)) & (1u << 24)) != 0); // continua ocupado (medido)
    port.dreq = true;
    r.dma.request_changed(Channel::Gpu);
    CHECK_EQ(port.written.size(), usize{2});
}

TEST_CASE("dma/slice: BS × BA palavras, MADR no fim, BA chega a 0, pausa sem DREQ") {
    {
        DmaRig r;
        TestPort port;
        r.dma.attach_port(Channel::Spu, port);
        r.enable(4);
        r.write(madr(4), 0x5000);
        r.write(bcr(4), 0x00030004); // 3 blocos de 4
        r.write(chcr(4), 0x01000201);
        CHECK_EQ(port.written.size(), usize{12});
        CHECK_EQ(r.read(madr(4)), u32{0x5030}); // início + BS × BA × 4 (medido)
        CHECK_EQ(r.read(bcr(4)), u32{0x00000004}); // BA = 0, BS mantido (medido)
    }
    {
        DmaRig r;
        LimitedPort port;
        port.budget = 4; // um bloco
        r.dma.attach_port(Channel::Spu, port);
        r.enable(4);
        r.write(madr(4), 0x5000);
        r.write(bcr(4), 0x00030004);
        r.write(chcr(4), 0x01000201);
        CHECK_EQ(port.written.size(), usize{4});
        CHECK_EQ(r.read(bcr(4)), u32{0x00020004});
        CHECK_EQ(r.read(madr(4)), u32{0x5010});
        CHECK(r.dma.active(4));
        port.budget = 100;
        r.dma.request_changed(Channel::Spu);
        CHECK_EQ(port.written.size(), usize{12});
        r.advance_to(1'000);
        CHECK(!r.dma.active(4));
    }
}

TEST_CASE("dma/lista encadeada: cabeçalhos, nós vazios, fim em FFFFFFh") {
    DmaRig r;
    TestPort port;
    r.dma.attach_port(Channel::Gpu, port);
    r.enable(2);
    // Nó A (2 palavras) → B (vazio) → C (3 palavras) → fim.
    r.poke(0x6000, 0x02006100);
    r.poke(0x6004, 0xA1);
    r.poke(0x6008, 0xA2);
    r.poke(0x6100, 0x00006200);
    r.poke(0x6200, 0x03FFFFFF);
    r.poke(0x6204, 0xC1);
    r.poke(0x6208, 0xC2);
    r.poke(0x620C, 0xC3);
    r.write(madr(2), 0x6000);
    r.write(bcr(2), 0x00050010); // ignorado no modo lista (medido)
    r.write(chcr(2), 0x01000401);
    CHECK(port.written == (std::vector<u32>{0xA1, 0xA2, 0xC1, 0xC2, 0xC3}));
    CHECK_EQ(r.read(madr(2)), u32{0x00FFFFFF});
    CHECK_EQ(r.read(bcr(2)), u32{0x00050010});
    // Duração: 3 nós × 10 + 5 palavras (medido ≈10 por nó e ≈1 por palavra).
    r.advance_to(34);
    CHECK(r.dma.active(2));
    r.advance_to(35);
    CHECK(!r.dma.active(2));
}

TEST_CASE("dma/lista terminada em 800000h gera erro de barramento e IRQ3") {
    DmaRig r;
    TestPort port;
    r.dma.attach_port(Channel::Gpu, port);
    r.enable(2);
    r.poke(0x6000, 0x00800000);
    r.write(madr(2), 0x6000);
    r.write(chcr(2), 0x01000401);
    r.advance_to(100);
    CHECK_EQ(r.read(madr(2)), u32{0x00800000});
    CHECK_EQ(r.read(dicr) & 0x80008000u, u32{0x80008000});
    CHECK((r.irq.status() & (1u << 3)) != 0);
}

TEST_CASE("dma/lista que aponta para si mesma nunca termina e não trava") {
    DmaRig r;
    TestPort port;
    r.dma.attach_port(Channel::Gpu, port);
    r.enable(2);
    r.poke(0x7000, 0x01007000); // 1 palavra e volta para o próprio nó
    r.poke(0x7004, 0x55);
    r.write(madr(2), 0x7000);
    r.write(chcr(2), 0x01000401);
    r.advance_to(1'000'000);
    CHECK((r.read(chcr(2)) & (1u << 24)) != 0); // medido: ocupado para sempre, sem IRQ
    CHECK_EQ(r.irq.status() & (1u << 3), u32{0});
    CHECK(port.written.size() > 1000);
    CHECK(r.dma_events < 2000); // processada em fatias, não nó a nó
    r.write(chcr(2), 0x00000401); // zerar o bit 24 para a lista
    const usize count = port.written.size();
    r.advance_to(2'000'000);
    CHECK_EQ(port.written.size(), count);
}

TEST_CASE("dma/prioridade: o canal de menor prioridade roda depois (medido)") {
    for (bool otc_first : {true, false}) {
        DmaRig r;
        TestPort port;
        port.next_read = 0x33333333;
        r.dma.attach_port(Channel::Cdrom, port);
        r.write(madr(6), 0x8008);
        r.write(bcr(6), 3);
        r.write(chcr(6), 0x11000002);
        r.write(madr(3), 0x8000);
        r.write(bcr(3), 3);
        r.write(chcr(3), 0x11000000);
        // Habilita os dois de uma vez; prioridade 0 é a maior.
        const u32 p6 = otc_first ? 0u : 5u;
        const u32 p3 = otc_first ? 5u : 0u;
        r.write(dpcr, (0x8u | p6) << 24 | (0x8u | p3) << 12);
        if (otc_first) {
            CHECK_EQ(r.ram(0x8000), u32{0x33333333}); // o CD-ROM (depois) sobrescreve
        } else {
            CHECK_EQ(r.ram(0x8000), u32{0x00FFFFFF}); // o OTC (depois) sobrescreve
        }
    }
}

TEST_CASE("dma/canal sem dispositivo avisa uma vez, citando a fase") {
    auto sink = std::make_shared<log::MemorySink>();
    const auto previous = log::Logger::instance().level();
    log::Logger::instance().set_level(log::Level::Warn);
    log::Logger::instance().add_sink(sink);
    {
        DmaRig r;
        r.enable(2);
        r.write(madr(2), 0x3000);
        r.write(bcr(2), 8);
        r.write(chcr(2), 0x11000001);
        r.write(chcr(2), 0x11000001);
        r.advance_to(100);
    }
    log::Logger::instance().remove_sink(sink);
    log::Logger::instance().set_level(previous);
    REQUIRE_EQ(sink->size(), usize{1});
    CHECK(sink->records()[0].message.find("GPU") != std::string::npos);
    CHECK(sink->records()[0].message.find("Fase 7") != std::string::npos);
}

TEST_CASE("console/CPU programa o OTC, espera o fim e recebe IRQ3") {
    using namespace rpcs1::test::asm_;
    system::Console c;
    const std::vector<u32> program = {
        lui(t0, 0x1F80),
        lui(t1, 0x0800), sw(t1, 0x10F0, t0),        // DPCR: só o canal 6
        lui(t1, 0x00C0), sw(t1, 0x10F4, t0),        // DICR: bit 23 + canal 6
        ori(t1, zero, 0x1010), sw(t1, 0x10E0, t0),  // MADR6 = 1010h
        addiu(t1, zero, 5), sw(t1, 0x10E4, t0),     // BCR6 = 5
        lui(t1, 0x1100), ori(t1, t1, 0x0002), sw(t1, 0x10E8, t0), // CHCR6 = 11000002h
        lw(t2, 0x10E8, t0),                         // espera: bit 24
        nop(),
        srl(t2, t2, 24),
        andi(t2, t2, 1),
        bne(t2, zero, -5),
        nop(),
        break_(),
    };
    for (usize i = 0; i < program.size(); ++i) {
        REQUIRE_EQ(c.bus().write32(0x80010000u + static_cast<u32>(i * 4), program[i]),
                   bus::BusFault::None);
    }
    c.cpu().cop0().write(cpu::cop0reg::sr, 0);
    c.cpu().set_pc(0x80010000);
    c.run(200);
    REQUIRE(c.cpu().last_exception().has_value());
    CHECK(c.cpu().last_exception()->code == cpu::ExceptionCode::Breakpoint);
    CHECK_EQ(c.bus().read32(0x1010).value, u32{0x100C});
    CHECK_EQ(c.bus().read32(0x1000).value, u32{0x00FFFFFF});
    CHECK((c.interrupts().status() & (1u << 3)) != 0);
    CHECK_EQ(c.dma().dicr(), u32{0xC0C00000});
}
