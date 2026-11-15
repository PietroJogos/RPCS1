// Rasterizador da GPU (Fase 8). Casos "medido" reproduzem resultados publicados no psx.wiki
// ("gpu-rasterization" e "gpu-primitives", SCPH-5501); os demais seguem as regras documentadas.
#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <string>

#include "framework/rpcs1_test.hpp"
#include "gpu/gpu.hpp"
#include "gpu/renderer.hpp"
#include "system/timing_rig.hpp"

using namespace rpcs1;

namespace {

constexpr u32 xy(s32 x, s32 y) {
    return (static_cast<u32>(y & 0xFFFF) << 16) | static_cast<u32>(x & 0xFFFF);
}

struct DrawRig {
    test::TimingRig t;
    gpu::Gpu gpu{t.video, t.irq};

    DrawRig() { gp0({0xE3000000, 0xE407FFFF}); } // área de desenho = VRAM inteira

    void gp0(std::initializer_list<u32> words) {
        for (u32 w : words) {
            gpu.gp0_write(w);
        }
    }
    [[nodiscard]] u16 px(u32 x, u32 y) const { return gpu.vram().get(x, y); }
    void set(u32 x, u32 y, u16 v) { gpu.vram().set(x, y, v); }
    [[nodiscard]] u32 red(u32 x, u32 y) const { return px(x, y) & 0x1Fu; }
    [[nodiscard]] u32 count(u32 x0, u32 y0, u32 w, u32 h) const {
        u32 n = 0;
        for (u32 y = y0; y < y0 + h; ++y) {
            for (u32 x = x0; x < x0 + w; ++x) {
                n += px(x, y) != 0 ? 1u : 0u;
            }
        }
        return n;
    }
    // Dígitos de base 32 do canal vermelho (formato usado nas páginas de teste).
    [[nodiscard]] std::string red_digits(u32 x0, u32 y, u32 n) const {
        static constexpr char digits[] = "0123456789abcdefghijklmnopqrstuv";
        std::string s;
        for (u32 x = x0; x < x0 + n; ++x) {
            s += digits[red(x, y)];
        }
        return s;
    }
};

} // namespace

TEST_CASE("render/triângulo 0,0 10,0 0,10 desenha x + y ≤ 9 nos dois sentidos (medido)") {
    for (const bool reversed : {false, true}) {
        DrawRig r;
        if (reversed) {
            r.gp0({0x20FFFFFF, xy(0, 10), xy(10, 0), xy(0, 0)});
        } else {
            r.gp0({0x20FFFFFF, xy(0, 0), xy(10, 0), xy(0, 10)});
        }
        bool exact = true;
        for (u32 y = 0; y < 12; ++y) {
            for (u32 x = 0; x < 12; ++x) {
                exact = exact && ((r.px(x, y) != 0) == (x + y <= 9));
            }
        }
        CHECK(exact);
        CHECK_EQ(r.count(0, 0, 12, 12), u32{55});
        CHECK_EQ(r.px(0, 0), u16{0x7FFF}); // sem dithering em polígono plano
    }
}

TEST_CASE("render/quadrado dividido em qualquer diagonal cobre cada pixel uma vez (medido)") {
    // Semitransparência aditiva (B + F) com F = 1: o valor final conta quantas vezes o pixel foi
    // desenhado.
    const auto check = [](DrawRig& r) {
        bool once = true;
        for (u32 y = 0; y < 12; ++y) {
            for (u32 x = 0; x < 12; ++x) {
                const u16 expected = (x < 10 && y < 10) ? 1 : 0;
                once = once && r.red(x, y) == expected;
            }
        }
        return once;
    };
    DrawRig a; // quadrilátero em "Z": triângulos 1-2-3 e 2-3-4
    a.gp0({0xE1000020, 0x2A000008, xy(0, 0), xy(10, 0), xy(0, 10), xy(10, 10)});
    CHECK(check(a));
    DrawRig b; // a outra diagonal, com dois triângulos
    b.gp0({0xE1000020, 0x22000008, xy(0, 0), xy(10, 0), xy(10, 10)});
    b.gp0({0x22000008, xy(0, 0), xy(10, 10), xy(0, 10)});
    CHECK(check(b));
    DrawRig c; // leque de 4 triângulos em volta do centro
    c.gp0({0xE1000020});
    c.gp0({0x22000008, xy(5, 5), xy(0, 0), xy(10, 0)});
    c.gp0({0x22000008, xy(5, 5), xy(10, 0), xy(10, 10)});
    c.gp0({0x22000008, xy(5, 5), xy(10, 10), xy(0, 10)});
    c.gp0({0x22000008, xy(5, 5), xy(0, 10), xy(0, 0)});
    CHECK(check(c));
}

