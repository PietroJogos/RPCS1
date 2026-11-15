#include "gpu/renderer.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace rpcs1::gpu {

namespace {

constexpr s32 dither_table[4][4] = {
    {-4, +0, -3, +1},
    {+2, -2, +3, -1},
    {-3, +1, -4, +0},
    {+3, -1, +2, -2},
};

// Maior distância entre vértices de uma primitiva desenhável (medido: 1023 × 511).
constexpr s32 max_width = 1023;
constexpr s32 max_height = 511;

// floor(a / b) para b > 0.
constexpr s64 floor_div(s64 a, s64 b) noexcept {
    const s64 q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

// a / b arredondado para o mais próximo (metade para cima), b > 0.
constexpr s64 div_round(s64 a, s64 b) noexcept {
    return floor_div(2 * a + b, 2 * b);
}

constexpr u32 channel(u32 color, u32 shift) noexcept { return (color >> shift) & 0xFFu; }

s32 apply_offset(s32 coord, s32 offset) noexcept {
    return sign_extend11(static_cast<u32>(coord + offset));
}

} // namespace

s32 dither_offset(s32 x, s32 y) noexcept {
    return dither_table[y & 3][x & 3];
}

struct Renderer::Context {
    s32 x1 = 0, y1 = 0, x2 = 0, y2 = 0; // área de desenho (inclusiva)
    bool textured = false;
    bool raw = false;
    bool semi = false;
    bool dither = false;
    bool mask_set = false;
    bool mask_check = false;
    u32 semi_mode = 0;
    u32 tex_mode = 0; // 0 = 4 bits, 1 = 8 bits, 2/3 = 15 bits
    u32 tex_x = 0, tex_y = 0;
    u32 clut_x = 0, clut_y = 0;
    u32 win_and_u = 0xFF, win_or_u = 0, win_and_v = 0xFF, win_or_v = 0;

