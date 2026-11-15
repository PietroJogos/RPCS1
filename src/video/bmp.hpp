// Gravação de um Framebuffer como imagem BMP (24 bits, sem compressão). (Fase 7)
//
// Formato Windows BITMAPINFOHEADER: cabeçalho de 14 + 40 bytes, linhas de baixo para cima,
// pixels em B, G, R e cada linha completada com zeros até múltiplo de 4 bytes.
// Usado por --dump-frame para registrar a imagem que a GPU emulada realmente gerou.
#pragma once

#include <filesystem>
#include <vector>

#include "core/error.hpp"
#include "core/types.hpp"
#include "video/framebuffer.hpp"

namespace rpcs1::video {

[[nodiscard]] std::vector<u8> encode_bmp(const Framebuffer& image);
[[nodiscard]] Status save_bmp(const std::filesystem::path& path, const Framebuffer& image);

} // namespace rpcs1::video
