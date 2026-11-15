#include "framework/rpcs1_test.hpp"
#include "memory/memory_block.hpp"

using namespace rpcs1;
using namespace rpcs1::memory;

TEST_CASE("memory_block/tamanhos da RAM e do scratchpad") {
    CHECK_EQ(MainRam::size, usize{2 * 1024 * 1024});
    CHECK_EQ(Scratchpad::size, usize{1024});
}

TEST_CASE("memory_block/ordem de bytes little-endian") {
    Scratchpad block;
    block.fill(0);
    block.write32(0, 0x11223344);
    CHECK_EQ(block.read8(0), u8{0x44});
    CHECK_EQ(block.read8(1), u8{0x33});
    CHECK_EQ(block.read8(2), u8{0x22});
    CHECK_EQ(block.read8(3), u8{0x11});
    CHECK_EQ(block.read16(0), u16{0x3344});
    CHECK_EQ(block.read16(2), u16{0x1122});

    block.write8(5, 0xAB);
    block.write16(6, 0xCDEF);
    CHECK_EQ(block.read32(4), u32{0xCDEFAB00});
}

TEST_CASE("memory_block/escritas estreitas preservam os bytes vizinhos") {
    Scratchpad block;
    block.write32(8, 0xFFFFFFFF);
    block.write8(9, 0x00);
    CHECK_EQ(block.read32(8), u32{0xFFFF00FF});
    block.write16(10, 0x1234);
    CHECK_EQ(block.read32(8), u32{0x123400FF});
}

TEST_CASE("memory_block/deslocamentos além do tamanho espelham") {
    Scratchpad block;
    block.fill(0);
    block.write32(0x10, 0xCAFEBABE);
    CHECK_EQ(block.read32(0x10 + 1024), u32{0xCAFEBABE});
    CHECK_EQ(block.read32(0x10 + 5 * 1024), u32{0xCAFEBABE});
}

TEST_CASE("memory_block/últimos bytes acessíveis sem estouro") {
    MainRam ram;
    ram.fill(0);
    ram.write32(MainRam::size - 4, 0xDEADBEEF);
    CHECK_EQ(ram.read32(MainRam::size - 4), u32{0xDEADBEEF});
    CHECK_EQ(ram.read8(MainRam::size - 1), u8{0xDE});
    CHECK_EQ(ram.bytes().size(), MainRam::size);
}