    [[nodiscard]] bool inside(s32 x, s32 y) const noexcept {
        return x >= x1 && x <= x2 && y >= y1 && y <= y2;
    }
};

namespace {

void setup_common(Renderer::Context& ctx, const DrawEnv& env, const PrimitiveFlags& flags, u32 clut) {
    ctx.x1 = static_cast<s32>(env.area_top_left & 0x3FFu);
    ctx.y1 = static_cast<s32>((env.area_top_left >> 10) & 0x3FFu);
    ctx.x2 = static_cast<s32>(env.area_bottom_right & 0x3FFu);
    ctx.y2 = static_cast<s32>((env.area_bottom_right >> 10) & 0x3FFu);
    ctx.textured = flags.textured;
    ctx.raw = flags.textured && flags.raw;
    ctx.semi = flags.semi;
    ctx.mask_set = env.mask_set;
    ctx.mask_check = env.mask_check;
    ctx.semi_mode = (env.texpage >> 5) & 3u;
    ctx.tex_mode = (env.texpage >> 7) & 3u;
    ctx.tex_x = (env.texpage & 0x0Fu) * 64;
    ctx.tex_y = ((env.texpage >> 4) & 1u) * 256;
    ctx.clut_x = (clut & 0x3Fu) * 16;
    ctx.clut_y = (clut >> 6) & 0x1FFu;
    // E2h: texcoord = (texcoord AND NOT (máscara × 8)) OR ((deslocamento AND máscara) × 8).
    const u32 mask_x = env.texture_window & 0x1Fu;
    const u32 mask_y = (env.texture_window >> 5) & 0x1Fu;
    const u32 off_x = (env.texture_window >> 10) & 0x1Fu;
    const u32 off_y = (env.texture_window >> 15) & 0x1Fu;
    ctx.win_and_u = ~(mask_x * 8) & 0xFFu;
    ctx.win_or_u = (off_x & mask_x) * 8;
    ctx.win_and_v = ~(mask_y * 8) & 0xFFu;
    ctx.win_or_v = (off_y & mask_y) * 8;
}

} // namespace

u16 Renderer::fetch_texel(const Context& ctx, u32 u, u32 v) const {
    u = (u & ctx.win_and_u) | ctx.win_or_u;
    v = (v & ctx.win_and_v) | ctx.win_or_v;
    const u32 y = ctx.tex_y + v;
    switch (ctx.tex_mode) {
    case 0: { // 4 bits: 4 índices por halfword, o primeiro nos bits 0–3
        const u16 hw = vram_.get(ctx.tex_x + (u >> 2), y);
        const u32 index = (hw >> ((u & 3u) * 4)) & 0x0Fu;
        return vram_.get(ctx.clut_x + index, ctx.clut_y);
    }
    case 1: { // 8 bits: 2 índices por halfword, o primeiro nos bits 0–7
        const u16 hw = vram_.get(ctx.tex_x + (u >> 1), y);
        const u32 index = (hw >> ((u & 1u) * 8)) & 0xFFu;
        return vram_.get(ctx.clut_x + index, ctx.clut_y);
    }
    default: // 15 bits direto (3 = reservado, igual a 2)
        return vram_.get(ctx.tex_x + u, y);
    }
}

void Renderer::shade(const Context& ctx, s32 x, s32 y, u32 r, u32 g, u32 b, u32 u, u32 v) {
    u32 out = 0;      // 15 bits de cor
    bool mask_bit = false;
    bool blend = ctx.semi;
    if (ctx.textured) {
        const u16 texel = fetch_texel(ctx, u, v);
        if (texel == 0) {
            return; // 0000h: transparente
        }
        mask_bit = (texel & 0x8000u) != 0;
        blend = ctx.semi && mask_bit;
        if (ctx.raw) {
            out = texel & 0x7FFFu;
        } else {
            // Modulação: (texel × cor) / 128 sobre 8 bits; 80h = neutro.
            r = ((texel & 0x1Fu) << 3) * r >> 7;
            g = (((texel >> 5) & 0x1Fu) << 3) * g >> 7;
            b = (((texel >> 10) & 0x1Fu) << 3) * b >> 7;
        }
    }
    if (!ctx.textured || !ctx.raw) {
        const s32 d = ctx.dither ? dither_offset(x, y) : 0;
        const auto quantize = [d](u32 c) {
            return static_cast<u32>(std::clamp(static_cast<s32>(c) + d, 0, 255)) >> 3;
        };
        out = quantize(r) | (quantize(g) << 5) | (quantize(b) << 10);
    }

    const u32 vx = static_cast<u32>(x);
    const u32 vy = static_cast<u32>(y);
    const u16 back = vram_.get(vx, vy);
    if (ctx.mask_check && (back & 0x8000u)) {
        return;
    }
    if (blend) {
        u32 result = 0;
        for (u32 shift = 0; shift < 15; shift += 5) {
            const s32 bc = static_cast<s32>((back >> shift) & 0x1Fu);
            const s32 fc = static_cast<s32>((out >> shift) & 0x1Fu);
            s32 c = 0;
            switch (ctx.semi_mode) {
            case 0: c = (bc + fc) >> 1; break;
            case 1: c = bc + fc; break;
            case 2: c = bc - fc; break;
            default: c = bc + (fc >> 2); break;
            }
            result |= static_cast<u32>(std::clamp(c, 0, 31)) << shift;
        }
        out = result;
    }
    if (mask_bit || ctx.mask_set) {
        out |= 0x8000u;
    }
    vram_.set(vx, vy, static_cast<u16>(out));
    ++pixels_;
}

// --------------------------------------------------------------------------- triângulos

void Renderer::draw_triangle(const DrawEnv& env, const Vertex (&in)[3], const PrimitiveFlags& flags,
                             u32 clut) {
    Context ctx;
    setup_common(ctx, env, flags, clut);
    const bool dtd = ((env.texpage >> 9) & 1u) != 0;
    ctx.dither = dtd && (flags.gouraud || (flags.textured && !flags.raw));

    const s32 ox = sign_extend11(env.draw_offset & 0x7FFu);
    const s32 oy = sign_extend11((env.draw_offset >> 11) & 0x7FFu);
    Vertex v[3] = {in[0], in[1], in[2]};
    for (Vertex& p : v) {
        p.x = apply_offset(p.x, ox);
        p.y = apply_offset(p.y, oy);
    }
    const s32 min_x = std::min({v[0].x, v[1].x, v[2].x});
    const s32 max_x = std::max({v[0].x, v[1].x, v[2].x});
    const s32 min_y = std::min({v[0].y, v[1].y, v[2].y});
    const s32 max_y = std::max({v[0].y, v[1].y, v[2].y});
    if (max_x - min_x > max_width || max_y - min_y > max_height) {
        return; // não desenhado (medido)
    }
    const auto edge = [](const Vertex& a, const Vertex& b, s64 px, s64 py) {
        return static_cast<s64>(b.x - a.x) * (py - a.y) - static_cast<s64>(b.y - a.y) * (px - a.x);
    };
    s64 area = edge(v[0], v[1], v[2].x, v[2].y);
    if (area == 0) {
        return; // ⚠️ triângulo degenerado: nada é desenhado
    }
    if (area < 0) {
        std::swap(v[1], v[2]);
        area = -area;
    }
    // Pixel sobre a aresta a→b é desenhado se a aresta for esquerda (interior em +x) ou superior
    // (horizontal com interior em +y).
    const auto top_left = [](const Vertex& a, const Vertex& b) {
        const s32 dedx = -(b.y - a.y);
        const s32 dedy = b.x - a.x;
        return dedx > 0 || (dedx == 0 && dedy > 0);
    };
    const bool incl0 = top_left(v[1], v[2]);
    const bool incl1 = top_left(v[2], v[0]);
    const bool incl2 = top_left(v[0], v[1]);

    const s32 x_begin = std::max(min_x, ctx.x1);
    const s32 x_end = std::min(max_x, ctx.x2);
    const s32 y_begin = std::max(min_y, ctx.y1);
    const s32 y_end = std::min(max_y, ctx.y2);

    const auto interpolate = [&](u32 a0, u32 a1, u32 a2, s64 w0, s64 w1, s64 w2) {
        const s64 num = static_cast<s64>(a0) * w0 + static_cast<s64>(a1) * w1 +
                        static_cast<s64>(a2) * w2;
        return static_cast<u32>(std::clamp<s64>(div_round(num, area), 0, 255));
    };

    for (s32 y = y_begin; y <= y_end; ++y) {
        for (s32 x = x_begin; x <= x_end; ++x) {
            const s64 w0 = edge(v[1], v[2], x, y);
            const s64 w1 = edge(v[2], v[0], x, y);
            const s64 w2 = edge(v[0], v[1], x, y);
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            if ((w0 == 0 && !incl0) || (w1 == 0 && !incl1) || (w2 == 0 && !incl2)) continue;
            u32 r = channel(v[0].color, 0);
            u32 g = channel(v[0].color, 8);
            u32 b = channel(v[0].color, 16);
            if (flags.gouraud) {
                r = interpolate(channel(v[0].color, 0), channel(v[1].color, 0), channel(v[2].color, 0), w0, w1, w2);
                g = interpolate(channel(v[0].color, 8), channel(v[1].color, 8), channel(v[2].color, 8), w0, w1, w2);
                b = interpolate(channel(v[0].color, 16), channel(v[1].color, 16), channel(v[2].color, 16), w0, w1, w2);
            }
            u32 u = 0;
            u32 tv = 0;
            if (flags.textured) {
                u = interpolate(v[0].u, v[1].u, v[2].u, w0, w1, w2);
                tv = interpolate(v[0].v, v[1].v, v[2].v, w0, w1, w2);
            }
            shade(ctx, x, y, r, g, b, u, tv);
        }
    }
}

// ------------------------------------------------------------------------------ linhas

void Renderer::draw_line(const DrawEnv& env, const Vertex& a_in, const Vertex& b_in, bool gouraud,
                         bool semi) {
    Context ctx;
    PrimitiveFlags flags;
    flags.semi = semi;
    setup_common(ctx, env, flags, 0);
    ctx.dither = ((env.texpage >> 9) & 1u) != 0; // linhas: sempre que ligado (medido)

    const s32 ox = sign_extend11(env.draw_offset & 0x7FFu);
    const s32 oy = sign_extend11((env.draw_offset >> 11) & 0x7FFu);
    Vertex a = a_in;
    Vertex b = b_in;
    if (!gouraud) {
        b.color = a.color;
    }
    a.x = apply_offset(a.x, ox);
    a.y = apply_offset(a.y, oy);
    b.x = apply_offset(b.x, ox);
    b.y = apply_offset(b.y, oy);
    if (std::abs(b.x - a.x) > max_width || std::abs(b.y - a.y) > max_height) {
        return;
    }
    // ⚠️ Direção canônica (de cima para baixo, depois da esquerda para a direita), para que a
    // linha seja idêntica nos dois sentidos (medido: simétrica).
    if (b.y < a.y || (b.y == a.y && b.x < a.x)) {
        std::swap(a, b);
    }
    const s64 dx = b.x - a.x;
    const s64 dy = b.y - a.y;
    const s64 steps = std::max(std::abs(dx), std::abs(dy));
    for (s64 i = 0; i <= steps; ++i) {
        s32 x = a.x;
        s32 y = a.y;
        u32 r = channel(a.color, 0);
        u32 g = channel(a.color, 8);
        u32 bl = channel(a.color, 16);
        if (steps > 0) {
            x = a.x + static_cast<s32>(div_round(dx * i, steps));
            y = a.y + static_cast<s32>(div_round(dy * i, steps));
            const auto lerp = [&](u32 c0, u32 c1) {
                return static_cast<u32>(static_cast<s64>(c0) +
                                        div_round((static_cast<s64>(c1) - c0) * i, steps));
            };
            r = lerp(channel(a.color, 0), channel(b.color, 0));
            g = lerp(channel(a.color, 8), channel(b.color, 8));
            bl = lerp(channel(a.color, 16), channel(b.color, 16));
        }
        if (ctx.inside(x, y)) {
            shade(ctx, x, y, r, g, bl, 0, 0);
        }
    }
}

// -------------------------------------------------------------------------- retângulos

void Renderer::draw_rectangle(const DrawEnv& env, const Vertex& origin, u32 width, u32 height,
                              const PrimitiveFlags& flags, u32 clut) {
    Context ctx;
    setup_common(ctx, env, flags, clut);
    ctx.dither = false; // retângulos nunca têm dithering

    const s32 ox = sign_extend11(env.draw_offset & 0x7FFu);
    const s32 oy = sign_extend11((env.draw_offset >> 11) & 0x7FFu);
    const s32 x0 = apply_offset(origin.x, ox);
    const s32 y0 = apply_offset(origin.y, oy);
    const bool flip_x = ((env.texpage >> 12) & 1u) != 0;
    const bool flip_y = ((env.texpage >> 13) & 1u) != 0;
    const u32 r = channel(origin.color, 0);
    const u32 g = channel(origin.color, 8);
    const u32 b = channel(origin.color, 16);
    for (u32 dy = 0; dy < height; ++dy) {
        const s32 y = y0 + static_cast<s32>(dy);
        if (y < ctx.y1 || y > ctx.y2) continue;
        // ⚠️ Com espelhamento a coordenada decresce a partir de U/V (D-046).
        const u32 tv = (flip_y ? origin.v - dy : origin.v + dy) & 0xFFu;
        for (u32 dx = 0; dx < width; ++dx) {
            const s32 x = x0 + static_cast<s32>(dx);
            if (x < ctx.x1 || x > ctx.x2) continue;
            const u32 u = (flip_x ? origin.u - dx : origin.u + dx) & 0xFFu;
            shade(ctx, x, y, r, g, b, u, tv);
        }
    }
}

} // namespace rpcs1::gpu
