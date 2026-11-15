// GPU (Fase 7): GP0/GP1, GPUSTAT, VRAM, transferências e imagem da área de exibição.
// Valores "medidos" vêm dos testes em SCPH-5501 do psx.wiki ("gpustat-dma-bits"); os demais
// seguem o psx-spx ("GPU").
#include <initializer_list>

#include "bus/bus.hpp"
#include "framework/rpcs1_test.hpp"
#include "gpu/gpu.hpp"
#include "system/console.hpp"
#include "system/timing_rig.hpp"
#include "video/framebuffer.hpp"

using namespace rpcs1;
using gpu::Gpu;

namespace {

struct GpuRig {
    test::TimingRig t;
    Gpu gpu{t.video, t.irq};

    void gp0(std::initializer_list<u32> words) {
        for (u32 w : words) {
            gpu.gp0_write(w);
        }
    }
    void gp1(u32 word) { gpu.gp1_write(word); }
    [[nodiscard]] u16 px(u32 x, u32 y) const { return gpu.vram().get(x, y); }
    [[nodiscard]] u32 stat_bit(u32 bit) const { return (gpu.gpustat() >> bit) & 1u; }
};

constexpr u32 stat_ready_cmd = 26;
constexpr u32 stat_ready_send = 27;
constexpr u32 stat_ready_dma = 28;
constexpr u32 stat_dreq = 25;

} // namespace

TEST_CASE("gpu/GPUSTAT após ligar e após GP1(00h) = 14802000h") {
    GpuRig r;
    CHECK_EQ(r.gpu.gpustat(), u32{0x14802000});
    r.gp1(0x08000037); // modo qualquer
    r.gp1(0x03000000); // exibição ligada
    r.gp1(0x04000002);
    r.gp0({0xE6000003, 0xE10001FF});
    CHECK(r.gpu.gpustat() != u32{0x14802000});
    r.gp1(0x00000000);
    CHECK_EQ(r.gpu.gpustat(), u32{0x14802000});
}

TEST_CASE("gpu/número de palavras de cada comando GP0") {
    const std::initializer_list<std::pair<u32, u32>> table = {
        {0x00, 1}, {0x01, 1}, {0x02, 3}, {0x03, 1}, {0x1F, 1},
        {0x20, 4}, {0x24, 7}, {0x28, 5}, {0x2C, 9}, {0x30, 6}, {0x34, 9}, {0x38, 8}, {0x3C, 12},
        {0x40, 3}, {0x50, 4}, {0x48, 0}, {0x58, 0},
        {0x60, 3}, {0x64, 4}, {0x68, 2}, {0x6C, 3}, {0x70, 2}, {0x74, 3}, {0x78, 2}, {0x7C, 3},
        {0x80, 4}, {0x9F, 4}, {0xA0, 3}, {0xBF, 3}, {0xC0, 3}, {0xDF, 3},
        {0xE1, 1}, {0xE6, 1}, {0xFF, 1},
    };
    for (const auto& [op, words] : table) {
        CHECK_EQ(Gpu::command_words(op << 24), words);
    }
    // Bits de semitransparência/modulação (01h/02h) não mudam o tamanho.
    CHECK_EQ(Gpu::command_words(0x3F000000), u32{12});
    CHECK_EQ(Gpu::command_words(0x7F000000), u32{3});
}

TEST_CASE("gpu/preenchimento 02h: cor, passo de 16 em X e arredondamento da largura") {
    GpuRig r;
    // X = 12h → 10h; largura 11h → 20h; cor 00FF8040h → R 8, G 16, B 31.
    r.gp0({0x02FF8040, 0x00050012, 0x00020011});
    const u16 color = 8 | (16 << 5) | (31 << 10);
    CHECK_EQ(r.px(0x10, 5), color);
    CHECK_EQ(r.px(0x2F, 6), color);
    CHECK_EQ(r.px(0x30, 5), u16{0});
    CHECK_EQ(r.px(0x0F, 5), u16{0});
    CHECK_EQ(r.px(0x10, 7), u16{0});
    // 3F1h..3FFh viram 400h (linha inteira); 400h vira 0; altura 0 não preenche.
    r.gp0({0x020000FF, 0x00640000, 0x000103F1});
    CHECK_EQ(r.px(0, 100), u16{31});
    CHECK_EQ(r.px(1023, 100), u16{31});
    r.gp0({0x020000FF, 0x00650000, 0x00010400});
    CHECK_EQ(r.px(0, 101), u16{0});
    r.gp0({0x020000FF, 0x00660000, 0x00000010});
    CHECK_EQ(r.px(0, 102), u16{0});
}

