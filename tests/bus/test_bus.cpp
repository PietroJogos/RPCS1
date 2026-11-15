#include <memory>
#include <stdexcept>
#include <vector>

#include "bus/bus.hpp"
#include "core/log.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;
using namespace rpcs1::bus;

namespace {

constexpr u32 MiB_ = 1024 * 1024;

// Barramento com uma BIOS sintética (padrão de bytes conhecido, não é uma BIOS real).
std::unique_ptr<Bus> make_bus_with_bios() {
    auto b = std::make_unique<Bus>();
    std::vector<u8> image(memory::BiosRom::size);
    for (usize i = 0; i < image.size(); ++i) {
        image[i] = static_cast<u8>(i ^ (i >> 8) ^ (i >> 16));
    }
    // Primeira palavra no vetor de reset.
    image[0] = 0xEF;
    image[1] = 0xBE;
    image[2] = 0xAD;
    image[3] = 0xDE;
    if (!b->bios().load(image).ok()) {
        throw std::runtime_error("falha ao carregar BIOS sintética");
    }
    return b;
}

bool is_bus_error(const ReadResult& r) {
    return r.fault == BusFault::BusError;
}

// Dispositivo de teste que registra o último acesso.
class RecordingDevice final : public IoDevice {
public:
    std::string_view name() const noexcept override { return "teste"; }
    u32 io_read(u32 offset, AccessWidth width) override {
        last_offset = offset;
        last_width = width;
        ++reads;
        return 0xA1B2C3D4;
    }
    void io_write(u32 offset, u32 value, AccessWidth width) override {
        last_offset = offset;
        last_value = value;
        last_width = width;
        ++writes;
    }
    u32 last_offset = 0;
    u32 last_value = 0;
    AccessWidth last_width = AccessWidth::Word;
    int reads = 0;
    int writes = 0;
};

// Isola o logger global e captura avisos.
struct WarnCapture {
    std::shared_ptr<log::MemorySink> sink = std::make_shared<log::MemorySink>();
    log::Level previous = log::Logger::instance().level();
    WarnCapture() {
        log::Logger::instance().set_level(log::Level::Warn);
        log::Logger::instance().add_sink(sink);
    }
    ~WarnCapture() {
        log::Logger::instance().remove_sink(sink);
        log::Logger::instance().set_level(previous);
    }
};

} // namespace

TEST_CASE("bus/RAM acessível por KUSEG, KSEG0 e KSEG1") {
    Bus b;
    REQUIRE_EQ(b.write32(0x00001000, 0x12345678), BusFault::None);
    CHECK_EQ(b.read32(0x80001000).value, u32{0x12345678});
    CHECK_EQ(b.read32(0xA0001000).value, u32{0x12345678});
    REQUIRE_EQ(b.write8(0xA0001001, 0xAB), BusFault::None);
    CHECK_EQ(b.read32(0x00001000).value, u32{0x1234AB78});
    CHECK_EQ(b.read16(0x80001002).value, u32{0x1234});
    CHECK_EQ(b.read8(0x00001003).value, u32{0x12});
}

TEST_CASE("bus/SB e SH gravam só a parte baixa do registrador na RAM") {
    Bus b;
    REQUIRE_EQ(b.write32(0x100, 0xFFFFFFFF), BusFault::None);
    REQUIRE_EQ(b.write8(0x101, 0x11223344), BusFault::None);
    CHECK_EQ(b.read32(0x100).value, u32{0xFFFF44FF});
    REQUIRE_EQ(b.write16(0x102, 0xAABBCCDD), BusFault::None);
    CHECK_EQ(b.read32(0x100).value, u32{0xCCDD44FF});
}

TEST_CASE("bus/espelhos da RAM com DRAM_CTRL padrão (0B88h)") {
    Bus b;
    REQUIRE_EQ(b.write32(0x00000040, 0xCAFED00D), BusFault::None);
    // 2 MiB de chips numa janela de 8 MiB: espelhos em 2, 4 e 6 MiB.
    CHECK_EQ(b.read32(0x00200040).value, u32{0xCAFED00D});
    CHECK_EQ(b.read32(0x80400040).value, u32{0xCAFED00D});
    CHECK_EQ(b.read32(0xA0600040).value, u32{0xCAFED00D});
    REQUIRE_EQ(b.write32(0x007FFFFC, 0x01020304), BusFault::None);
    CHECK_EQ(b.read32(0x001FFFFC).value, u32{0x01020304});
    // Além dos 8 MiB: sem mapeamento.
    CHECK(is_bus_error(b.read32(0x00800000)));
    CHECK(is_bus_error(b.read8(0x00FFFFFF)));
    CHECK_EQ(b.write32(0x00800000, 1), BusFault::BusError);
}

