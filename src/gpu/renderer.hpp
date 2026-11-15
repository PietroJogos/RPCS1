// Rasterizador por software da GPU (Fase 8): triângulos, linhas e retângulos na VRAM.
//
// Fontes: psx-spx "GPU" e psx.wiki (hardware/gpu/gp0-drawing, rendering, vram, gp0-environment;
// testes "gpu-rasterization" e "gpu-primitives" medidos em SCPH-5501).
//
// Regras implementadas:
//   - Vértices: 11 bits com sinal; o deslocamento de desenho (E5h) é somado e o resultado volta a
//     11 bits com sinal. Triângulos e linhas com distância > 1023 em X ou > 511 em Y entre vértices
//     não são desenhados (medido; vale por triângulo de um quadrilátero).
//   - Triângulos: amostragem nas coordenadas inteiras dos pixels; pixels sobre arestas esquerdas
//     e superiores são desenhados, sobre arestas direitas e inferiores não (medido: o triângulo
//     0,0 / 10,0 / 0,10 desenha x + y ≤ 9; arestas compartilhadas são desenhadas uma vez).
//     Quadriláteros = triângulos 1-2-3 e 2-3-4.
//   - Cor gouraud e UV: interpolação afim exata, arredondada para o mais próximo (medido em UV).
//   - Linhas: incluem os dois extremos; extremos iguais = 1 pixel; sempre com dithering quando
//     ligado (medido).
//   - Texturas 4/8/15 bits com CLUT, janela de textura (E2h), texel 0000h transparente,
//     modulação (texel × cor) / 128 saturada, texturas "cruas".
//   - Semitransparência (B/2 + F/2, B + F, B − F, B + F/4) em valores de 5 bits (medido no modo 0);
//     em primitivas texturizadas só para texels com bit 15.
//   - Dithering 4×4 sobre valores de 8 bits (medido), em polígonos gouraud ou modulados e em
//     linhas; nunca em retângulos.
//   - Bit de máscara (E6h) e área de desenho (E3h/E4h, inclusiva).
// Escolhas não documentadas: docs/DECISIONS.md, D-044 a D-047.
#pragma once

#include "core/types.hpp"
#include "gpu/vram.hpp"

namespace rpcs1::gpu {

// Estado de desenho vindo de GP0(E1h–E6h).
struct DrawEnv {
    u32 texpage = 0;        // E1h bits 0–13 (bits 0–8 e 11 também vêm de polígonos texturizados)
    u32 texture_window = 0; // E2h bits 0–19
    u32 area_top_left = 0;  // E3h
    u32 area_bottom_right = 0;
    u32 draw_offset = 0;    // E5h bits 0–21
    bool mask_set = false;
    bool mask_check = false;
};

struct Vertex {
    s32 x = 0;
    s32 y = 0;
    u32 color = 0; // 00BBGGRRh (8 bits por canal)
    u32 u = 0;
    u32 v = 0;
};

struct PrimitiveFlags {
    bool gouraud = false;
    bool textured = false;
    bool raw = false;  // textura sem modulação (bit 24)
    bool semi = false; // semitransparente (bit 25)
};

// Estende o sinal de um valor de 11 bits.
[[nodiscard]] constexpr s32 sign_extend11(u32 value) noexcept {
    return static_cast<s32>((value & 0x7FFu) ^ 0x400u) - 0x400;
}

// Valor da matriz de dithering 4×4 para o pixel (x, y) (psx.wiki "rendering", conferido pela
// medição de linha gouraud com dithering).
[[nodiscard]] s32 dither_offset(s32 x, s32 y) noexcept;

class Renderer {
public:
    explicit Renderer(Vram& vram) : vram_(vram) {}

    // `clut` e `texpage` são as metades altas das palavras de UV (texpage já aplicado ao
    // DrawEnv pelo chamador). Coordenadas ainda sem o deslocamento de desenho.
    void draw_triangle(const DrawEnv& env, const Vertex (&v)[3], const PrimitiveFlags& flags,
                       u32 clut);
    void draw_line(const DrawEnv& env, const Vertex& a, const Vertex& b, bool gouraud, bool semi);
    void draw_rectangle(const DrawEnv& env, const Vertex& origin, u32 width, u32 height,
                        const PrimitiveFlags& flags, u32 clut);

    [[nodiscard]] u64 pixels_drawn() const noexcept { return pixels_; }

    struct Context; // parâmetros de uma primitiva já decodificados (renderer.cpp)

private:
    void shade(const Context& ctx, s32 x, s32 y, u32 r, u32 g, u32 b, u32 u, u32 v);
    [[nodiscard]] u16 fetch_texel(const Context& ctx, u32 u, u32 v) const;

    Vram& vram_;
    u64 pixels_ = 0;
};

} // namespace rpcs1::gpu
