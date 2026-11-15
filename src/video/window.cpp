#include "video/window.hpp"

#include <cmath>

namespace rpcs1::video {

Rect fit_aspect(s32 client_width, s32 client_height, double aspect) noexcept {
    if (client_width <= 0 || client_height <= 0 || !(aspect > 0.0)) {
        return {};
    }
    s32 width = client_width;
    s32 height = static_cast<s32>(std::lround(static_cast<double>(width) / aspect));
    if (height > client_height) {
        height = client_height;
        width = static_cast<s32>(std::lround(static_cast<double>(height) * aspect));
        if (width > client_width) {
            width = client_width;
        }
    }
    return Rect{(client_width - width) / 2, (client_height - height) / 2, width, height};
}

#ifndef _WIN32
Result<std::unique_ptr<Window>> create_window(const WindowConfig& /*config*/) {
    return make_error(ErrorCode::Unsupported,
                      "janela nativa ainda não implementada para esta plataforma");
}
#endif

} // namespace rpcs1::video