TEST_CASE("render/triângulos finos: 200, 199 e 100 pixels (medido)") {
    DrawRig a;
    a.gp0({0x20FFFFFF, xy(0, 0), xy(200, 0), xy(200, 1)});
    CHECK_EQ(a.count(0, 0, 210, 4), u32{200});
    DrawRig b;
    b.gp0({0x20FFFFFF, xy(0, 0), xy(0, 200), xy(1, 200)});
    CHECK_EQ(b.count(0, 0, 4, 210), u32{199});
    DrawRig c;
    c.gp0({0x20FFFFFF, xy(0, 0), xy(200, 1), xy(200, 2)});
    CHECK_EQ(c.count(0, 0, 210, 4), u32{100});
}

TEST_CASE("render/limites: 1023 × 511 desenha, 1024 ou 512 não (medido, por triângulo)") {
    const auto drawn = [](std::initializer_list<u32> words) {
        DrawRig r;
        r.gp0(words);
        return r.gpu.pixels_drawn();
    };
    CHECK(drawn({0x20FFFFFF, xy(0, 0), xy(1023, 0), xy(0, 16)}) > 0);
    CHECK_EQ(drawn({0x20FFFFFF, xy(-1, 0), xy(1023, 0), xy(0, 16)}), u64{0});
    CHECK(drawn({0x20FFFFFF, xy(0, 0), xy(16, 0), xy(0, 511)}) > 0);
    CHECK_EQ(drawn({0x20FFFFFF, xy(0, -1), xy(16, 0), xy(0, 511)}), u64{0});
    // Quadrilátero de 1060 de largura cujos triângulos têm menos de 1024 cada: desenhado.
    // 1-2-3 vai de −30 a 500 e 2-3-4 de 500 a 1030.
    CHECK(drawn({0x28FFFFFF, xy(-30, 0), xy(500, 0), xy(500, 16), xy(1030, 16)}) > 0);
    // Quadrilátero cujos dois triângulos passam de 1023: nada.
    CHECK_EQ(drawn({0x28FFFFFF, xy(-30, 0), xy(1000, 0), xy(0, 16), xy(1030, 16)}), u64{0});
    // Linhas: mesmo limite.
    CHECK(drawn({0x40FFFFFF, xy(0, 0), xy(1023, 0)}) > 0);
    CHECK_EQ(drawn({0x40FFFFFF, xy(-1, 0), xy(1023, 0)}), u64{0});
    CHECK_EQ(drawn({0x40FFFFFF, xy(0, -1), xy(0, 511)}), u64{0});
}

TEST_CASE("render/coordenadas de 11 bits e deslocamento E5h com volta (medido)") {
    DrawRig r;
    // Bits 11–15 ignorados: FFFCh e 07FCh são −4. Deslocamento (8, 8).
    r.gp0({0xE5000000 | (8u << 11) | 8u});
    r.gp0({0x20FFFFFF, xy(0x07FC, 0x07FC), xy(6, 0xFFFC), xy(0xFFFC, 6)});
    CHECK(r.px(4, 4) != 0);
    // Deslocamento −1024: x = −208 + (−1024) volta para 816 (11 bits com sinal).
    DrawRig w;
    w.gp0({0xE5000000 | 0x400u});
    w.gp0({0x20FFFFFF, xy(-208, 0), xy(-198, 0), xy(-208, 10)});
    CHECK(w.px(816, 0) != 0);
    CHECK_EQ(w.count(816, 0, 12, 12), u32{55});
}

