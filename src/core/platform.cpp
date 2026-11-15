#include "core/platform.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "core/unicode.hpp"

namespace rpcs1::platform {

Result<std::filesystem::path> executable_directory() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return make_error(ErrorCode::PlatformError,
                              "GetModuleFileNameW falhou (código " +
                                  std::to_string(GetLastError()) + ")");
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        if (buffer.size() >= 32768) {
            return make_error(ErrorCode::PlatformError, "caminho do executável longo demais");
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
#else
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return make_error(ErrorCode::PlatformError, "não foi possível localizar o executável");
    }
    return path.parent_path();
#endif
}

void configure_console_utf8() noexcept {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::vector<std::string> command_line_arguments(int argc, char** argv) {
    std::vector<std::string> args;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide != nullptr) {
        for (int i = 1; i < count; ++i) {
            args.push_back(unicode::wide_to_utf8(wide[i]));
        }
        LocalFree(wide);
    }
#else
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
#endif
    return args;
}

} // namespace rpcs1::platform
