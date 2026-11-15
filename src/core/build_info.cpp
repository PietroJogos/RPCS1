#include "core/build_info.hpp"

#include <format>

#include "core/version.hpp"

namespace rpcs1::build_info {

std::string compiler() {
#if defined(_MSC_VER) && !defined(__clang__)
    return std::format("MSVC {}.{:02}.{}", _MSC_FULL_VER / 10000000, (_MSC_FULL_VER / 100000) % 100,
                       _MSC_FULL_VER % 100000);
#elif defined(__clang__)
    return std::format("Clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    return std::format("GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    return "compilador desconhecido";
#endif
}

const char* configuration() noexcept {
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

const char* platform() noexcept {
#if defined(_WIN64)
    return "Windows x64";
#elif defined(_WIN32)
    return "Windows x86";
#elif defined(__linux__)
    return "Linux";
#else
    return "plataforma desconhecida";
#endif
}

std::string full_version_string() {
    return std::format("{} {} — {} — {}, {}, {}", version::product, version::string,
                       version::stage, compiler(), configuration(), platform());
}

} // namespace rpcs1::build_info
