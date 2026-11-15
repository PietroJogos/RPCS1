#include "core/file_io.hpp"

#include <format>
#include <fstream>

#include "core/unicode.hpp"

namespace rpcs1::file_io {

namespace {

std::string quoted(const std::filesystem::path& path) {
    return "\"" + unicode::path_to_utf8(path) + "\"";
}

template <typename Container>
Result<Container> read_impl(const std::filesystem::path& path, usize max_size) {
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(status)) {
        return make_error(ErrorCode::NotFound, "arquivo não encontrado: " + quoted(path));
    }
    if (!std::filesystem::is_regular_file(status)) {
        return make_error(ErrorCode::InvalidArgument,
                          "o caminho não é um arquivo comum: " + quoted(path));
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return make_error(ErrorCode::IoError,
                          "não foi possível obter o tamanho de " + quoted(path) + ": " +
                              ec.message());
    }
    if (size > max_size) {
        return make_error(ErrorCode::OutOfRange,
                          std::format("arquivo grande demais ({} bytes; limite {} bytes): {}",
                                      size, max_size, quoted(path)));
    }

    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) {
        return make_error(ErrorCode::IoError, "não foi possível abrir " + quoted(path));
    }
    Container data(static_cast<usize>(size), typename Container::value_type{});
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (static_cast<u64>(stream.gcount()) != size) {
            return make_error(ErrorCode::IoError,
                              std::format("leitura incompleta de {} ({} de {} bytes)",
                                          quoted(path), stream.gcount(), size));
        }
    }
    return data;
}

Status write_impl(const std::filesystem::path& path, const char* data, usize size) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return make_error(ErrorCode::IoError, "não foi possível criar a pasta de " +
                                                      quoted(path) + ": " + ec.message());
        }
    }

    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream stream(temp, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream) {
            return make_error(ErrorCode::IoError,
                              "não foi possível criar o arquivo temporário " + quoted(temp));
        }
        stream.write(data, static_cast<std::streamsize>(size));
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temp, ec);
            return make_error(ErrorCode::IoError, "falha ao gravar " + quoted(temp));
        }
    }

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return make_error(ErrorCode::IoError, "não foi possível substituir " + quoted(path) +
                                                  ": " + ec.message());
    }
    return Status::success();
}

} // namespace

Result<std::vector<u8>> read_binary(const std::filesystem::path& path, usize max_size) {
    return read_impl<std::vector<u8>>(path, max_size);
}

Result<std::string> read_text(const std::filesystem::path& path, usize max_size) {
    return read_impl<std::string>(path, max_size);
}

Status write_atomic(const std::filesystem::path& path, std::span<const u8> data) {
    return write_impl(path, reinterpret_cast<const char*>(data.data()), data.size());
}

Status write_atomic(const std::filesystem::path& path, std::string_view text) {
    return write_impl(path, text.data(), text.size());
}

} // namespace rpcs1::file_io
