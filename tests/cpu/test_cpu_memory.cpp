#include <vector>

#include "cpu_harness.hpp"

using namespace rpcs1;
using namespace rpcs1::test;
using namespace rpcs1::test::asm_;
using cpu::ExceptionCode;
using cpu::StepResult;

namespace {

constexpr u32 data = Machine::data_base;

class RecordingDevice final : public bus::IoDevice {
public:
    std::string_view name() const noexcept override { return "teste"; }
    u32 io_read(u32, bus::AccessWidth) override { return 0; }
    void io_write(u32 offset, u32 value, bus::AccessWidth width) override {
        last_offset = offset;
        last_value = value;
        last_width = width;
    }
    u32 last_offset = 0;
    u32 last_value = 0;
    bus::AccessWidth last_width = bus::AccessWidth::Word;
};

} // namespace

TEST_CASE("cpu/LB, LBU, LH, LHU e LW com extensão de sinal/zero") {
    Machine m({lb(t0, 0, t9), lbu(t1, 0, t9), lh(t2, 0, t9), lhu(t3, 2, t9), lw(t4, 0, t9),
               lb(t5, 2, t9), lh(t6, 2, t9), nop()});
    m.cpu.set_gpr(t9, data);
    m.poke32(data, 0x8001FF80);
    m.steps(8);
    CHECK_EQ(m.reg(t0), u32{0xFFFFFF80});
    CHECK_EQ(m.reg(t1), u32{0x00000080});
    CHECK_EQ(m.reg(t2), u32{0xFFFFFF80});
    CHECK_EQ(m.reg(t3), u32{0x00008001});
    CHECK_EQ(m.reg(t4), u32{0x8001FF80});
    CHECK_EQ(m.reg(t5), u32{0x00000001});
    CHECK_EQ(m.reg(t6), u32{0xFFFF8001});
}

TEST_CASE("cpu/SB, SH e SW") {
    Machine m({sw(t0, 0, t9), sb(t1, 1, t9), sh(t2, 2, t9), sw(t0, -4, t9)});
    m.cpu.set_gpr(t9, data + 8);
    m.cpu.set_gpr(t0, 0x11223344);
    m.cpu.set_gpr(t1, 0xAABBCCDD);
    m.cpu.set_gpr(t2, 0x99995566);
    m.steps(4);
    CHECK_EQ(m.word(data + 8), u32{0x5566DD44});
    CHECK_EQ(m.word(data + 4), u32{0x11223344});
}

TEST_CASE("cpu/SH em E/S entrega o registrador inteiro ao dispositivo") {
    Machine m({sh(t0, 4, t9), sb(t0, 9, t9)});
    RecordingDevice dev;
    m.bus.attach_device(bus::IoSlot::Timers, dev);
    m.cpu.set_gpr(t9, 0x1F801100);
    m.cpu.set_gpr(t0, 0xDEADBEEF);
    m.steps(1);
    CHECK_EQ(dev.last_offset, u32{4});
    CHECK_EQ(dev.last_value, u32{0xDEADBEEF});
    CHECK(dev.last_width == bus::AccessWidth::Half);
    m.steps(1);
    CHECK_EQ(dev.last_offset, u32{9});
    CHECK_EQ(dev.last_value, u32{0xDEADBEEF});
    CHECK(dev.last_width == bus::AccessWidth::Byte);
}

TEST_CASE("cpu/load delay: a instrução seguinte vê o valor antigo") {
    Machine m({lw(t0, 0, t9), addu(t1, t0, zero), addu(t2, t0, zero)});
    m.cpu.set_gpr(t9, data);
    m.cpu.set_gpr(t0, 0xAAAA);
    m.poke32(data, 0x1234);
    m.steps(1);
    CHECK_EQ(m.reg(t0), u32{0xAAAA}); // ainda não visível
    m.steps(1);
    CHECK_EQ(m.reg(t1), u32{0xAAAA}); // delay slot leu o valor antigo
    CHECK_EQ(m.reg(t0), u32{0x1234}); // completa ao fim do delay slot
    m.steps(1);
    CHECK_EQ(m.reg(t2), u32{0x1234});
}

