// Imagem de saída apresentada pela janela do hospedeiro.
//
// Na Fase 0 o framebuffer permanece preto (sem sinal), pois a GPU do PS1
// ainda não existe. A partir da Fase 7 ele receberá a área de exibição da VRAM.
#pragma once

#include <vector>

#include "core/types.hpp"

namespace rpcs1::video {

class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(u32 width, u32 height, u32 fill = 0);

    void resize(u32 width, u32 height, u32 fill = 0);
    void clear(u32 color) noexcept;

    [[nodiscard]] u32 width() const noexcept { return width_; }
    [[nodiscard]] u32 height() const noexcept { return height_; }
    [[nodiscard]] bool empty() const noexcept { return pixels_.empty(); }

    // Pixel no formato 0x00RRGGBB. Coordenadas fora da imagem: leitura retorna 0
    // e escrita é ignorada.
    [[nodiscard]] u32 pixel(u32 x, u32 y) const noexcept;
    void set_pixel(u32 x, u32 y, u32 color) noexcept;

    [[nodiscard]] const u32* data() const noexcept { return pixels_.data(); }

    // Proporção de exibição desejada (a TV mostra a imagem do PS1 em 4:3).
    double display_aspect = 4.0 / 3.0;

    static constexpr u32 max_dimension = 4096;

private:
    u32 width_ = 0;
    u32 height_ = 0;
    std::vector<u32> pixels_;
};

} // namespace rpcs1::video