TEST_CASE("gpu/preenchimento ignora a máscara e dá a volta na VRAM") {
    GpuRig r;
    r.gpu.vram().set(0x3F5, 0x1FF, 0x8001);
    r.gp0({0xE6000003}); // gravar com bit 15 e respeitar bit 15: ignorados pelo 02h
    r.gp0({0x02000008, 0x01FF03F0, 0x00020020}); // X 3F0h, Y 1FFh, 32×2
    CHECK_EQ(r.px(0x3F5, 0x1FF), u16{1});
    CHECK_EQ(r.px(0x3FF, 0x1FF), u16{1});
    CHECK_EQ(r.px(0x00F, 0x1FF), u16{1}); // X deu a volta
    CHECK_EQ(r.px(0x005, 0), u16{1});     // Y deu a volta
    CHECK_EQ(r.px(0x010, 0), u16{0});
}

TEST_CASE("gpu/CPU→VRAM (A0h): halfword baixo primeiro, sobra ímpar descartada") {
    GpuRig r;
    r.gp0({0xA0000000, 0x0014000A, 0x00010003});
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{0}); // recebendo dados
    r.gp0({0x22221111});
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{0});
    r.gp0({0xFFFF3333});
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{1});
    CHECK_EQ(r.px(10, 20), u16{0x1111});
    CHECK_EQ(r.px(11, 20), u16{0x2222});
    CHECK_EQ(r.px(12, 20), u16{0x3333});
    CHECK_EQ(r.px(13, 20), u16{0});
    // A palavra seguinte é um comando novo.
    r.gp0({0x020000FF, 0x00400000, 0x00010010});
    CHECK_EQ(r.px(0, 64), u16{31});
}

TEST_CASE("gpu/A0h: tamanho 0 = máximo (largura 400h)") {
    GpuRig r;
    r.gp0({0xA0000000, 0x00000000, 0x00010000}); // 0 × 1 → 1024 × 1 = 512 palavras
    for (u32 i = 0; i < 511; ++i) {
        r.gp0({0x00010001});
    }
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{0});
    r.gp0({0x00020002});
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{1});
    CHECK_EQ(r.px(0, 0), u16{1});
    CHECK_EQ(r.px(1023, 0), u16{2});
    CHECK_EQ(r.px(0, 1), u16{0});
}

TEST_CASE("gpu/VRAM→CPU (C0h) pelo GPUREAD e bit 27") {
    GpuRig r;
    r.gpu.vram().set(10, 20, 0x1111);
    r.gpu.vram().set(11, 20, 0x2222);
    r.gpu.vram().set(12, 20, 0x3333);
    r.gpu.vram().set(10, 21, 0x4444);
    CHECK_EQ(r.stat_bit(stat_ready_send), u32{0});
    r.gp0({0xC0000000, 0x0014000A, 0x00020002}); // 2×2
    CHECK_EQ(r.stat_bit(stat_ready_send), u32{1});
    CHECK_EQ(r.gpu.gpuread(), u32{0x22221111});
    CHECK_EQ(r.stat_bit(stat_ready_send), u32{1});
    CHECK_EQ(r.gpu.gpuread(), u32{0x00004444});
    CHECK_EQ(r.stat_bit(stat_ready_send), u32{0});
    CHECK_EQ(r.gpu.gpuread(), u32{0x00004444}); // fim: GPUREAD mantém o último valor
    // Total ímpar: a metade alta da última palavra é 0 (⚠️ D-042).
    r.gp0({0xC0000000, 0x0014000A, 0x00010003});
    CHECK_EQ(r.gpu.gpuread(), u32{0x22221111});
    CHECK_EQ(r.gpu.gpuread(), u32{0x00003333});
    CHECK_EQ(r.stat_bit(stat_ready_send), u32{0});
}