TEST_CASE("cpu/load delay: escrita no delay slot prevalece sobre a carga") {
    Machine m({lw(t0, 0, t9), addiu(t0, zero, 7), nop(), nop()});
    m.cpu.set_gpr(t9, data);
    m.poke32(data, 0x1234);
    m.steps(4);
    CHECK_EQ(m.reg(t0), u32{7});
}

TEST_CASE("cpu/load delay: duas cargas seguidas no mesmo registrador (modelo de pipeline)") {
    // ⚠️ Não documentado no psx-spx; derivado do pipeline de 5 estágios (D-019).
    Machine m({lw(t0, 0, t9), lw(t0, 4, t9), addu(t1, t0, zero), addu(t2, t0, zero)});
    m.cpu.set_gpr(t9, data);
    m.poke32(data, 0x1111);
    m.poke32(data + 4, 0x2222);
    m.steps(4);
    CHECK_EQ(m.reg(t1), u32{0x1111});
    CHECK_EQ(m.reg(t2), u32{0x2222});
}

TEST_CASE("cpu/store no load delay slot grava o valor antigo") {
    Machine m({lw(t0, 0, t9), sw(t0, 8, t9), sw(t0, 12, t9)});
    m.cpu.set_gpr(t9, data);
    m.cpu.set_gpr(t0, 0x5555);
    m.poke32(data, 0x9999);
    m.steps(3);
    CHECK_EQ(m.word(data + 8), u32{0x5555});
    CHECK_EQ(m.word(data + 12), u32{0x9999});
}

TEST_CASE("cpu/LWL e LWR em cada deslocamento") {
    // Bytes na memória: 11 22 33 44 (palavra 44332211h); rt inicial AABBCCDDh.
    struct Case {
        bool left;
        s32 offset;
        u32 expected;
    };
    const Case cases[] = {
        {true, 0, 0x11BBCCDD},  {true, 1, 0x2211CCDD},  {true, 2, 0x332211DD},
        {true, 3, 0x44332211},  {false, 0, 0x44332211}, {false, 1, 0xAA443322},
        {false, 2, 0xAABB4433}, {false, 3, 0xAABBCC44},
    };
    for (const auto& c : cases) {
        Machine m({c.left ? lwl(t0, c.offset, t9) : lwr(t0, c.offset, t9), nop()});
        m.cpu.set_gpr(t9, data);
        m.cpu.set_gpr(t0, 0xAABBCCDD);
        m.poke32(data, 0x44332211);
        m.steps(2);
        CHECK_EQ(m.reg(t0), c.expected);
    }
}

TEST_CASE("cpu/LWR + LWL carregam palavra desalinhada sem nop entre elas") {
    Machine m({lwr(t0, 1, t9), lwl(t0, 4, t9), addu(t1, t0, zero), addu(t2, t0, zero)});
    m.cpu.set_gpr(t9, data);
    m.cpu.set_gpr(t0, 0xAAAAAAAA);
    m.poke32(data, 0x33221100);     // bytes 00 11 22 33
    m.poke32(data + 4, 0x77665544); // bytes 44 55 66 77
    m.steps(4);
    CHECK_EQ(m.reg(t1), u32{0xAA332211}); // delay slot do LWL vê o resultado do LWR
    CHECK_EQ(m.reg(t2), u32{0x44332211}); // palavra desalinhada em data+1
}

