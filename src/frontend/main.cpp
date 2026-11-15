// Ponto de entrada do RPCS1 — emulador original de PlayStation 1.
// Desenvolvido pelo PiStudio BR.
#include <cstdio>
#include <exception>

#include "core/log.hpp"
#include "core/platform.hpp"
#include "frontend/application.hpp"
#include "frontend/crash_handler.hpp"

int main(int argc, char** argv) {
    rpcs1::platform::configure_console_utf8();
    rpcs1::frontend::install_crash_handlers();

    try {
        const auto args = rpcs1::platform::command_line_arguments(argc, argv);
        rpcs1::frontend::Application app;
        return static_cast<int>(app.run(args));
    } catch (const std::exception& e) {
        rpcs1::log::fatal(rpcs1::log::Category::Core, "Exceção não tratada: {}", e.what());
        rpcs1::log::Logger::instance().flush();
        std::fprintf(stderr, "RPCS1: erro fatal: %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "RPCS1: erro fatal desconhecido\n");
    }
    return static_cast<int>(rpcs1::frontend::ExitCode::RuntimeError);
}
