#include <fstream>
#include <vector>

#include "framework/rpcs1_test.hpp"
#include "memory/bios_rom.hpp"

using namespace rpcs1;
using namespace rpcs1::memory;

// Imagens sintéticas: o RPCS1 não contém nem usa BIOS real nos testes.
namespace {

std::vector<u8> synthetic_image(usize size) {
    std::vector<u8> image(size);
    for (usize i = 0; i < size; ++i) {
        image[i] = static_cast<u8>((i * 7 + (i >> 8)) & 0xFF);
    }
    return image;
}

} // namespace

TEST_CASE("bios_rom/aceita somente 512 KiB") {
    BiosRom rom;
    CHECK(!rom.loaded());
    for (usize size : {usize{0}, usize{1}, usize{256 * 1024}, usize{512 * 1024 - 1},
                       usize{512 * 1024 + 1}, usize{1024 * 1024}}) {
        auto status = rom.load(synthetic_image(size));
        CHECK(!status.ok());
        if (!status.ok()) {
            CHECK(status.error().code == ErrorCode::InvalidData);
        }
        CHECK(!rom.loaded());
    }
    REQUIRE(rom.load(synthetic_image(BiosRom::size)).ok());
    CHECK(rom.loaded());
}

TEST_CASE("bios_rom/conteúdo e leitura little-endian") {
    BiosRom rom;
    auto image = synthetic_image(BiosRom::size);
    image[0] = 0x78;
    image[1] = 0x56;
    image[2] = 0x34;
    image[3] = 0x12;
    REQUIRE(rom.load(image).ok());
    CHECK_EQ(rom.read32(0), u32{0x12345678});
    CHECK_EQ(rom.read16(2), u16{0x1234});
    CHECK_EQ(rom.read8(BiosRom::size - 1), image.back());
    rom.clear();
    CHECK(!rom.loaded());
    CHECK_EQ(rom.read32(0), u32{0});
}

TEST_CASE("bios_rom/load_file valida arquivo") {
    test::TempDir dir;
    BiosRom rom;

    auto missing = rom.load_file(dir.path() / "nao_existe.bin");
    REQUIRE(!missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);

    const auto big = dir.path() / "grande.bin";
    {
        std::ofstream out(big, std::ios::binary);
        const auto data = synthetic_image(BiosRom::size + 2048);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    auto too_big = rom.load_file(big);
    REQUIRE(!too_big.ok());
    CHECK(too_big.error().code == ErrorCode::OutOfRange);
    CHECK(!rom.loaded());

    const auto small = dir.path() / "pequeno.bin";
    {
        std::ofstream out(small, std::ios::binary);
        const auto data = synthetic_image(1000);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    auto too_small = rom.load_file(small);
    REQUIRE(!too_small.ok());
    CHECK(too_small.error().code == ErrorCode::InvalidData);

    const auto ok = dir.path() / "sintetica.bin";
    {
        std::ofstream out(ok, std::ios::binary);
        const auto data = synthetic_image(BiosRom::size);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    REQUIRE(rom.load_file(ok).ok());
    CHECK(rom.loaded());
    CHECK_EQ(rom.read8(12345), synthetic_image(BiosRom::size)[12345]);
}
