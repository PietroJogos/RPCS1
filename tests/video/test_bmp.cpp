#include "core/file_io.hpp"
#include "framework/rpcs1_test.hpp"
#include "video/bmp.hpp"

using namespace rpcs1;
using namespace rpcs1::video;

namespace {

u32 le32(const std::vector<u8>& b, usize at) {
    return static_cast<u32>(b[at]) | (static_cast<u32>(b[at + 1]) << 8) |
           (static_cast<u32>(b[at + 2]) << 16) | (static_cast<u32>(b[at + 3]) << 24);
}

} // namespace

TEST_CASE("bmp/cabeçalho, linhas de baixo para cima, ordem B-G-R e alinhamento") {
    Framebuffer fb(2, 2, 0);
    fb.set_pixel(0, 0, 0x112233);
    fb.set_pixel(1, 0, 0x445566);
    fb.set_pixel(0, 1, 0xAABBCC);
    fb.set_pixel(1, 1, 0xDDEEFF);
    const std::vector<u8> b = encode_bmp(fb);
    REQUIRE_EQ(b.size(), usize{54 + 8 * 2}); // 2 × 3 = 6 bytes → linha de 8
    CHECK_EQ(b[0], u8{'B'});
    CHECK_EQ(b[1], u8{'M'});
    CHECK_EQ(le32(b, 2), u32{70});
    CHECK_EQ(le32(b, 10), u32{54});
    CHECK_EQ(le32(b, 14), u32{40});
    CHECK_EQ(le32(b, 18), u32{2});
    CHECK_EQ(le32(b, 22), u32{2});
    CHECK_EQ(b[26], u8{1});
    CHECK_EQ(b[28], u8{24});
    CHECK_EQ(le32(b, 30), u32{0});
    CHECK_EQ(le32(b, 34), u32{16});
    // Primeira linha do arquivo = última da imagem (y = 1).
    const std::vector<u8> row0(b.begin() + 54, b.begin() + 62);
    CHECK(row0 == (std::vector<u8>{0xCC, 0xBB, 0xAA, 0xFF, 0xEE, 0xDD, 0, 0}));
    const std::vector<u8> row1(b.begin() + 62, b.end());
    CHECK(row1 == (std::vector<u8>{0x33, 0x22, 0x11, 0x66, 0x55, 0x44, 0, 0}));
}

TEST_CASE("bmp/gravação em arquivo e imagem vazia rejeitada") {
    test::TempDir dir;
    Framebuffer fb(320, 240, 0x00FF00);
    const auto path = dir.path() / "quadro.bmp";
    REQUIRE(save_bmp(path, fb).ok());
    auto bytes = file_io::read_binary(path, 1024 * 1024);
    REQUIRE(bytes.ok());
    CHECK_EQ(bytes.value().size(), usize{54 + 320 * 3 * 240});
    CHECK_EQ(bytes.value()[54 + 1], u8{0xFF}); // G do primeiro pixel
    CHECK(!save_bmp(dir.path() / "vazio.bmp", Framebuffer{}).ok());
    CHECK(!save_bmp(dir.path(), fb).ok()); // destino é uma pasta
}