TEST_CASE("gpu/máscara GP0(E6h) em A0h e 80h") {
    GpuRig r;
    r.gpu.vram().set(0, 0, 0x8001);
    r.gp0({0xE6000002}); // não sobrescrever pixels com bit 15
    r.gp0({0xA0000000, 0x00000000, 0x00010002, 0x12341234});
    CHECK_EQ(r.px(0, 0), u16{0x8001});
    CHECK_EQ(r.px(1, 0), u16{0x1234});
    r.gp0({0x80000000, 0x00000001, 0x00000000, 0x00010001}); // (1,0) → (0,0)
    CHECK_EQ(r.px(0, 0), u16{0x8001});
    r.gp0({0xE6000001}); // gravar com bit 15
    r.gp0({0xA0000000, 0x00000002, 0x00010001, 0x00000005});
    CHECK_EQ(r.px(2, 0), u16{0x8005});
    r.gp0({0x80000000, 0x00000001, 0x00000000, 0x00010001});
    CHECK_EQ(r.px(0, 0), u16{0x9234});
    CHECK_EQ(r.stat_bit(11), u32{1});
    CHECK_EQ(r.stat_bit(12), u32{0});
}

TEST_CASE("gpu/cópia VRAM→VRAM (80h) com volta nas bordas") {
    GpuRig r;
    r.gpu.vram().set(1023, 511, 0x0A0A);
    r.gpu.vram().set(0, 511, 0x0B0B);
    r.gpu.vram().set(1023, 0, 0x0C0C);
    r.gpu.vram().set(0, 0, 0x0D0D);
    r.gp0({0x80000000, 0x01FF03FF, 0x00640064, 0x00020002}); // 2×2 de (1023,511) para (100,100)
    CHECK_EQ(r.px(100, 100), u16{0x0A0A});
    CHECK_EQ(r.px(101, 100), u16{0x0B0B});
    CHECK_EQ(r.px(100, 101), u16{0x0C0C});
    CHECK_EQ(r.px(101, 101), u16{0x0D0D});
}

TEST_CASE("gpu/atributos E1h–E6h, GP1(09h) e GP1(10h) da GPU v2") {
    GpuRig r;
    r.gp0({0xE1003FFF});
    CHECK_EQ(r.gpu.gpustat() & 0x7FFu, u32{0x7FF});
    CHECK_EQ(r.stat_bit(15), u32{1}); // E1h bit 11 = base Y 2 da textura
    CHECK_EQ(r.stat_bit(14), u32{0}); // bits 12–13 (espelhamento) não aparecem
    r.gp0({0xE1000800});
    CHECK_EQ(r.stat_bit(15), u32{1});
    CHECK_EQ(r.gpu.gpustat() & 0x7FFu, u32{0});
    CHECK(!r.gpu.vram_2mb_enabled());
    r.gp1(0x09000001);
    CHECK(r.gpu.vram_2mb_enabled());

    r.gp0({0xE20ABCDE, 0xE3012345, 0xE40FFFFF, 0xE5FFFFFF});
    r.gp1(0x10000002);
    CHECK_EQ(r.gpu.gpuread(), u32{0x000ABCDE});
    r.gp1(0x10000003);
    CHECK_EQ(r.gpu.gpuread(), u32{0x00012345});
    r.gp1(0x10000004);
    CHECK_EQ(r.gpu.gpuread(), u32{0x000FFFFF});
    r.gp1(0x10000005);
    CHECK_EQ(r.gpu.gpuread(), u32{0x003FFFFF});
    r.gp1(0x10000000); // índice sem informação: mantém
    CHECK_EQ(r.gpu.gpuread(), u32{0x003FFFFF});
    r.gp1(0x10000007);
    CHECK_EQ(r.gpu.gpuread(), u32{2});
    r.gp1(0x10000008);
    CHECK_EQ(r.gpu.gpuread(), u32{0});
    r.gp1(0x1F000007); // 10h–1Fh: mesmo comando
    CHECK_EQ(r.gpu.gpuread(), u32{2});
    r.gp1(0x10000008);
    r.gp1(0x50000007); // 40h–FFh espelham 00h–3Fh
    CHECK_EQ(r.gpu.gpuread(), u32{2});
}

