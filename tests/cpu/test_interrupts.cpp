#include <vector>

#include "cpu_harness.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/console.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::ExceptionCode;
using cpu::StepResult;
using interrupts::InterruptController;
using interrupts::Irq;
namespace sr = rpcs1::cpu::sr;

namespace {

constexpr u32 base = Machine::code_base;
constexpr u32 vector = Machine::exception_vector;
constexpr u32 im_hw = 1u << 10; // SR.Im bit da interrupção de hardware

class FakeLine final : public cpu::InterruptLine {
public:
    bool irq_asserted() const noexcept override { return asserted; }
    bool asserted = false;
};

} // namespace

TEST_CASE("irq/interrupção de hardware é aceita antes da próxima instrução") {
    Machine m({addiu(t0, zero, 1), addiu(t1, zero, 2)});
    m.load({addiu(t2, zero, 3)}, vector);
    FakeLine line;
    m.cpu.set_interrupt_line(&line);
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::iec | im_hw);

    m.steps(1);
    CHECK_EQ(m.reg(t0), u32{1});
    line.asserted = true;
    CHECK(m.cpu.step() == StepResult::Exception);
    REQUIRE(m.cpu.last_exception().has_value());
    CHECK(m.cpu.last_exception()->code == ExceptionCode::Interrupt);
    CHECK_EQ(m.reg(t1), u32{0});                 // não executou
    CHECK_EQ(m.cpu.cop0().epc(), base + 4);      // retorna para ela
    CHECK_EQ(m.cpu.cop0().cause(true) & 0x7Cu, u32{0}); // ExcCode = 0
    CHECK((m.cpu.cop0().cause(true) & (1u << 10)) != 0);
    CHECK_EQ(m.cpu.cop0().status() & 0x3Fu, u32{0x04}); // IEc empilhado e desligado
    CHECK(m.cpu.step() == StepResult::Executed);  // tratador roda com IEc = 0
    CHECK_EQ(m.reg(t2), u32{3});
}

TEST_CASE("irq/sem IEc ou sem o bit de máscara a interrupção é ignorada") {
    for (u32 status : {im_hw, sr::iec, sr::iec | (1u << 8)}) {
        Machine m({addiu(t0, zero, 1), addiu(t1, zero, 2)});
        FakeLine line;
        line.asserted = true;
        m.cpu.set_interrupt_line(&line);
        m.cpu.cop0().write(cpu::cop0reg::sr, status);
        m.steps(2);
        CHECK_EQ(m.reg(t1), u32{2});
    }
}

TEST_CASE("irq/interrupção de software pelos bits 8–9 de CAUSE") {
    Machine m({ori(t0, zero, 0x100), mtc0(t0, cpu::cop0reg::cause), addiu(t1, zero, 1), nop()});
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::iec | (1u << 8));
    m.steps(2);
    CHECK(m.cpu.step() == StepResult::Exception);
    CHECK(m.cpu.last_exception()->code == ExceptionCode::Interrupt);
    CHECK_EQ(m.reg(t1), u32{0});
    CHECK_EQ(m.cpu.cop0().epc(), base + 8);
    CHECK_EQ(m.cpu.cop0().cause(false) & 0x300u, u32{0x100}); // bit de software preservado
}

TEST_CASE("irq/interrupção em delay slot aponta EPC para o desvio") {
    Machine m({beq(zero, zero, 2), nop(), nop(), nop()});
    FakeLine line;
    m.cpu.set_interrupt_line(&line);
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::iec | im_hw);
    m.steps(1);
    line.asserted = true;
    CHECK(m.cpu.step() == StepResult::Exception);
    CHECK_EQ(m.cpu.cop0().epc(), base); // o desvio será reexecutado
    CHECK((m.cpu.cop0().cause(true) >> 31) == 1u);
    CHECK(((m.cpu.cop0().cause(true) >> 30) & 1u) == 1u);
}

TEST_CASE("irq/interrupção logo após uma carga completa a carga") {
    Machine m({lw(t0, 0, t9), addu(t1, t0, zero)});
    m.cpu.set_gpr(t9, Machine::data_base);
    m.poke32(Machine::data_base, 0xABCD);
    FakeLine line;
    m.cpu.set_interrupt_line(&line);
    m.cpu.cop0().write(cpu::cop0reg::sr, sr::iec | im_hw);
    m.steps(1);
    line.asserted = true;
    CHECK(m.cpu.step() == StepResult::Exception);
    CHECK_EQ(m.reg(t0), u32{0xABCD});
}

TEST_CASE("irq/CAUSE bit 10 acompanha a linha (não é latch)") {
    cpu::Cop0 c;
    CHECK((c.cause(true) & (1u << 10)) != 0);
    CHECK((c.cause(false) & (1u << 10)) == 0);
}