TEST_CASE("cpu/SWL e SWR em cada deslocamento") {
    // Memória inicial 44332211h (bytes 11 22 33 44); rt = AABBCCDDh.
    struct Case {
        bool left;
        s32 offset;
        u32 expected;
    };
    const Case cases[] = {
        {true, 0, 0x443322AA},  {true, 1, 0x4433AABB},  {true, 2, 0x44AABBCC},
        {true, 3, 0xAABBCCDD},  {false, 0, 0xAABBCCDD}, {false, 1, 0xBBCCDD11},
        {false, 2, 0xCCDD2211}, {false, 3, 0xDD332211},
    };
    for (const auto& c : cases) {
        Machine m({c.left ? swl(t0, c.offset, t9) : swr(t0, c.offset, t9)});
        m.cpu.set_gpr(t9, data);
        m.cpu.set_gpr(t0, 0xAABBCCDD);
        m.poke32(data, 0x44332211);
        m.steps(1);
        CHECK_EQ(m.word(data), c.expected);
    }
}

TEST_CASE("cpu/SWR + SWL gravam palavra desalinhada") {
    Machine m({swr(t0, 1, t9), swl(t0, 4, t9)});
    m.cpu.set_gpr(t9, data);
    m.cpu.set_gpr(t0, 0x44332211);
    m.steps(2);
    CHECK_EQ(m.word(data), u32{0x33221100});
    CHECK_EQ(m.word(data + 4), u32{0x00000044});
}

TEST_CASE("cpu/acessos desalinhados geram AdEL/AdES com BadVaddr") {
    struct Case {
        u32 instruction;
        ExceptionCode code;
        u32 offset;
    };
    const Case cases[] = {
        {lh(t0, 1, t9), ExceptionCode::AddressErrorLoad, 1},
        {lhu(t0, 3, t9), ExceptionCode::AddressErrorLoad, 3},
        {lw(t0, 2, t9), ExceptionCode::AddressErrorLoad, 2},
        {sh(t1, 1, t9), ExceptionCode::AddressErrorStore, 1},
        {sw(t1, 3, t9), ExceptionCode::AddressErrorStore, 3},
    };
    for (const auto& c : cases) {
        Machine m({c.instruction});
        m.cpu.set_gpr(t9, data);
        m.cpu.set_gpr(t0, 0x0BAD);
        m.cpu.set_gpr(t1, 0xFFFFFFFF);
        m.poke32(data, 0x12345678);
        CHECK(m.cpu.step() == StepResult::Exception);
        REQUIRE(m.cpu.last_exception().has_value());
        CHECK(m.cpu.last_exception()->code == c.code);
        CHECK_EQ(m.cpu.last_exception()->bad_vaddr, std::optional<u32>(data + c.offset));
        CHECK_EQ(m.reg(t0), u32{0x0BAD});        // destino intocado
        CHECK_EQ(m.word(data), u32{0x12345678}); // memória intocada
    }
    // Bytes nunca geram erro de alinhamento.
    Machine m({lb(t0, 3, t9), sb(t0, 1, t9)});
    m.cpu.set_gpr(t9, data);
    m.steps(2);
}

TEST_CASE("cpu/erro de barramento em dados (DBE) não define BadVaddr") {
    for (u32 instruction : {lw(t0, 0, t9), sw(t0, 0, t9), lb(t0, 0, t9), lwl(t0, 3, t9)}) {
        Machine m({instruction});
        m.cpu.set_gpr(t9, 0x1F801024); // lacuna na área de E/S
        CHECK(m.cpu.step() == StepResult::Exception);
        REQUIRE(m.cpu.last_exception().has_value());
        CHECK(m.cpu.last_exception()->code == ExceptionCode::BusErrorData);
        CHECK(!m.cpu.last_exception()->bad_vaddr.has_value());
    }
}

TEST_CASE("cpu/exceção logo após uma carga ainda completa a carga") {
    Machine m({lw(t0, 0, t9), lw(t1, 2, t9)});
    m.cpu.set_gpr(t9, data);
    m.poke32(data, 0x1234);
    m.steps(1);
    CHECK(m.cpu.step() == StepResult::Exception);
    CHECK_EQ(m.reg(t0), u32{0x1234});
}
