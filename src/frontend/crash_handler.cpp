#include "frontend/crash_handler.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/log.hpp"

namespace rpcs1::frontend {

namespace {

void on_terminate() {
    // Melhor esforço: o estado do processo pode estar comprometido.
    try {
        log::fatal(log::Category::Core, "std::terminate chamado (exceção não tratada)");
        log::Logger::instance().flush();
    } catch (...) {
    }
    std::fflush(stderr);
    std::abort();
}

#ifdef _WIN32
LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS* info) {
    const auto code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void* address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress
                                                        : nullptr;
    try {
        log::fatal(log::Category::Core, "Falha fatal: exceção 0x{:08X} no endereço {}",
                   static_cast<unsigned long>(code), address);
        log::Logger::instance().flush();
    } catch (...) {
    }
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void install_crash_handlers() {
    std::set_terminate(&on_terminate);
#ifdef _WIN32
    SetUnhandledExceptionFilter(&on_unhandled_exception);
#endif
}

} // namespace rpcs1::frontend