TEST_CASE("bus/DRAM_CTRL altera a janela de RAM") {
    Bus b;
    // 0888h: 2 MiB, sem espelhos.
    REQUIRE_EQ(b.write32(0x1F801060, 0x0888), BusFault::None);
    CHECK(b.read32(0x001FFFFC).ok());
    CHECK(is_bus_error(b.read32(0x00200000)));
    // 0000h: 1 MiB.
    REQUIRE_EQ(b.write32(0xBF801060, 0x0000), BusFault::None);
    CHECK(b.read32(0x000FFFFC).ok());
    CHECK(is_bus_error(b.read32(0x00100000)));
    // Leitura de volta do registrador pelo barramento.
    CHECK_EQ(b.read32(0x1F801060).value, u32{0});
}

TEST_CASE("bus/banco /RAS1 habilitado não tem chips") {
    WarnCapture capture;
    Bus b;
    REQUIRE_EQ(b.write32(0x1F801060, 0x0F00), BusFault::None); // 8 MiB + 8 MiB
    const auto r = b.read32(0x00800000);
    CHECK(r.ok());
    CHECK_EQ(r.value, u32{0});
    CHECK_EQ(b.write32(0x00800000, 5), BusFault::None);
    CHECK_EQ(b.read32(0x00800000).value, u32{0});
    CHECK(capture.sink->size() >= 1);
}

TEST_CASE("bus/acessos desalinhados geram erro de endereço") {
    Bus b;
    REQUIRE_EQ(b.write32(0x200, 0x11111111), BusFault::None);
    CHECK(b.read16(0x201).fault == BusFault::AddressError);
    CHECK(b.read32(0x202).fault == BusFault::AddressError);
    CHECK(b.read32(0x203).fault == BusFault::AddressError);
    CHECK_EQ(b.write16(0x203, 0), BusFault::AddressError);
    CHECK_EQ(b.write32(0x201, 0), BusFault::AddressError);
    CHECK(b.fetch_instruction(0xBFC00002).fault == BusFault::AddressError);
    // A memória não foi alterada.
    CHECK_EQ(b.read32(0x200).value, u32{0x11111111});
    // O alinhamento é verificado antes do mapeamento (endereço inexistente desalinhado).
    CHECK(b.read32(0x00800001).fault == BusFault::AddressError);
}

TEST_CASE("bus/scratchpad: KUSEG e KSEG0 sim, KSEG1 e execução não") {
    Bus b;
    REQUIRE_EQ(b.write32(0x1F800000, 0x0BADF00D), BusFault::None);
    CHECK_EQ(b.read32(0x9F800000).value, u32{0x0BADF00D});
    REQUIRE_EQ(b.write16(0x9F8003FE, 0x7777), BusFault::None);
    CHECK_EQ(b.read16(0x1F8003FE).value, u32{0x7777});
    CHECK(is_bus_error(b.read32(0xBF800000)));
    CHECK_EQ(b.write32(0xBF800000, 1), BusFault::BusError);
    CHECK(is_bus_error(b.fetch_instruction(0x1F800000)));
    CHECK(is_bus_error(b.fetch_instruction(0x9F800000)));
    // Logo após o 1 KiB.
    CHECK(is_bus_error(b.read32(0x1F800400)));
    CHECK(is_bus_error(b.read8(0x1F800FFF)));
}

TEST_CASE("bus/BIOS ROM: aliases, vetor de reset e somente leitura") {
    auto b = make_bus_with_bios();
    CHECK_EQ(b->fetch_instruction(0xBFC00000).value, u32{0xDEADBEEF});
    CHECK_EQ(b->read32(0x9FC00000).value, u32{0xDEADBEEF});
    CHECK_EQ(b->read32(0x1FC00000).value, u32{0xDEADBEEF});
    CHECK_EQ(b->read8(0xBFC00003).value, u32{0xDE});
    const u32 last = b->read32(0xBFC7FFFC).value;
    CHECK_EQ(last, b->bios().read32(memory::BiosRom::size - 4));

    WarnCapture capture;
    CHECK_EQ(b->write32(0xBFC00000, 0), BusFault::None);
    CHECK_EQ(b->read32(0xBFC00000).value, u32{0xDEADBEEF});
    CHECK(capture.sink->size() >= 1);
}

TEST_CASE("bus/BIOS: janela de 512 KiB por padrão; janela maior espelha") {
    auto b = make_bus_with_bios();
    CHECK(is_bus_error(b->read32(0xBFC80000)));
    CHECK(is_bus_error(b->fetch_instruction(0xBFFFFFFC)));
    // N = 16h → 4 MiB (máximo da região).
    REQUIRE_EQ(b->write32(0x1F801010, 0x0016243F), BusFault::None);
    CHECK_EQ(b->read32(0xBFC80000).value, u32{0xDEADBEEF});
    CHECK_EQ(b->read32(0xBFF80000).value, u32{0xDEADBEEF});
    // N maior que a região é limitado a 4 MiB (o endereço seguinte já é inexistente).
    REQUIRE_EQ(b->write32(0x1F801010, 0x001F243F), BusFault::None);
    CHECK(b->read32(0xBFFFFFFC).ok());
}