TEST_CASE("render/área de desenho E3h/E4h inclusiva") {
    DrawRig r;
    r.gp0({0xE3000000 | (10u << 10) | 10u, 0xE4000000 | (20u << 10) | 20u});
    r.gp0({0x60FFFFFF, xy(0, 0), xy(40, 40)}); // retângulo 40×40
    CHECK_EQ(r.count(0, 0, 40, 40), u32{121});
    CHECK(r.px(10, 10) != 0);
    CHECK(r.px(20, 20) != 0);
    CHECK_EQ(r.px(21, 20), u16{0});
    r.gp0({0x20FFFFFF, xy(0, 30), xy(40, 30), xy(0, 70)}); // fora da área
    CHECK_EQ(r.count(0, 21, 40, 19), u32{0});
}

TEST_CASE("render/linha gouraud 00→FF em 16 pixels, com e sem dithering (medido)") {
    DrawRig off;
    off.gp0({0x50000000, xy(0, 0), 0x000000FF, xy(15, 0)});
    CHECK_EQ(off.red_digits(0, 0, 16), std::string("02468acehjlnprtv"));
    DrawRig on;
    on.gp0({0xE1000200}); // E1h bit 9: dithering
    on.gp0({0x50000000, xy(0, 0), 0x000000FF, xy(15, 0)});
    CHECK_EQ(on.red_digits(0, 0, 16), std::string("02368acfgjknprtv"));
    // Mesma linha desenhada ao contrário: mesmo resultado.
    DrawRig back;
    back.gp0({0xE1000200});
    back.gp0({0x500000FF, xy(15, 0), 0x00000000, xy(0, 0)});
    CHECK_EQ(back.red_digits(0, 0, 16), std::string("02368acfgjknprtv"));
}

TEST_CASE("render/linhas: extremos incluídos, simetria e 1 pixel com extremos iguais (medido)") {
    DrawRig r;
    r.gp0({0x40FFFFFF, xy(8, 8), xy(16, 8)});
    CHECK_EQ(r.count(0, 0, 32, 32), u32{9});
    DrawRig p;
    p.gp0({0x40FFFFFF, xy(5, 5), xy(5, 5)});
    CHECK_EQ(p.count(0, 0, 16, 16), u32{1});
    CHECK(p.px(5, 5) != 0);
    // Para várias inclinações, ida e volta desenham os mesmos pixels.
    const s32 ends[][2] = {{20, 7}, {7, 20}, {20, -7}, {-7, 20}, {13, 13}, {19, 2}, {3, 18}};
    bool symmetric = true;
    for (const auto& e : ends) {
        DrawRig f;
        DrawRig b;
        f.gp0({0x40FFFFFF, xy(32, 32), xy(32 + e[0], 32 + e[1])});
        b.gp0({0x40FFFFFF, xy(32 + e[0], 32 + e[1]), xy(32, 32)});
        for (u32 y = 0; y < 64; ++y) {
            for (u32 x = 0; x < 64; ++x) {
                symmetric = symmetric && f.px(x, y) == b.px(x, y);
            }
        }
        symmetric = symmetric && f.count(0, 0, 64, 64) ==
                                     static_cast<u32>(std::max(std::abs(e[0]), std::abs(e[1])) + 1);
    }
    CHECK(symmetric);
}

TEST_CASE("render/polilinha fechada desenha os cantos duas vezes (medido)") {
    DrawRig r;
    r.gp0({0xE1000020}); // B + F
    r.gp0({0x4A000008, xy(0, 0), xy(10, 0), xy(10, 10), xy(0, 10), xy(0, 0), 0x55555555});
    CHECK_EQ(r.red(10, 0), u32{2});
    CHECK_EQ(r.red(10, 10), u32{2});
    CHECK_EQ(r.red(0, 10), u32{2});
    CHECK_EQ(r.red(0, 0), u32{2}); // início e fim
    CHECK_EQ(r.red(5, 0), u32{1});
    CHECK_EQ(r.red(5, 5), u32{0});
    // Polilinha gouraud: o marcador vem na posição da cor.
    DrawRig g;
    g.gp0({0x580000FF, xy(0, 20), 0x000000FF, xy(10, 20), 0x55555555});
    CHECK_EQ(g.red(0, 20), u32{31});
    CHECK_EQ(g.red(10, 20), u32{31});
    CHECK_EQ(g.gpu.gpustat() >> 26 & 1u, u32{1}); // pronto para outro comando
}