TEST_CASE("gpu/GP1(08h), (03h) e (04h) no GPUSTAT") {
    GpuRig r;
    r.gp1(0x0800007F);
    CHECK_EQ(r.stat_bit(16), u32{1});
    CHECK_EQ((r.gpu.gpustat() >> 17) & 3u, u32{3});
    CHECK_EQ(r.stat_bit(19), u32{1}); // 480 linhas (com entrelaçamento)
    CHECK_EQ(r.stat_bit(20), u32{1});
    CHECK_EQ(r.stat_bit(21), u32{1});
    CHECK_EQ(r.stat_bit(22), u32{1});
    r.gp1(0x08000004); // 480 sem entrelaçamento: bit 19 = 0, bit 13 = 1
    CHECK_EQ(r.stat_bit(19), u32{0});
    CHECK_EQ(r.stat_bit(13), u32{1});
    CHECK_EQ(r.stat_bit(23), u32{1});
    r.gp1(0x03000000);
    CHECK_EQ(r.stat_bit(23), u32{0});
    r.gp1(0x03000001);
    CHECK_EQ(r.stat_bit(23), u32{1});
    for (u32 dir = 0; dir < 4; ++dir) {
        r.gp1(0x04000000 | dir);
        CHECK_EQ((r.gpu.gpustat() >> 29) & 3u, dir);
    }
}

TEST_CASE("gpu/DREQ (bit 25) segue a direção do DMA (medido)") {
    GpuRig r;
    r.gp1(0x04000000);
    CHECK_EQ(r.stat_bit(stat_dreq), u32{0});
    CHECK(!r.gpu.dma_request());
    r.gp1(0x04000001);
    CHECK_EQ(r.stat_bit(stat_dreq), u32{1});
    // Direção 2: igual ao bit 28, que cai enquanto um polígono é recebido.
    r.gp1(0x04000002);
    CHECK_EQ(r.stat_bit(stat_dreq), u32{1});
    r.gp0({0x20000000, 0x00000000});
    CHECK_EQ(r.stat_bit(stat_ready_dma), u32{0});
    CHECK_EQ(r.stat_bit(stat_dreq), u32{0});
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{0});
    r.gp0({0x00000001, 0x00000002});
    CHECK_EQ(r.stat_bit(stat_ready_dma), u32{1});
    CHECK_EQ(r.stat_bit(stat_dreq), u32{1});
    // Uma transferência A0h não derruba o bit 28.
    r.gp0({0xA0000000, 0x00000000, 0x00010002});
    CHECK_EQ(r.stat_bit(stat_ready_dma), u32{1});
    r.gp0({0x00000000});
    // Direção 3: igual ao bit 27.
    r.gp1(0x04000003);
    CHECK_EQ(r.stat_bit(stat_dreq), u32{0});
    r.gp0({0xC0000000, 0x00000000, 0x00010002});
    CHECK_EQ(r.stat_bit(stat_dreq), u32{1});
    (void)r.gpu.gpuread();
    CHECK_EQ(r.stat_bit(stat_dreq), u32{0});
}

TEST_CASE("gpu/GP0(1Fh) liga IRQ1 e o bit 24; GP1(02h) reconhece") {
    GpuRig r;
    r.gp0({0x1F000000});
    CHECK_EQ(r.stat_bit(24), u32{1});
    CHECK_EQ(r.t.irq.status() & 2u, u32{2});
    r.t.irq.io_write(0, ~2u, bus::AccessWidth::Word);
    r.gp0({0x1F000000}); // já ligado: sem nova borda
    CHECK_EQ(r.t.irq.status() & 2u, u32{0});
    r.gp1(0x02000000);
    CHECK_EQ(r.stat_bit(24), u32{0});
    r.gp0({0x1F000000});
    CHECK_EQ(r.t.irq.status() & 2u, u32{2});
}

