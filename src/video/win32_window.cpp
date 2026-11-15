// Implementação Win32 da janela de vídeo. Usa apenas a API nativa do Windows
// (user32/gdi32): GDI StretchDIBits apresenta o framebuffer em CPU.
#include "video/window.hpp"

#include <windows.h>

#include "core/log.hpp"
#include "core/unicode.hpp"

namespace rpcs1::video {

namespace {

constexpr wchar_t window_class_name[] = L"RPCS1.MainWindow";

std::string last_error_text(const char* function) {
    return std::string(function) + " falhou (código " + std::to_string(GetLastError()) + ")";
}

class Win32Window final : public Window {
public:
    static Result<std::unique_ptr<Window>> create(const WindowConfig& config);

    ~Win32Window() override { close(); }

    bool pump_events() override;
    void present(const Framebuffer& framebuffer) override;
    [[nodiscard]] bool is_open() const noexcept override { return hwnd_ != nullptr; }
    void close() override;

private:
    Win32Window() = default;

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void paint(HDC dc);

    HWND hwnd_ = nullptr;
    Framebuffer last_frame_; // mantido para redesenhar em WM_PAINT
};

Result<std::unique_ptr<Window>> Win32Window::create(const WindowConfig& config) {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Win32Window::window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = window_class_name;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return make_error(ErrorCode::PlatformError, last_error_text("RegisterClassExW"));
    }

    // O manifesto declara DPI "PerMonitorV2"; o tamanho lógico é convertido em pixels físicos.
    const UINT dpi = GetDpiForSystem();
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect{0, 0, MulDiv(static_cast<int>(config.client_width), static_cast<int>(dpi), 96),
              MulDiv(static_cast<int>(config.client_height), static_cast<int>(dpi), 96)};
    AdjustWindowRectExForDpi(&rect, style, FALSE, 0, dpi);

    std::unique_ptr<Win32Window> window(new Win32Window());
    const std::wstring title = unicode::utf8_to_wide(config.title);
    HWND hwnd = CreateWindowExW(0, window_class_name, title.c_str(), style, CW_USEDEFAULT,
                                CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, window.get());
    if (hwnd == nullptr) {
        return make_error(ErrorCode::PlatformError, last_error_text("CreateWindowExW"));
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    log::debug(log::Category::Video, "Janela Win32 criada ({}x{} lógicos, {} DPI)",
               config.client_width, config.client_height, dpi);
    return std::unique_ptr<Window>(std::move(window));
}

bool Win32Window::pump_events() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return hwnd_ != nullptr;
}

void Win32Window::present(const Framebuffer& framebuffer) {
    if (hwnd_ == nullptr) {
        return;
    }
    last_frame_ = framebuffer;
    HDC dc = GetDC(hwnd_);
    if (dc != nullptr) {
        paint(dc);
        ReleaseDC(hwnd_, dc);
    }
}

void Win32Window::close() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_); // WM_DESTROY zera hwnd_
        hwnd_ = nullptr;
    }
}

void Win32Window::paint(HDC dc) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const s32 cw = client.right - client.left;
    const s32 ch = client.bottom - client.top;
    auto* black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    const Rect dst = last_frame_.empty() ? Rect{}
                                         : fit_aspect(cw, ch, last_frame_.display_aspect);
    if (dst.width <= 0 || dst.height <= 0) {
        FillRect(dc, &client, black);
        return;
    }

    // Faixas pretas fora da área 4:3.
    const RECT bars[] = {
        {0, 0, cw, dst.y},
        {0, dst.y + dst.height, cw, ch},
        {0, dst.y, dst.x, dst.y + dst.height},
        {dst.x + dst.width, dst.y, cw, dst.y + dst.height},
    };
    for (const RECT& bar : bars) {
        if (bar.right > bar.left && bar.bottom > bar.top) {
            FillRect(dc, &bar, black);
        }
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(last_frame_.width());
    bmi.bmiHeader.biHeight = -static_cast<LONG>(last_frame_.height()); // de cima para baixo
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 0x00RRGGBB
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, dst.x, dst.y, dst.width, dst.height, 0, 0,
                  static_cast<int>(last_frame_.width()), static_cast<int>(last_frame_.height()),
                  last_frame_.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32Window::window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                          LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        auto* self = static_cast<Win32Window*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    return self->handle_message(message, wparam, lparam);
}

LRESULT Win32Window::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    HWND hwnd = hwnd_;
    switch (message) {
    case WM_CLOSE:
        log::info(log::Category::Frontend, "Janela fechada pelo usuário");
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        return 0;
    case WM_ERASEBKGND:
        return 1; // o fundo é pintado em paint(); evita cintilação
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace

Result<std::unique_ptr<Window>> create_window(const WindowConfig& config) {
    return Win32Window::create(config);
}

} // namespace rpcs1::video