TEST_CASE("render/UV interpolado e arredondado: 00 10 20 … EF (medido)") {
    DrawRig r;
    // Textura de 15 bits em (512, 256): o texel de U vale 8000h | U.
    for (u32 u = 0; u < 256; ++u) {
        r.set(512 + u, 256, static_cast<u16>(0x8000u | u));
    }
    const u32 page = 8u | (1u << 4) | (2u << 7); // X 512, Y 256, 15 bits
    r.gp0({0x2D000000, xy(0, 0), 0x00000000, xy(16, 0), (page << 16) | 0x00FF, xy(0, 1), 0x0000,
           xy(16, 1), 0x00FF});
    const u32 expected[16] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
                              0x80, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF};
    bool same = true;
    for (u32 i = 0; i < 16; ++i) {
        same = same && r.px(i, 0) == (0x8000u | expected[i]);
    }
    CHECK(same);
    CHECK_EQ(r.px(16, 0), u16{0});
    // A página de textura do polígono vai para GPUSTAT bits 0–8.
    CHECK_EQ(r.gpu.gpustat() & 0x1FFu, page);
}

TEST_CASE("render/página do polígono não muda os bits 9, 10, 12 e 13 de E1h (medido)") {
    DrawRig r;
    r.gp0({0xE1003600}); // 9, 10, 12, 13
    r.gp0({0x25000000, xy(0, 0), 0, xy(4, 0), 0xFFFF0000, xy(0, 4), 0});
    const u32 stat = r.gpu.gpustat();
    CHECK_EQ(stat & 0x1FFu, u32{0x1FF});
    CHECK_EQ((stat >> 9) & 3u, u32{3});
    CHECK_EQ((stat >> 15) & 1u, u32{1}); // bit 11 → GPUSTAT 15
}

TEST_CASE("render/semitransparência B/2 + F/2 em 5 bits (medido)") {
    const u16 backs[4] = {0x0000, 0x2108, 0x5EF7, 0x7FFF};
    DrawRig r; // retângulo plano semitransparente 62h, cor 808080h
    for (u32 i = 0; i < 4; ++i) r.set(i, 0, backs[i]);
    r.gp0({0x62808080, xy(0, 0), 0x00010004});
    CHECK_EQ(r.px(0, 0), u16{0x2108});
    CHECK_EQ(r.px(1, 0), u16{0x318C});
    CHECK_EQ(r.px(2, 0), u16{0x4E73});
    CHECK_EQ(r.px(3, 0), u16{0x5EF7});
    // Retângulo texturizado cru 67h com texel C210h (bit 15 = semitransparente).
    DrawRig t;
    for (u32 i = 0; i < 4; ++i) {
        t.set(i, 0, backs[i]);
        t.set(512 + i, 256, 0xC210);
    }
    t.gp0({0xE1000118, 0x67000000, xy(0, 0), 0x00000000, 0x00010004});
    CHECK_EQ(t.px(0, 0), u16{0xA108});
    CHECK_EQ(t.px(1, 0), u16{0xB18C});
    CHECK_EQ(t.px(2, 0), u16{0xCE73});
    CHECK_EQ(t.px(3, 0), u16{0xDEF7});
}

TEST_CASE("render/semitransparência: modos 1, 2 e 3 com saturação; texel sem bit 15 é opaco") {
    const auto blend = [](u32 mode, u16 back, u32 color) {
        DrawRig r;
        r.set(0, 0, back);
        r.gp0({0xE1000000 | (mode << 5), 0x62000000 | color, xy(0, 0), 0x00010001});
        return r.px(0, 0);
    };
    // F = 16 (80h) em todos os canais.
    CHECK_EQ(blend(1, 0x2108, 0x808080), u16{0x6318});  // 8 + 16 = 24
    CHECK_EQ(blend(1, 0x5EF7, 0x808080), u16{0x7FFF});  // 23 + 16 → 31
    CHECK_EQ(blend(2, 0x5EF7, 0x808080), u16{0x1CE7});  // 23 − 16 = 7
    CHECK_EQ(blend(2, 0x2108, 0x808080), u16{0x0000});  // 8 − 16 → 0
    CHECK_EQ(blend(3, 0x2108, 0x808080), u16{0x318C});  // 8 + 16/4 = 12
    CHECK_EQ(blend(3, 0x7FFF, 0x808080), u16{0x7FFF});
    DrawRig t; // texel sem bit 15: desenhado opaco mesmo com ABE
    t.set(0, 0, 0x2108);
    t.set(512, 256, 0x4210);
    t.gp0({0xE1000118, 0x67000000, xy(0, 0), 0x00000000, 0x00010001});
    CHECK_EQ(t.px(0, 0), u16{0x4210});
}

