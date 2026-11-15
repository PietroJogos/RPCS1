// VRAM da GPU: 1 MiB = 1024 × 512 pixels de 16 bits (Fase 7).
//
// Pixel: bits 0–4 R, 5–9 G, 10–14 B, bit 15 = máscara. A VRAM não é mapeada no barramento
// da CPU: só é acessada pela GPU (GP0/GP1) e pelo DMA. Coordenadas dão a volta nas bordas.
#pragma once

#include <memory>

#include "core/types.hpp"

namespace rpcs1::gpu {

class Vram {
public:
    static constexpr u32 width = 1024;
    static constexpr u32 height = 512;

    Vram() : pixels_(std::make_unique<u16[]>(width * height)) {}

    [[nodiscard]] u16 get(u32 x, u32 y) const noexcept {
        return pixels_[index(x, y)];
    }
    void set(u32 x, u32 y, u16 value) noexcept { pixels_[index(x, y)] = value; }
    void clear(u16 value = 0) noexcept {
        for (u32 i = 0; i < width * height; ++i) {
            pixels_[i] = value;
        }
    }

    // Byte do fluxo de uma linha (usado pela exibição em 24 bits).
    [[nodiscard]] u8 byte_at(u32 byte_x, u32 y) const noexcept {
        const u16 hw = get(byte_x >> 1, y);
        return static_cast<u8>((byte_x & 1u) ? hw >> 8 : hw);
    }

private:
    [[nodiscard]] static constexpr u32 index(u32 x, u32 y) noexcept {
        return (y & (height - 1)) * width + (x & (width - 1));
    }
    std::unique_ptr<u16[]> pixels_;
};

// RGB555 (+ bit de máscara) → 0x00RRGGBB, expandindo 5 bits para 8 (x << 3 | x >> 2).
[[nodiscard]] constexpr u32 rgb555_to_rgb888(u16 pixel) noexcept {
    const u32 r = pixel & 0x1Fu;
    const u32 g = (pixel >> 5) & 0x1Fu;
    const u32 b = (pixel >> 10) & 0x1Fu;
    const auto expand = [](u32 c) { return (c << 3) | (c >> 2); };
    return (expand(r) << 16) | (expand(g) << 8) | expand(b);
}

// Cor de comando 24 bits (00BBGGRRh) → RGB555 (mantém os 5 bits altos de cada canal).
[[nodiscard]] constexpr u16 command_color_to_rgb555(u32 color) noexcept {
    const u32 r = (color >> 3) & 0x1Fu;
    const u32 g = (color >> 11) & 0x1Fu;
    const u32 b = (color >> 19) & 0x1Fu;
    return static_cast<u16>(r | (g << 5) | (b << 10));
}

} // namespace rpcs1::gpu
