#include "framework/rpcs1_test.hpp"
#include "memory/memory_control.hpp"

using namespace rpcs1;
using namespace rpcs1::memory;
using bus::AccessWidth;

TEST_CASE("memory_control/valores de reset e janelas padrão") {
    MemoryControl mc;
    CHECK_EQ(mc.expansion1_base(), u32{0x1F000000});
    CHECK_EQ(mc.expansion2_base(), u32{0x1F802000});
    CHECK_EQ(mc.delay_size(DelayDevice::Bios), u32{0x0013243F});
    CHECK_EQ(mc.dram_ctrl(), u32{0x0B88});
    CHECK_EQ(mc.cache_control(), u32{0});
    // N = bits 16–20; janela = 1 << N (psx-spx: 512 KiB, 1 byte, 512 KiB, 128 bytes).
    CHECK_EQ(mc.window_size(DelayDevice::Expansion1), u64{512 * 1024});
    CHECK_EQ(mc.window_size(DelayDevice::Expansion3), u64{1});
    CHECK_EQ(mc.window_size(DelayDevice::Bios), u64{512 * 1024});
    CHECK_EQ(mc.window_size(DelayDevice::Expansion2), u64{128});
}

TEST_CASE("memory_control/decodificação do DRAM_CTRL") {
    struct Case {
        u32 value;
        u32 bank0;
        u32 bank1;
    };
    const Case cases[] = {
        {0x0000, 1 * 1024 * 1024, 0},               // 1 MiB + 15 MiB sem mapeamento
        {0x0800, 2 * 1024 * 1024, 0},               // 2 MiB + 14 MiB
        {0x0888, 2 * 1024 * 1024, 0},               // valor "correto" para 2 MiB
        {0x0B88, 8 * 1024 * 1024, 0},               // padrão da BIOS: 8 MiB (3 espelhos)
        {0x0B80, 8 * 1024 * 1024, 0},               // PU-7 / EARLY-PU-8
        {0x0F00, 8 * 1024 * 1024, 8 * 1024 * 1024}, // 8 MiB + 8 MiB
        {0x0200, 4 * 1024 * 1024, 0},
        {0x0400, 1 * 1024 * 1024, 1 * 1024 * 1024},
    };
    for (const auto& c : cases) {
        const RamLayout layout = MemoryControl::decode_dram_ctrl(c.value);
        CHECK_EQ(layout.bank0_size, c.bank0);
        CHECK_EQ(layout.bank1_size, c.bank1);
    }
}

TEST_CASE("memory_control/leitura e escrita de registradores") {
    MemoryControl mc;
    CHECK(MemoryControl::contains(0x1F801000));
    CHECK(MemoryControl::contains(0x1F801023));
    CHECK(!MemoryControl::contains(0x1F801024));
    CHECK(MemoryControl::contains(0x1F801060));
    CHECK(MemoryControl::contains(0x1F801063));
    CHECK(!MemoryControl::contains(0x1F801064));

    mc.write(0x1F801010, 0x00163FFF, AccessWidth::Word);
    CHECK_EQ(mc.read(0x1F801010, AccessWidth::Word), u32{0x00163FFF});
    CHECK_EQ(mc.window_size(DelayDevice::Bios), u64{4 * 1024 * 1024});
    // Leituras estreitas devolvem a faixa de bytes correspondente.
    CHECK_EQ(mc.read(0x1F801012, AccessWidth::Half), u32{0x0016});
    CHECK_EQ(mc.read(0x1F801011, AccessWidth::Byte), u32{0x3F});
}

TEST_CASE("memory_control/bits fixos e de flag") {
    MemoryControl mc;
    // Delay/Size: bits 21–23 sempre zero; bit 28 (flag de erro) se limpa escrevendo 1.
    mc.write(0x1F801008, 0xFFFFFFFF, AccessWidth::Word);
    CHECK_EQ(mc.read(0x1F801008, AccessWidth::Word), u32{0xEF1FFFFF});
    // COM_DELAY: bits 16–31 lidos como zero.
    mc.write(0x1F801020, 0xFFFFFFFF, AccessWidth::Word);
    CHECK_EQ(mc.com_delay(), u32{0x0000FFFF});
    // DRAM_CTRL: bits 16–31 sem uso (lidos como zero pelo RPCS1).
    mc.write(0x1F801060, 0xABCD0888, AccessWidth::Word);
    CHECK_EQ(mc.dram_ctrl(), u32{0x0888});
    CHECK_EQ(mc.ram_layout().bank0_size, u32{2 * 1024 * 1024});
}

TEST_CASE("memory_control/escrita estreita usa os 32 bits do registrador da CPU") {
    // Comportamento escolhido (⚠️ não documentado para o Memory Control): SB/SH colocam o
    // registrador inteiro no barramento, e o registrador recebe o valor completo.
    MemoryControl mc;
    mc.write(0x1F801060, 0x00000888, AccessWidth::Half);
    CHECK_EQ(mc.dram_ctrl(), u32{0x0888});
    mc.write(0x1F801061, 0x00000B88, AccessWidth::Byte);
    CHECK_EQ(mc.dram_ctrl(), u32{0x0B88});
}

TEST_CASE("memory_control/reset restaura padrões") {
    MemoryControl mc;
    mc.write(0x1F801060, 0x0000, AccessWidth::Word);
    mc.set_cache_control(0x0001E988);
    mc.reset();
    CHECK_EQ(mc.dram_ctrl(), u32{0x0B88});
    CHECK_EQ(mc.cache_control(), u32{0});
}
