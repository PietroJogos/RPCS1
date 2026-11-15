#include "memory/bios_rom.hpp"

#include <algorithm>
#include <format>

#include "core/file_io.hpp"
#include "core/log.hpp"
#include "core/unicode.hpp"

namespace rpcs1::memory {

Status BiosRom::load(std::span<const u8> image) {
    if (image.size() != size) {
        return make_error(ErrorCode::InvalidData,
                          std::format("BIOS deve ter exatamente {} bytes (512 KiB); recebido {} bytes",
                                      size, image.size()));
    }
    auto dst = rom_.bytes();
    std::copy(image.begin(), image.end(), dst.begin());
    loaded_ = true;
    return Status::success();
}

Status BiosRom::load_file(const std::filesystem::path& path) {
    // O limite recusa arquivos maiores que 512 KiB antes de lê-los, para não carregar
    // por engano uma imagem de disco inteira na memória.
    auto data = file_io::read_binary(path, size);
    if (!data) {
        return data.error();
    }
    auto status = load(data.value());
    if (!status) {
        return make_error(status.error().code,
                          unicode::path_to_utf8(path) + ": " + status.error().message);
    }
    log::info(log::Category::Bios, "BIOS carregada: {}", unicode::path_to_utf8(path));
    return Status::success();
}

void BiosRom::clear() noexcept {
    rom_.fill(0);
    loaded_ = false;
}

} // namespace rpcs1::memory
