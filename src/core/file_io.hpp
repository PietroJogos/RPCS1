// Leitura e escrita robustas de arquivos.
//
// Toda leitura exige um limite máximo de tamanho, para que arquivos inesperados
// (ex.: uma imagem de disco passada no lugar da BIOS) não esgotem a memória.
// A escrita é atômica: grava em um arquivo temporário e o renomeia por cima do
// destino, de modo que uma falha no meio da operação não corrompe o original
// (essencial para Memory Cards e configurações).
#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/types.hpp"

namespace rpcs1::file_io {

[[nodiscard]] Result<std::vector<u8>> read_binary(const std::filesystem::path& path,
                                                  usize max_size);
[[nodiscard]] Result<std::string> read_text(const std::filesystem::path& path, usize max_size);

[[nodiscard]] Status write_atomic(const std::filesystem::path& path, std::span<const u8> data);
[[nodiscard]] Status write_atomic(const std::filesystem::path& path, std::string_view text);

} // namespace rpcs1::file_io