TEST_CASE("render/texel 0000h é transparente; CLUT de 4 e 8 bits") {
    DrawRig r;
    for (u32 i = 0; i < 16; ++i) r.set(i, 500, static_cast<u16>(i == 0 ? 0 : 0x0400u + i));
    r.set(512, 256, 0x3210); // 4 bits: índices 0, 1, 2, 3 (o primeiro nos bits 0–3)
    r.set(0, 0, 0x1234);
    const u32 clut = 500u << 6;
    r.gp0({0xE1000018, 0x65000000, xy(0, 0), (clut << 16) | 0x0000, 0x00010004});
    CHECK_EQ(r.px(0, 0), u16{0x1234}); // índice 0 → CLUT 0000h → não desenhado
    CHECK_EQ(r.px(1, 0), u16{0x0401});
    CHECK_EQ(r.px(2, 0), u16{0x0402});
    CHECK_EQ(r.px(3, 0), u16{0x0403});
    // 8 bits: índices 1 e 2 (o primeiro nos bits 0–7), CLUT em X = 16.
    DrawRig e;
    for (u32 i = 0; i < 256; ++i) e.set(16 + i, 400, static_cast<u16>(0x0800u + i));
    e.set(512, 256, 0x0201);
    e.gp0({0xE1000098, 0x65000000, xy(0, 0), (((400u << 6) | 1u) << 16), 0x00010002});
    CHECK_EQ(e.px(0, 0), u16{0x0801});
    CHECK_EQ(e.px(1, 0), u16{0x0802});
}

TEST_CASE("render/texel 8000h é preto opaco, não transparente") {
    DrawRig r;
    r.set(0, 0, 0x1234);
    r.set(512, 256, 0x8000);
    r.gp0({0xE1000118, 0x65000000, xy(0, 0), 0x00000000, 0x00010001});
    CHECK_EQ(r.px(0, 0), u16{0x8000});
}

TEST_CASE("render/janela de textura E2h: máscara 1, deslocamento 1 (medido)") {
    DrawRig r;
    for (u32 u = 0; u < 256; ++u) r.set(512 + u, 256, static_cast<u16>(0x8000u | u));
    r.gp0({0xE1000118, 0xE2000401, 0x65000000, xy(0, 0), 0x00000000, 0x00010010});
    bool ok = true;
    for (u32 i = 0; i < 16; ++i) {
        ok = ok && r.px(i, 0) == (0x8000u | (0x08u + (i & 7u)));
    }
    CHECK(ok);
}

TEST_CASE("render/modulação (texel × cor) / 128 com saturação") {
    const auto modulate = [](u32 color, u16 texel) {
        DrawRig r;
        r.set(512, 256, texel);
        r.gp0({0xE1000118, 0x64000000 | color, xy(0, 0), 0x00000000, 0x00010001});
        return r.px(0, 0);
    };
    CHECK_EQ(modulate(0x808080, 0x4210), u16{0x4210}); // 80h = neutro
    CHECK_EQ(modulate(0x404040, 0x4210), u16{0x2108}); // metade
    CHECK_EQ(modulate(0xFFFFFF, 0x7FFF), u16{0x7FFF}); // satura em 1Fh
    CHECK_EQ(modulate(0x000080, 0x7C1F), u16{0x001F}); // só o vermelho
    CHECK_EQ(modulate(0x808080, 0x8421), u16{0x8421}); // bit 15 do texel é gravado
}