TEST_CASE("gpu/comandos de desenho são consumidos sem dessincronizar o FIFO") {
    GpuRig r;
    // Triângulo gouraud texturizado (9 palavras) e retângulo variável (3), depois um preenchimento.
    r.gp0({0x34FFFFFF, 1, 2, 3, 4, 5, 6, 7, 8});
    r.gp0({0x60FFFFFF, 0x00000000, 0x00100010});
    r.gp0({0x020000FF, 0x00000000, 0x00010010});
    CHECK_EQ(r.px(0, 0), u16{31});
    CHECK_EQ(r.gpu.discarded_draw_commands(), u64{2});
    CHECK_EQ(r.px(5, 5), u16{0}); // nada foi desenhado
}

TEST_CASE("gpu/polilinha termina no marcador 5000_5000h, nunca antes de 2 vértices") {
    GpuRig r;
    // Plana: o 2º vértice (= 50005000h) não é marcador; 55555555h encerra.
    r.gp0({0x48FFFFFF, 0x00000000, 0x50005000, 0x00100010, 0x55555555});
    r.gp0({0x020000FF, 0x00000000, 0x00010010});
    CHECK_EQ(r.px(0, 0), u16{31});
    // Gouraud: marcador só na posição de cor; um vértice 55555555h não encerra.
    r.gp0({0x58FFFFFF, 0x00000000, 0x00FF0000, 0x00100010, 0x0000FF00, 0x55555555,
           0x55555555});
    r.gp0({0x02FF0000, 0x00010000, 0x00010010});
    CHECK_EQ(r.px(0, 1), u16{31 << 10});
    CHECK_EQ(r.gpu.discarded_draw_commands(), u64{2});
}

TEST_CASE("gpu/GP1(01h) esvazia o FIFO; GP1(00h) preserva VRAM e GP1(09h)") {
    GpuRig r;
    r.gp0({0x02FFFFFF, 0x00000000}); // comando incompleto
    r.gp1(0x01000000);
    CHECK_EQ(r.stat_bit(stat_ready_cmd), u32{1});
    r.gp0({0x020000FF, 0x00000000, 0x00010010});
    CHECK_EQ(r.px(0, 0), u16{31});
    r.gp1(0x09000001);
    r.gp0({0xE1000800});
    r.gp1(0x00000000);
    CHECK_EQ(r.stat_bit(15), u32{0}); // E1h zerado
    CHECK_EQ(r.px(0, 0), u16{31});
    CHECK(r.gpu.vram_2mb_enabled());  // GP1(09h) não é afetado por GP1(00h)
    r.gpu.power_on();
    CHECK(!r.gpu.vram_2mb_enabled());
    CHECK_EQ(r.px(0, 0), u16{0});
}

TEST_CASE("gpu/GP1(06h–08h) chegam à temporização de vídeo no próximo HSYNC") {
    GpuRig r;
    r.gp1(0x08000009); // PAL, 320 pontos
    r.gp1(0x06C60260);
    r.gp1(0x07048020);
    CHECK(r.t.video.display_timing().mode == gpu::VideoMode::Ntsc);
    r.t.advance_to(5000);
    const gpu::DisplayTiming& t = r.t.video.display_timing();
    CHECK(t.mode == gpu::VideoMode::Pal);
    CHECK_EQ(t.dot_divider, u32{8});
    CHECK_EQ(t.x1, u32{0x260});
    CHECK_EQ(t.x2, u32{0xC60});
    CHECK_EQ(t.y1, u32{0x020});
    CHECK_EQ(t.y2, u32{0x120});
    CHECK(!t.interlaced);
    CHECK_EQ(r.t.video.line_length(), u32{3406});
    r.gp1(0x08000060); // 368 pontos + entrelaçado
    r.t.advance_to(10000);
    CHECK_EQ(r.t.video.display_timing().dot_divider, u32{7});
    CHECK(r.t.video.display_timing().interlaced);
}