TEST_CASE("irq_controller/pedidos, máscara e reconhecimento") {
    InterruptController ic;
    ic.request(Irq::VBlank);
    ic.request(Irq::Cdrom);
    CHECK_EQ(ic.status(), u32{0x5});
    CHECK(!ic.irq_asserted()); // máscara zerada
    ic.io_write(4, 0x4, bus::AccessWidth::Word);
    CHECK(ic.irq_asserted());
    ic.io_write(0, ~0x4u, bus::AccessWidth::Word); // reconhece só o CDROM
    CHECK_EQ(ic.status(), u32{0x1});
    CHECK(!ic.irq_asserted());
    ic.io_write(0, 0xFFFFFFFF, bus::AccessWidth::Word); // 1 mantém
    CHECK_EQ(ic.status(), u32{0x1});
    ic.io_write(4, 0xFFFFFFFF, bus::AccessWidth::Word);
    CHECK_EQ(ic.mask(), u32{0x7FF});
    ic.io_write(0, 0, bus::AccessWidth::Word);
    CHECK_EQ(ic.status(), u32{0});
    ic.request(Irq::Lightpen);
    CHECK_EQ(ic.io_read(1, bus::AccessWidth::Byte), u32{0x04}); // bit 10 no byte 1
    ic.reset();
    CHECK_EQ(ic.status(), u32{0});
    CHECK_EQ(ic.mask(), u32{0});
}

TEST_CASE("console/controlador de interrupções ligado ao barramento e à CPU") {
    system::Console c;
    CHECK(dynamic_cast<InterruptController*>(&c.bus().device(bus::IoSlot::Interrupts)) != nullptr);
    c.interrupts().request(Irq::Timer0);
    CHECK_EQ(c.bus().read32(0x1F801070).value, u32{0x10});
    CHECK(!c.cpu().irq_line());
    REQUIRE_EQ(c.bus().write32(0xBF801074, 0x10), bus::BusFault::None);
    CHECK_EQ(c.bus().read16(0x1F801074).value, u32{0x10});
    CHECK(c.cpu().irq_line());
}

TEST_CASE("console/IRQ ponta a ponta: pedido, tratador e reconhecimento") {
    system::Console c;
    auto& b = c.bus();
    // Programa principal: laço infinito de nops.
    for (u32 i = 0; i < 4; ++i) {
        REQUIRE_EQ(b.write32(base + i * 4, nop()), bus::BusFault::None);
    }
    REQUIRE_EQ(b.write32(base + 16, j(base)), bus::BusFault::None);
    // Tratador: marca, reconhece I_STAT e para num BREAK.
    const std::vector<u32> handler = {addiu(s0, zero, 1), lui(t8, 0x1F80),
                                      sw(zero, 0x1070, t8), break_()};
    for (usize i = 0; i < handler.size(); ++i) {
        REQUIRE_EQ(b.write32(vector + static_cast<u32>(i * 4), handler[i]), bus::BusFault::None);
    }
    c.cpu().cop0().write(cpu::cop0reg::sr, sr::iec | im_hw);
    c.cpu().set_pc(base);
    REQUIRE_EQ(b.write32(0x1F801074, 1u << static_cast<u32>(Irq::VBlank)), bus::BusFault::None);

    for (int i = 0; i < 3; ++i) {
        CHECK(c.cpu().step() == StepResult::Executed);
    }
    c.interrupts().request(Irq::VBlank);
    CHECK(c.cpu().step() == StepResult::Exception); // interrupção
    CHECK(c.cpu().last_exception()->code == ExceptionCode::Interrupt);
    CHECK_EQ(c.cpu().cop0().epc(), base + 12);
    for (int i = 0; i < 3; ++i) {
        CHECK(c.cpu().step() == StepResult::Executed);
    }
    CHECK_EQ(c.cpu().gpr(s0), u32{1});
    CHECK_EQ(c.interrupts().status(), u32{0}); // reconhecido pelo tratador
    CHECK(!c.cpu().irq_line());
}

TEST_CASE("console/power_on zera; reset preserva a RAM") {
    system::Console c;
    REQUIRE_EQ(c.bus().write32(0x1000, 0xCAFE), bus::BusFault::None);
    c.cpu().cop0().write(cpu::cop0reg::sr, 0);
    c.interrupts().request(Irq::Spu);
    c.reset();
    CHECK_EQ(c.bus().read32(0x1000).value, u32{0xCAFE});
    CHECK_EQ(c.cpu().cop0().status(), cpu::Cop0::sr_reset);
    CHECK_EQ(c.interrupts().status(), u32{0});
    CHECK_EQ(c.cpu().pc(), u32{0xBFC00000});
    c.power_on();
    CHECK_EQ(c.bus().read32(0x1000).value, u32{0});
}