TEST_CASE("bus/Expansão 1 sem dispositivo") {
    Bus b;
    CHECK_EQ(b.read8(0x1F000000).value, u32{0xFF});
    CHECK_EQ(b.read16(0x1F000084).value, u32{0xFFFF});
    CHECK_EQ(b.read32(0xBF000080).value, u32{0xFFFFFFFF});
    CHECK_EQ(b.write32(0x1F000000, 0x12345678), BusFault::None);
    CHECK_EQ(b.read32(0x1F000000).value, u32{0xFFFFFFFF});
    // Janela padrão de 512 KiB.
    CHECK(b.read8(0x1F07FFFF).ok());
    CHECK(is_bus_error(b.read8(0x1F080000)));
    CHECK(is_bus_error(b.read8(0x1F7FFFFF)));
}

TEST_CASE("bus/Expansão 2: POST, janela e largura") {
    Bus b;
    CHECK(!b.post_code().has_value());
    REQUIRE_EQ(b.write8(0xBF802041, 0x0000000E), BusFault::None);
    REQUIRE(b.post_code().has_value());
    CHECK_EQ(*b.post_code(), u8{0x0E});
    // Bits 4–7 do POST são zero.
    REQUIRE_EQ(b.write8(0x1F802041, 0xF3), BusFault::None);
    CHECK_EQ(*b.post_code(), u8{0x03});
    CHECK_EQ(b.read8(0x1F802000).value, u32{0xFF});
    // Só 8 bits; janela de 128 bytes.
    CHECK_EQ(b.write32(0x1F802040, 0), BusFault::BusError);
    CHECK(is_bus_error(b.read16(0x1F802000)));
    CHECK(is_bus_error(b.read8(0x1F802080)));
    CHECK(is_bus_error(b.read8(0x1F803FFF)));
    // Base diferente de 1F802000h desabilita a região.
    REQUIRE_EQ(b.write32(0x1F801004, 0x1F803000), BusFault::None);
    CHECK(is_bus_error(b.read8(0x1F802000)));
    CHECK_EQ(b.write8(0x1F802041, 1), BusFault::BusError);
}

TEST_CASE("bus/Expansão 3: janela padrão de 1 byte") {
    Bus b;
    CHECK_EQ(b.read8(0x1FA00000).value, u32{0xFF});
    CHECK(is_bus_error(b.read8(0x1FA00001)));
    CHECK(is_bus_error(b.read32(0x1FA00004)));
    REQUIRE_EQ(b.write32(0x1F80100C, 0x00153022), BusFault::None); // 2 MiB
    CHECK(b.read32(0x1FBFFFFC).ok());
}

TEST_CASE("bus/regiões inexistentes geram erro de barramento") {
    Bus b;
    const u32 addresses[] = {
        0x01000000, 0x1EFFFFFC, 0x1F804000, 0x1F9FFFFC, 0x20000000, 0x7FFFFFFC,
        0x81000000, 0xA1000000, 0xC0000000, 0xFFFDFFFC, 0xFFFE0200, 0xFFFFFFFC,
    };
    for (u32 a : addresses) {
        CHECK(is_bus_error(b.read32(a)));
        CHECK_EQ(b.write32(a, 0), BusFault::BusError);
    }
}

TEST_CASE("bus/lacunas da área de E/S geram erro de barramento") {
    Bus b;
    const u32 gaps[] = {0x1F801024, 0x1F80103C, 0x1F801064, 0x1F80106C, 0x1F801078,
                        0x1F80107C, 0x1F801130, 0x1F8017FC, 0x1F801804, 0x1F80180C,
                        0x1F801818, 0x1F80181C, 0x1F801828, 0x1F801BFC};
    for (u32 a : gaps) {
        CHECK(is_bus_error(b.read32(a)));
        CHECK_EQ(b.write32(a, 0), BusFault::BusError);
    }
}

TEST_CASE("bus/periféricos não implementados são marcados e avisam uma vez") {
    WarnCapture capture;
    Bus b;
    const auto r = b.read32(0x1F801814); // GPUSTAT
    CHECK(r.ok());
    CHECK_EQ(r.value, u32{0});
    (void)b.read32(0x1F801814);
    (void)b.read32(0x1F801814);
    REQUIRE_EQ(capture.sink->size(), usize{1});
    const auto rec = capture.sink->records()[0];
    CHECK(rec.message.find("GPU") != std::string::npos);
    CHECK(rec.message.find("não implementado") != std::string::npos);
    CHECK(rec.message.find("Fase 7") != std::string::npos);

    auto* placeholder = dynamic_cast<UnimplementedDevice*>(&b.device(IoSlot::Gpu));
    REQUIRE(placeholder != nullptr);
    CHECK_EQ(placeholder->access_count(), u64{3});
    for (usize i = 0; i < static_cast<usize>(IoSlot::Count); ++i) {
        CHECK(dynamic_cast<UnimplementedDevice*>(&b.device(static_cast<IoSlot>(i))) != nullptr);
    }
}