TEST_CASE("render/dithering: polígono gouraud sim, plano e retângulo não") {
    // 42h (66) + deslocamento da matriz: < 64 → 7, ≥ 64 → 8.
    DrawRig flat;
    flat.gp0({0xE1000200, 0x20000042, xy(0, 0), xy(8, 0), xy(0, 8)});
    CHECK_EQ(flat.red(0, 0), u32{8});
    CHECK_EQ(flat.red(2, 0), u32{8});
    DrawRig gouraud;
    gouraud.gp0({0xE1000200, 0x30000042, xy(0, 0), 0x42, xy(8, 0), 0x42, xy(0, 8)});
    CHECK_EQ(gouraud.red(0, 0), u32{7}); // −4
    CHECK_EQ(gouraud.red(1, 0), u32{8}); // +0
    CHECK_EQ(gouraud.red(2, 0), u32{7}); // −3
    CHECK_EQ(gouraud.red(0, 1), u32{8}); // +2
    CHECK_EQ(gouraud.red(2, 2), u32{7}); // −4
    DrawRig rect;
    rect.gp0({0xE1000200, 0x60000042, xy(0, 0), 0x00040004});
    CHECK_EQ(rect.red(0, 0), u32{8});
    CHECK_EQ(rect.red(2, 0), u32{8});
    CHECK_EQ(gpu::dither_offset(4, 4), s32{-4});
    CHECK_EQ(gpu::dither_offset(3, 3), s32{-2});
}

TEST_CASE("render/máscara E6h: força o bit 15 e protege pixels marcados") {
    DrawRig r;
    r.set(1, 0, 0x8001);
    r.gp0({0xE6000002, 0x60FFFFFF, xy(0, 0), 0x00010002});
    CHECK_EQ(r.px(0, 0), u16{0x7FFF});
    CHECK_EQ(r.px(1, 0), u16{0x8001}); // protegido
    r.gp0({0xE6000001, 0x60FFFFFF, xy(0, 1), 0x00010001});
    CHECK_EQ(r.px(0, 1), u16{0xFFFF});
}

TEST_CASE("render/retângulos: tamanhos fixos, variável módulo 1024/512, deslocamento e espelho") {
    DrawRig r;
    r.gp0({0x68FFFFFF, xy(0, 0)});   // 1×1
    r.gp0({0x70FFFFFF, xy(10, 0)});  // 8×8
    r.gp0({0x78FFFFFF, xy(20, 0)});  // 16×16
    CHECK_EQ(r.count(0, 0, 10, 20), u32{1});
    CHECK_EQ(r.count(10, 0, 10, 20), u32{64});
    CHECK_EQ(r.count(20, 0, 20, 20), u32{256});
    DrawRig v;
    v.gp0({0x60FFFFFF, xy(0, 0), 0x00020401}); // largura 1025 → 1
    CHECK_EQ(v.count(0, 0, 40, 4), u32{2});
    v.gp0({0x60FFFFFF, xy(0, 10), 0x00000400}); // largura 1024 → 0
    CHECK_EQ(v.count(0, 10, 40, 4), u32{0});
    DrawRig o;
    o.gp0({0xE5000000 | (5u << 11) | 3u, 0x68FFFFFF, xy(1, 1)});
    CHECK(o.px(4, 6) != 0);
    // Espelho em X (E1h bit 12): U decresce ao longo da linha.
    DrawRig f;
    for (u32 u = 0; u < 256; ++u) f.set(512 + u, 256, static_cast<u16>(0x8000u | u));
    f.gp0({0xE1001118, 0x65000000, xy(0, 0), 0x00000010, 0x00010004});
    CHECK(f.px(1, 0) == f.px(0, 0) - 1u);
    CHECK(f.px(2, 0) == f.px(1, 0) - 1u);
}

TEST_CASE("render/triângulo gouraud texturizado modulado em VRAM com quadrilátero") {
    // Fumaça de integração: textura de 4 bits, gouraud e modulação juntos não saem da área.
    DrawRig r;
    r.gp0({0xE3000000 | (8u << 10) | 8u, 0xE4000000 | (40u << 10) | 40u});
    for (u32 i = 0; i < 16; ++i) r.set(i, 500, static_cast<u16>(0x7FFF));
    for (u32 x = 0; x < 16; ++x)
        for (u32 y = 0; y < 64; ++y) r.set(512 + x, 256 + y, 0x1111);
    const u32 page = 8u | (1u << 4);
    r.gp0({0x3C808080, xy(0, 0), ((500u << 6) << 16) | 0x0000, 0x00808080, xy(48, 0),
           (page << 16) | 0x003F, 0x00808080, xy(0, 48), 0x3F00, 0x00808080, xy(48, 48), 0x3F3F});
    CHECK_EQ(r.count(0, 0, 64, 64), u32{33 * 33});
    CHECK_EQ(r.px(8, 8), u16{0x7FFF});
}