TEST_CASE("gpu/tamanho da área de exibição") {
    GpuRig r;
    gpu::DisplayInfo d = r.gpu.display_info();
    CHECK_EQ(d.width, u32{256}); // valores de GP1(00h)
    CHECK_EQ(d.height, u32{240});
    CHECK(!d.enabled);
    r.gp1(0x06C60260);
    r.gp1(0x08000001);
    CHECK_EQ(r.gpu.display_info().width, u32{320});
    r.gp1(0x08000040);
    CHECK_EQ(r.gpu.display_info().width, u32{364}); // 2560/7 = 365; (365+2) AND NOT 3
    r.gp1(0x08000027);
    d = r.gpu.display_info();
    CHECK_EQ(d.width, u32{640});
    CHECK_EQ(d.height, u32{480});
    r.gp1(0x07FFC010); // Y2 além do quadro NTSC (263 linhas)
    CHECK_EQ(r.gpu.display_info().height, u32{(263 - 0x10) * 2});
    r.gp1(0x06000500); // X2 < X1
    CHECK_EQ(r.gpu.display_info().width, u32{0});
    r.gp1(0x05040C08); // início (8, 259)
    d = r.gpu.display_info();
    CHECK_EQ(d.start_x, u32{8});
    CHECK_EQ(d.start_y, u32{259});
}

TEST_CASE("gpu/imagem de 15 bits a partir da VRAM") {
    GpuRig r;
    r.gp1(0x06C60260);
    r.gp1(0x07040010);
    r.gp1(0x08000001);
    r.gp1(0x05000802); // início (2, 2)
    r.gp1(0x03000000);
    r.gpu.vram().set(2, 2, 0x7FFF);
    r.gpu.vram().set(3, 2, 0x001F);
    r.gpu.vram().set(2, 3, 0x8010); // bit 15 não aparece na imagem
    r.gpu.vram().set(3, 3, 0x03E0);
    video::Framebuffer fb;
    r.gpu.render_display(fb);
    CHECK_EQ(fb.width(), u32{320});
    CHECK_EQ(fb.height(), u32{240});
    CHECK_EQ(fb.pixel(0, 0), u32{0xFFFFFF});
    CHECK_EQ(fb.pixel(1, 0), u32{0xFF0000});
    CHECK_EQ(fb.pixel(0, 1), u32{0x840000});
    CHECK_EQ(fb.pixel(1, 1), u32{0x00FF00});
    CHECK_EQ(fb.pixel(2, 0), u32{0});
}

TEST_CASE("gpu/imagem de 24 bits: fluxo de bytes R, G, B") {
    GpuRig r;
    r.gp1(0x06C60260);
    r.gp1(0x08000011); // 320 pontos, 24 bits
    r.gp1(0x03000000);
    r.gpu.vram().set(0, 0, 0x2211);
    r.gpu.vram().set(1, 0, 0x4433);
    r.gpu.vram().set(2, 0, 0x6655);
    r.gpu.vram().set(3, 0, 0x8877);
    video::Framebuffer fb;
    r.gpu.render_display(fb);
    CHECK_EQ(fb.pixel(0, 0), u32{0x112233});
    CHECK_EQ(fb.pixel(1, 0), u32{0x445566});
    r.gp1(0x05000001); // X inicial em halfwords: começa no byte 2
    r.gpu.render_display(fb);
    CHECK_EQ(fb.pixel(0, 0), u32{0x334455});
    CHECK_EQ(fb.pixel(1, 0), u32{0x667788});
}

TEST_CASE("gpu/exibição desligada gera imagem preta") {
    GpuRig r;
    r.gpu.vram().clear(0x7FFF);
    video::Framebuffer fb;
    r.gpu.render_display(fb);
    CHECK_EQ(fb.width(), u32{256});
    CHECK_EQ(fb.pixel(10, 10), u32{0});
    r.gp1(0x03000000);
    r.gpu.render_display(fb);
    CHECK_EQ(fb.pixel(10, 10), u32{0xFFFFFF});
}

