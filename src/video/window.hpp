// Janela nativa do hospedeiro que exibe a saída de vídeo do console virtual.
#pragma once

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/types.hpp"
#include "video/framebuffer.hpp"

namespace rpcs1::video {

struct WindowConfig {
    std::string title = "RPCS1";
    u32 client_width = 640;  // em pixels lógicos (96 DPI)
    u32 client_height = 480;
};

struct Rect {
    s32 x = 0;
    s32 y = 0;
    s32 width = 0;
    s32 height = 0;
    bool operator==(const Rect&) const = default;
};

// Maior retângulo com a proporção `aspect` centralizado na área cliente
// (letterbox/pillarbox). Usado para manter a imagem em 4:3.
[[nodiscard]] Rect fit_aspect(s32 client_width, s32 client_height, double aspect) noexcept;

class Window {
public:
    virtual ~Window() = default;

    // Processa eventos pendentes do sistema. Retorna false quando a janela foi fechada.
    virtual bool pump_events() = 0;

    // Exibe o framebuffer escalado para a área cliente, mantendo a proporção.
    virtual void present(const Framebuffer& framebuffer) = 0;

    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    virtual void close() = 0;
};

// Cria a janela da plataforma atual (Win32 no Windows).
[[nodiscard]] Result<std::unique_ptr<Window>> create_window(const WindowConfig& config);

} // namespace rpcs1::video
