#include "framework/rpcs1_test.hpp"
#include "video/framebuffer.hpp"
#include "video/window.hpp"

using namespace rpcs1;
using namespace rpcs1::video;

TEST_CASE("framebuffer/criação, preenchimento e acesso") {
    Framebuffer fb(320, 240, 0x112233);
    CHECK_EQ(fb.width(), u32{320});
    CHECK_EQ(fb.height(), u32{240});
    CHECK(!fb.empty());
    CHECK_EQ(fb.pixel(0, 0), u32{0x112233});
    CHECK_EQ(fb.pixel(319, 239), u32{0x112233});

    fb.set_pixel(10, 20, 0xFF0000);
    CHECK_EQ(fb.pixel(10, 20), u32{0xFF0000});
    CHECK_EQ(fb.data()[20 * 320 + 10], u32{0xFF0000});

    fb.clear(0);
    CHECK_EQ(fb.pixel(10, 20), u32{0});
}

TEST_CASE("framebuffer/acesso fora dos limites é seguro") {
    Framebuffer fb(4, 4, 7);
    CHECK_EQ(fb.pixel(4, 0), u32{0});
    CHECK_EQ(fb.pixel(0, 4), u32{0});
    fb.set_pixel(100, 100, 1); // ignorado
    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            CHECK_EQ(fb.pixel(x, y), u32{7});
        }
    }
    Framebuffer empty;
    CHECK(empty.empty());
    CHECK_EQ(empty.pixel(0, 0), u32{0});
}

TEST_CASE("window/fit_aspect mantém 4:3 centralizado") {
    // Área exata 4:3.
    CHECK(fit_aspect(640, 480, 4.0 / 3.0) == (Rect{0, 0, 640, 480}));
    // Área larga: faixas laterais (pillarbox).
    CHECK(fit_aspect(1920, 1080, 4.0 / 3.0) == (Rect{240, 0, 1440, 1080}));
    // Área alta: faixas superior/inferior (letterbox).
    CHECK(fit_aspect(640, 1000, 4.0 / 3.0) == (Rect{0, 260, 640, 480}));
    // Entradas degeneradas.
    CHECK(fit_aspect(0, 480, 4.0 / 3.0) == Rect{});
    CHECK(fit_aspect(640, 480, 0.0) == Rect{});
}