TEST_CASE("gpu/acesso pelo barramento em 1F801810h/1F801814h (Console)") {
    system::Console c;
    c.power_on();
    CHECK_EQ(c.bus().read32(0x1F801814).value, u32{0x14802000});
    CHECK_EQ(c.bus().read16(0x1F801816).value, u32{0x1480});
    CHECK(c.bus().write32(0x1F801810, 0x020000FF) == bus::BusFault::None);
    (void)c.bus().write32(0x1F801810, 0x00000000);
    (void)c.bus().write32(0x1F801810, 0x00010010);
    CHECK_EQ(c.gpu().vram().get(15, 0), u16{31});
    (void)c.bus().write32(0x1F801814, 0x10000007);
    CHECK_EQ(c.bus().read32(0x1F801810).value, u32{2});
    // O reset do console reinicia a GPU, mas a VRAM mantém o conteúdo.
    (void)c.bus().write32(0x1F801814, 0x03000000);
    c.reset();
    CHECK_EQ(c.gpu().gpustat(), u32{0x14802000});
    CHECK_EQ(c.gpu().vram().get(15, 0), u16{31});
}

TEST_CASE("gpu/DMA canal 2: lista encadeada, blocos para a VRAM e leitura de volta") {
    system::Console c;
    c.power_on();
    bus::Bus& b = c.bus();
    const auto w = [&](u32 address, u32 value) { (void)b.write32(address, value); };
    // A RAM zerada é uma sequência de NOPs: executá-los faz o tempo (e o DMA) avançar.
    c.cpu().set_pc(0x80010000);
    const auto wait_dma = [&] {
        c.run(200);
        REQUIRE(!c.dma().active(2));
    };
    w(0x1F8010F0, 0x07654B21); // DPCR: habilita o canal 2

    // Lista: nó 1 = preenchimento; nó 2 vazio; fim.
    w(0x00001000, 0x03001100);
    w(0x00001004, 0x020000FF);
    w(0x00001008, 0x00000000);
    w(0x0000100C, 0x00010010);
    w(0x00001100, 0x00FFFFFF);
    w(0x1F801814, 0x04000002);
    w(0x1F8010A0, 0x00001000);
    w(0x1F8010A4, 0);
    w(0x1F8010A8, 0x01000401);
    CHECK_EQ(c.gpu().vram().get(0, 0), u16{31});
    CHECK_EQ(c.dma().words_transferred(2), u64{3});
    wait_dma();

    // A0h pela CPU e os dados (4×2 = 4 palavras) em 2 blocos de 2 palavras por DMA.
    w(0x1F801810, 0xA0000000);
    w(0x1F801810, 0x00200040);
    w(0x1F801810, 0x00020004);
    for (u32 i = 0; i < 4; ++i) {
        w(0x00002000 + 4 * i, ((2 * i + 2) << 16) | (2 * i + 1));
    }
    w(0x1F8010A0, 0x00002000);
    w(0x1F8010A4, 0x00020002);
    w(0x1F8010A8, 0x01000201);
    CHECK_EQ(c.gpu().vram().get(64, 32), u16{1});
    CHECK_EQ(c.gpu().vram().get(67, 32), u16{4});
    CHECK_EQ(c.gpu().vram().get(67, 33), u16{8});
    CHECK_EQ((c.gpu().gpustat() >> 26) & 1u, u32{1});
    wait_dma();

    // GPUREAD → RAM: o canal espera o DREQ (bit 27) e começa quando a CPU envia C0h.
    w(0x1F801814, 0x04000003);
    w(0x1F8010A0, 0x00003000);
    w(0x1F8010A4, 0x00020002);
    w(0x1F8010A8, 0x01000200);
    CHECK((c.dma().chcr_value(2) & dma::chcr::busy) != 0); // armado, sem DREQ
    CHECK(!c.dma().active(2));
    CHECK_EQ(c.dma().words_transferred(2), u64{7});
    w(0x1F801810, 0xC0000000);
    w(0x1F801810, 0x00200040);
    w(0x1F801810, 0x00020004);
    CHECK_EQ(b.read32(0x00003000).value, u32{0x00020001});
    CHECK_EQ(b.read32(0x0000300C).value, u32{0x00080007});
    CHECK_EQ((c.gpu().gpustat() >> 27) & 1u, u32{0});
}
