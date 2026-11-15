#include "video/bmp.hpp"

#include "core/file_io.hpp"

namespace rpcs1::video {

namespace {

constexpr u32 file_header_size = 14;
constexpr u32 info_header_size = 40;

void put16(std::vector<u8>& out, usize at, u32 value) {
    out[at] = static_cast<u8>(value);
    out[at + 1] = static_cast<u8>(value >> 8);
}

void put32(std::vector<u8>& out, usize at, u32 value) {
    for (usize k = 0; k < 4; ++k) {
        out[at + k] = static_cast<u8>(value >> (8 * k));
    }
}

} // namespace

std::vector<u8> encode_bmp(const Framebuffer& image) {
    const u32 width = image.width();
    const u32 height = image.height();
    const u32 stride = (width * 3 + 3) & ~3u;
    const u32 data_offset = file_header_size + info_header_size;
    const u32 data_size = stride * height;

    std::vector<u8> out(static_cast<usize>(data_offset) + data_size, 0);
    // BITMAPFILEHEADER
    out[0] = 'B';
    out[1] = 'M';
    put32(out, 2, data_offset + data_size);
    put32(out, 10, data_offset);
    // BITMAPINFOHEADER
    put32(out, 14, info_header_size);
    put32(out, 18, width);
    put32(out, 22, height); // positivo = linhas de baixo para cima
    put16(out, 26, 1);      // planos
    put16(out, 28, 24);     // bits por pixel
    put32(out, 30, 0);      // BI_RGB
    put32(out, 34, data_size);
    put32(out, 38, 2835);   // 72 DPI
    put32(out, 42, 2835);

    for (u32 y = 0; y < height; ++y) {
        usize at = data_offset + static_cast<usize>(height - 1 - y) * stride;
        for (u32 x = 0; x < width; ++x) {
            const u32 rgb = image.pixel(x, y);
            out[at++] = static_cast<u8>(rgb);       // B
            out[at++] = static_cast<u8>(rgb >> 8);  // G
            out[at++] = static_cast<u8>(rgb >> 16); // R
        }
    }
    return out;
}

Status save_bmp(const std::filesystem::path& path, const Framebuffer& image) {
    if (image.empty()) {
        return make_error(ErrorCode::InvalidArgument, "imagem vazia não pode ser gravada");
    }
    const std::vector<u8> bytes = encode_bmp(image);
    return file_io::write_atomic(path, bytes);
}

} // namespace rpcs1::video
