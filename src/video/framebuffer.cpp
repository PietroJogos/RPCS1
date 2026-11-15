#include "video/framebuffer.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace rpcs1::video {

Framebuffer::Framebuffer(u32 width, u32 height, u32 fill) {
    resize(width, height, fill);
}

void Framebuffer::resize(u32 width, u32 height, u32 fill) {
    RPCS1_ASSERT(width <= max_dimension && height <= max_dimension,
                 "dimensões de framebuffer fora do limite");
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<usize>(width) * height, fill);
}

void Framebuffer::clear(u32 color) noexcept {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

u32 Framebuffer::pixel(u32 x, u32 y) const noexcept {
    if (x >= width_ || y >= height_) {
        return 0;
    }
    return pixels_[static_cast<usize>(y) * width_ + x];
}

void Framebuffer::set_pixel(u32 x, u32 y, u32 color) noexcept {
    if (x >= width_ || y >= height_) {
        return;
    }
    pixels_[static_cast<usize>(y) * width_ + x] = color;
}

} // namespace rpcs1::video