TEST_CASE("bus/dispositivo conectado recebe deslocamento, largura e registrador inteiro") {
    Bus b;
    RecordingDevice dev;
    b.attach_device(IoSlot::Timers, dev);

    REQUIRE_EQ(b.write16(0x1F801114, 0xDEAD1234), BusFault::None);
    CHECK_EQ(dev.writes, 1);
    CHECK_EQ(dev.last_offset, u32{0x14});
    CHECK_EQ(dev.last_value, u32{0xDEAD1234}); // 32 bits, mesmo em SH
    CHECK(dev.last_width == AccessWidth::Half);

    const auto r8 = b.read8(0xBF801121);
    CHECK_EQ(dev.last_offset, u32{0x21});
    CHECK(dev.last_width == AccessWidth::Byte);
    CHECK_EQ(r8.value, u32{0xD4}); // truncado pelo barramento
    CHECK_EQ(b.read32(0x9F801100).value, u32{0xA1B2C3D4});
    CHECK_EQ(dev.reads, 2);
}

TEST_CASE("bus/faixas de E/S começam e terminam nos endereços documentados") {
    struct Expect {
        IoSlot slot;
        u32 begin;
        u32 end;
    };
    const Expect expected[] = {
        {IoSlot::Sio0, 0x1F801040, 0x1F801050}, {IoSlot::Sio1, 0x1F801050, 0x1F801060},
        {IoSlot::Interrupts, 0x1F801070, 0x1F801078}, {IoSlot::Dma, 0x1F801080, 0x1F801100},
        {IoSlot::Timers, 0x1F801100, 0x1F801130}, {IoSlot::Cdrom, 0x1F801800, 0x1F801804},
        {IoSlot::Gpu, 0x1F801810, 0x1F801818}, {IoSlot::Mdec, 0x1F801820, 0x1F801828},
        {IoSlot::Spu, 0x1F801C00, 0x1F802000},
    };
    for (const auto& e : expected) {
        const auto& info = io_slot_info(e.slot);
        CHECK_EQ(info.begin, e.begin);
        CHECK_EQ(info.end, e.end);
    }
}

TEST_CASE("bus/controle de cache em KSEG2") {
    Bus b;
    REQUIRE_EQ(b.write32(0xFFFE0130, 0x0001E988), BusFault::None);
    CHECK_EQ(b.read32(0xFFFE0130).value, u32{0x0001E988});
    CHECK_EQ(b.memory_control().cache_control(), u32{0x0001E988});
    CHECK_EQ(b.read16(0xFFFE0130).value, u32{0xE988});
    CHECK(is_bus_error(b.fetch_instruction(0xFFFE0130)));
    // Outro endereço do bloco de controle: não documentado (lê 0).
    WarnCapture capture;
    CHECK(b.read32(0xFFFE0000).ok());
}

TEST_CASE("bus/power_on zera a RAM; reset preserva") {
    Bus b;
    REQUIRE_EQ(b.write32(0x1000, 0x55AA55AA), BusFault::None);
    REQUIRE_EQ(b.write32(0x1F800010, 0x66), BusFault::None);
    REQUIRE_EQ(b.write32(0x1F801060, 0x0888), BusFault::None);
    REQUIRE_EQ(b.write8(0x1F802041, 0x5), BusFault::None);

    b.reset();
    CHECK_EQ(b.read32(0x1000).value, u32{0x55AA55AA});
    CHECK_EQ(b.read32(0x1F800010).value, u32{0x66});
    CHECK_EQ(b.memory_control().dram_ctrl(), u32{0x0B88});
    CHECK(!b.post_code().has_value());

    b.power_on();
    CHECK_EQ(b.read32(0x1000).value, u32{0});
    CHECK_EQ(b.read32(0x1F800010).value, u32{0});
}

TEST_CASE("bus/varredura de toda a RAM de 2 MiB") {
    Bus b;
    for (u32 a = 0; a < 2 * MiB_; a += 4) {
        if (b.write32(0x80000000u + a, a * 2654435761u) != BusFault::None) {
            CHECK(false);
            return;
        }
    }
    u32 mismatches = 0;
    for (u32 a = 0; a < 2 * MiB_; a += 4) {
        if (b.read32(0xA0000000u + a).value != a * 2654435761u) {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, u32{0});
}
